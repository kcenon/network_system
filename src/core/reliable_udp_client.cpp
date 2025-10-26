/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "network_system/core/reliable_udp_client.h"
#include "network_system/core/messaging_udp_client.h"
#include "network_system/integration/logger_integration.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

namespace network_system::core
{
	// Packet header structure (12 bytes)
	struct packet_header
	{
		uint32_t sequence_number; // Packet sequence number
		uint32_t ack_number;      // Acknowledgment number
		uint16_t flags;           // Control flags
		uint16_t data_length;     // Payload length

		static constexpr uint16_t FLAG_ACK = 0x01;  // Acknowledgment packet
		static constexpr uint16_t FLAG_DATA = 0x02; // Data packet
		static constexpr uint16_t FLAG_SYN = 0x04;  // Connection start
		static constexpr uint16_t FLAG_FIN = 0x08;  // Connection end

		auto serialize() const -> std::vector<uint8_t>
		{
			std::vector<uint8_t> buffer(sizeof(packet_header));
			std::memcpy(buffer.data(), this, sizeof(packet_header));
			return buffer;
		}

		static auto deserialize(const std::vector<uint8_t>& buffer) -> packet_header
		{
			packet_header header{};
			if (buffer.size() >= sizeof(packet_header))
			{
				std::memcpy(&header, buffer.data(), sizeof(packet_header));
			}
			return header;
		}
	};

	// Packet info for retransmission tracking
	struct packet_info
	{
		std::vector<uint8_t> data;
		std::chrono::steady_clock::time_point send_time;
		size_t retransmit_count{0};
		uint32_t sequence_number;
	};

	class reliable_udp_client::impl
	{
	public:
		impl(std::string_view client_id, reliability_mode mode)
			: client_id_(client_id), mode_(mode), next_sequence_(1), expected_sequence_(1)
		{
			NETWORK_LOG_DEBUG("[reliable_udp_client::" + client_id_ +
							  "] Created with mode=" + std::to_string(static_cast<int>(mode)));
		}

		~impl() noexcept
		{
			if (is_running_.load())
			{
				stop_client();
			}
		}

		auto start_client(std::string_view host, uint16_t port) -> VoidResult
		{
			std::lock_guard<std::mutex> lock(state_mutex_);

			if (is_running_.load())
			{
				return error_void(error_codes::common::already_exists,
								  "Client is already running");
			}

			// Create underlying UDP client
			udp_client_ = std::make_shared<messaging_udp_client>(client_id_);

			// Set receive callback
			udp_client_->set_receive_callback(
				[this, self = shared_from_this()](const std::vector<uint8_t>& data,
												  const asio::ip::udp::endpoint&) {
					handle_received_packet(data);
				});

			// Set error callback
			udp_client_->set_error_callback([this, self = shared_from_this()](
												std::error_code ec) { handle_error(ec); });

			// Start UDP client
			auto result = udp_client_->start_client(host, port);
			if (result.is_err())
			{
				return result;
			}

			is_running_.store(true);

			// Start retransmission timer for reliable modes
			if (mode_ != reliability_mode::unreliable)
			{
				start_retransmission_timer();
			}

			NETWORK_LOG_INFO("[reliable_udp_client::" + client_id_ + "] Started successfully");
			return ok();
		}

		auto stop_client() -> VoidResult
		{
			std::lock_guard<std::mutex> lock(state_mutex_);

			if (!is_running_.load())
			{
				return ok();
			}

			is_running_.store(false);

			// Stop retransmission timer
			stop_retransmission_timer();

			// Stop underlying UDP client
			if (udp_client_)
			{
				udp_client_->stop_client();
				udp_client_.reset();
			}

			// Clear pending packets
			{
				std::lock_guard<std::mutex> pending_lock(pending_mutex_);
				pending_packets_.clear();
			}

			// Clear receive buffer
			{
				std::lock_guard<std::mutex> recv_lock(receive_mutex_);
				receive_buffer_.clear();
			}

			NETWORK_LOG_INFO("[reliable_udp_client::" + client_id_ + "] Stopped");
			return ok();
		}

		auto send_packet(std::vector<uint8_t>&& data) -> VoidResult
		{
			if (!is_running_.load())
			{
				return error_void(error_codes::common::internal_error,
								  "Client is not running");
			}

			switch (mode_)
			{
			case reliability_mode::unreliable:
				return send_unreliable(std::move(data));

			case reliability_mode::reliable_ordered:
			case reliability_mode::reliable_unordered:
				return send_reliable(std::move(data));

			case reliability_mode::sequenced:
				return send_sequenced(std::move(data));
			}

			return error_void(error_codes::common::internal_error, "Invalid reliability mode");
		}

		auto wait_for_stop() -> void
		{
			if (udp_client_)
			{
				udp_client_->wait_for_stop();
			}
		}

		auto set_receive_callback(std::function<void(const std::vector<uint8_t>&)> callback)
			-> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			receive_callback_ = std::move(callback);
		}

		auto set_error_callback(std::function<void(std::error_code)> callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			error_callback_ = std::move(callback);
		}

		auto set_congestion_window(size_t packets) -> void { congestion_window_ = packets; }

		auto set_max_retries(size_t retries) -> void { max_retries_ = retries; }

		auto set_retransmission_timeout(uint32_t timeout_ms) -> void
		{
			retransmission_timeout_ms_ = timeout_ms;
		}

		auto get_stats() const -> reliable_udp_stats
		{
			std::lock_guard<std::mutex> lock(stats_mutex_);
			return stats_;
		}

		auto is_running() const -> bool { return is_running_.load(); }

		auto client_id() const -> const std::string& { return client_id_; }

		auto mode() const -> reliability_mode { return mode_; }

		auto shared_from_this() -> std::shared_ptr<impl>
		{
			// This is called from impl, not from reliable_udp_client
			// We need to be careful about the shared_ptr lifetime
			return std::shared_ptr<impl>(this, [](impl*) {
				// No-op deleter since we don't own the impl
			});
		}

	private:
		// Unreliable send (pure UDP)
		auto send_unreliable(std::vector<uint8_t>&& data) -> VoidResult
		{
			packet_header header{};
			header.sequence_number = 0;
			header.ack_number = 0;
			header.flags = packet_header::FLAG_DATA;
			header.data_length = static_cast<uint16_t>(data.size());

			auto packet = create_packet(header, data);

			return udp_client_->send_packet(
				std::move(packet), [this](std::error_code ec, std::size_t) {
					if (!ec)
					{
						std::lock_guard<std::mutex> lock(stats_mutex_);
						stats_.packets_sent++;
					}
				});
		}

		// Reliable send (with ACK and retransmission)
		auto send_reliable(std::vector<uint8_t>&& data) -> VoidResult
		{
			std::lock_guard<std::mutex> lock(pending_mutex_);

			// Check congestion window
			if (pending_packets_.size() >= congestion_window_)
			{
				return error_void(error_codes::common::internal_error,
								  "Congestion window full");
			}

			uint32_t seq = next_sequence_++;

			packet_header header{};
			header.sequence_number = seq;
			header.ack_number = 0;
			header.flags = packet_header::FLAG_DATA;
			header.data_length = static_cast<uint16_t>(data.size());

			auto packet = create_packet(header, data);

			// Store for retransmission
			packet_info info{};
			info.data = packet;
			info.send_time = std::chrono::steady_clock::now();
			info.retransmit_count = 0;
			info.sequence_number = seq;
			pending_packets_[seq] = std::move(info);

			// Send packet
			return udp_client_->send_packet(
				std::move(packet), [this, seq](std::error_code ec, std::size_t) {
					if (!ec)
					{
						std::lock_guard<std::mutex> lock(stats_mutex_);
						stats_.packets_sent++;
					}
					else
					{
						// Remove from pending if send failed
						std::lock_guard<std::mutex> pending_lock(pending_mutex_);
						pending_packets_.erase(seq);
					}
				});
		}

		// Sequenced send (drop old packets)
		auto send_sequenced(std::vector<uint8_t>&& data) -> VoidResult
		{
			uint32_t seq = next_sequence_++;

			packet_header header{};
			header.sequence_number = seq;
			header.ack_number = 0;
			header.flags = packet_header::FLAG_DATA;
			header.data_length = static_cast<uint16_t>(data.size());

			auto packet = create_packet(header, data);

			return udp_client_->send_packet(
				std::move(packet), [this](std::error_code ec, std::size_t) {
					if (!ec)
					{
						std::lock_guard<std::mutex> lock(stats_mutex_);
						stats_.packets_sent++;
					}
				});
		}

		// Create packet with header + payload
		auto create_packet(const packet_header& header, const std::vector<uint8_t>& payload)
			-> std::vector<uint8_t>
		{
			std::vector<uint8_t> packet(sizeof(packet_header) + payload.size());
			std::memcpy(packet.data(), &header, sizeof(packet_header));
			if (!payload.empty())
			{
				std::memcpy(packet.data() + sizeof(packet_header), payload.data(),
							payload.size());
			}
			return packet;
		}

		// Handle received packet
		auto handle_received_packet(const std::vector<uint8_t>& data) -> void
		{
			if (data.size() < sizeof(packet_header))
			{
				NETWORK_LOG_WARN("[reliable_udp_client::" + client_id_ +
								 "] Received invalid packet (too small)");
				return;
			}

			packet_header header{};
			std::memcpy(&header, data.data(), sizeof(packet_header));

			// Handle ACK packet
			if (header.flags & packet_header::FLAG_ACK)
			{
				handle_ack(header.ack_number);
				return;
			}

			// Handle data packet
			if (header.flags & packet_header::FLAG_DATA)
			{
				// Send ACK for reliable modes
				if (mode_ == reliability_mode::reliable_ordered ||
					mode_ == reliability_mode::reliable_unordered)
				{
					send_ack(header.sequence_number);
				}

				// Extract payload
				std::vector<uint8_t> payload(data.begin() + sizeof(packet_header), data.end());

				// Process based on mode
				switch (mode_)
				{
				case reliability_mode::unreliable:
					deliver_to_application(std::move(payload));
					break;

				case reliability_mode::reliable_ordered:
					handle_ordered_delivery(header.sequence_number, std::move(payload));
					break;

				case reliability_mode::reliable_unordered:
					deliver_to_application(std::move(payload));
					break;

				case reliability_mode::sequenced:
					handle_sequenced_delivery(header.sequence_number, std::move(payload));
					break;
				}

				{
					std::lock_guard<std::mutex> lock(stats_mutex_);
					stats_.packets_received++;
				}
			}
		}

		// Handle ACK
		auto handle_ack(uint32_t ack_number) -> void
		{
			std::lock_guard<std::mutex> lock(pending_mutex_);

			auto it = pending_packets_.find(ack_number);
			if (it != pending_packets_.end())
			{
				// Calculate RTT
				auto now = std::chrono::steady_clock::now();
				auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
							   now - it->second.send_time)
							   .count();

				// Update stats
				{
					std::lock_guard<std::mutex> stats_lock(stats_mutex_);
					stats_.acks_received++;

					// Update average RTT (exponential moving average)
					if (stats_.average_rtt_ms == 0.0)
					{
						stats_.average_rtt_ms = static_cast<double>(rtt);
					}
					else
					{
						stats_.average_rtt_ms = 0.875 * stats_.average_rtt_ms + 0.125 * rtt;
					}
				}

				// Remove from pending
				pending_packets_.erase(it);

				NETWORK_LOG_TRACE("[reliable_udp_client::" + client_id_ + "] Received ACK for seq=" +
								  std::to_string(ack_number) + ", RTT=" + std::to_string(rtt) + "ms");
			}
		}

		// Send ACK
		auto send_ack(uint32_t sequence_number) -> void
		{
			packet_header header{};
			header.sequence_number = 0;
			header.ack_number = sequence_number;
			header.flags = packet_header::FLAG_ACK;
			header.data_length = 0;

			auto packet = create_packet(header, {});

			udp_client_->send_packet(std::move(packet), [this](std::error_code ec, std::size_t) {
				if (!ec)
				{
					std::lock_guard<std::mutex> lock(stats_mutex_);
					stats_.acks_sent++;
				}
			});
		}

		// Handle ordered delivery
		auto handle_ordered_delivery(uint32_t sequence_number, std::vector<uint8_t>&& payload)
			-> void
		{
			std::lock_guard<std::mutex> lock(receive_mutex_);

			if (sequence_number == expected_sequence_)
			{
				// Deliver in-order packet
				deliver_to_application(std::move(payload));
				expected_sequence_++;

				// Deliver any buffered packets that are now in order
				while (true)
				{
					auto it = receive_buffer_.find(expected_sequence_);
					if (it == receive_buffer_.end())
					{
						break;
					}

					deliver_to_application(std::move(it->second));
					receive_buffer_.erase(it);
					expected_sequence_++;
				}
			}
			else if (sequence_number > expected_sequence_)
			{
				// Buffer out-of-order packet
				receive_buffer_[sequence_number] = std::move(payload);

				NETWORK_LOG_TRACE("[reliable_udp_client::" + client_id_ +
								  "] Buffered out-of-order packet seq=" +
								  std::to_string(sequence_number) +
								  " (expected=" + std::to_string(expected_sequence_) + ")");
			}
			else
			{
				// Duplicate or old packet, ignore
				NETWORK_LOG_TRACE("[reliable_udp_client::" + client_id_ +
								  "] Dropped old packet seq=" + std::to_string(sequence_number));
			}
		}

		// Handle sequenced delivery
		auto handle_sequenced_delivery(uint32_t sequence_number, std::vector<uint8_t>&& payload)
			-> void
		{
			if (sequence_number >= expected_sequence_)
			{
				expected_sequence_ = sequence_number + 1;
				deliver_to_application(std::move(payload));
			}
			else
			{
				// Drop old packet
				std::lock_guard<std::mutex> lock(stats_mutex_);
				stats_.packets_dropped++;

				NETWORK_LOG_TRACE("[reliable_udp_client::" + client_id_ +
								  "] Dropped old packet in sequenced mode seq=" +
								  std::to_string(sequence_number));
			}
		}

		// Deliver packet to application
		auto deliver_to_application(std::vector<uint8_t>&& payload) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (receive_callback_)
			{
				receive_callback_(payload);
			}
		}

		// Handle error
		auto handle_error(std::error_code ec) -> void
		{
			NETWORK_LOG_ERROR("[reliable_udp_client::" + client_id_ + "] Error: " + ec.message());

			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (error_callback_)
			{
				error_callback_(ec);
			}
		}

		// Start retransmission timer
		auto start_retransmission_timer() -> void
		{
			retransmission_thread_ = std::thread([this]() {
				while (is_running_.load())
				{
					std::this_thread::sleep_for(
						std::chrono::milliseconds(retransmission_timeout_ms_));
					check_and_retransmit();
				}
			});
		}

		// Stop retransmission timer
		auto stop_retransmission_timer() -> void
		{
			if (retransmission_thread_.joinable())
			{
				retransmission_thread_.join();
			}
		}

		// Check for packets needing retransmission
		auto check_and_retransmit() -> void
		{
			std::lock_guard<std::mutex> lock(pending_mutex_);

			auto now = std::chrono::steady_clock::now();

			for (auto it = pending_packets_.begin(); it != pending_packets_.end();)
			{
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
								   now - it->second.send_time)
								   .count();

				if (elapsed >= retransmission_timeout_ms_)
				{
					// Check max retries
					if (it->second.retransmit_count >= max_retries_)
					{
						NETWORK_LOG_WARN("[reliable_udp_client::" + client_id_ +
										 "] Packet seq=" + std::to_string(it->first) +
										 " exceeded max retries, dropping");

						{
							std::lock_guard<std::mutex> stats_lock(stats_mutex_);
							stats_.packets_dropped++;
						}

						it = pending_packets_.erase(it);
						continue;
					}

					// Retransmit
					auto packet_copy = it->second.data;
					it->second.send_time = now;
					it->second.retransmit_count++;

					{
						std::lock_guard<std::mutex> stats_lock(stats_mutex_);
						stats_.packets_retransmitted++;
					}

					NETWORK_LOG_TRACE("[reliable_udp_client::" + client_id_ +
									  "] Retransmitting seq=" + std::to_string(it->first) +
									  " (attempt " + std::to_string(it->second.retransmit_count) +
									  ")");

					udp_client_->send_packet(std::move(packet_copy),
											 [](std::error_code, std::size_t) {});
				}

				++it;
			}
		}

	private:
		std::string client_id_;
		reliability_mode mode_;

		std::atomic<bool> is_running_{false};
		std::shared_ptr<messaging_udp_client> udp_client_;

		// Sequence numbers
		std::atomic<uint32_t> next_sequence_{1};
		std::atomic<uint32_t> expected_sequence_{1};

		// Pending packets (waiting for ACK)
		mutable std::mutex pending_mutex_;
		std::map<uint32_t, packet_info> pending_packets_;

		// Receive buffer (for ordered delivery)
		mutable std::mutex receive_mutex_;
		std::map<uint32_t, std::vector<uint8_t>> receive_buffer_;

		// Callbacks
		mutable std::mutex callback_mutex_;
		std::function<void(const std::vector<uint8_t>&)> receive_callback_;
		std::function<void(std::error_code)> error_callback_;

		// Configuration
		size_t congestion_window_{32};
		size_t max_retries_{5};
		uint32_t retransmission_timeout_ms_{200};

		// Statistics
		mutable std::mutex stats_mutex_;
		reliable_udp_stats stats_;

		// Retransmission timer
		std::thread retransmission_thread_;

		// State mutex
		std::mutex state_mutex_;
	};

	// reliable_udp_client implementation

	reliable_udp_client::reliable_udp_client(std::string_view client_id, reliability_mode mode)
		: pimpl_(std::make_unique<impl>(client_id, mode))
	{
	}

	reliable_udp_client::~reliable_udp_client() noexcept = default;

	auto reliable_udp_client::start_client(std::string_view host, uint16_t port) -> VoidResult
	{
		return pimpl_->start_client(host, port);
	}

	auto reliable_udp_client::stop_client() -> VoidResult { return pimpl_->stop_client(); }

	auto reliable_udp_client::send_packet(std::vector<uint8_t>&& data) -> VoidResult
	{
		return pimpl_->send_packet(std::move(data));
	}

	auto reliable_udp_client::wait_for_stop() -> void { pimpl_->wait_for_stop(); }

	auto reliable_udp_client::set_receive_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		pimpl_->set_receive_callback(std::move(callback));
	}

	auto reliable_udp_client::set_error_callback(std::function<void(std::error_code)> callback)
		-> void
	{
		pimpl_->set_error_callback(std::move(callback));
	}

	auto reliable_udp_client::set_congestion_window(size_t packets) -> void
	{
		pimpl_->set_congestion_window(packets);
	}

	auto reliable_udp_client::set_max_retries(size_t retries) -> void
	{
		pimpl_->set_max_retries(retries);
	}

	auto reliable_udp_client::set_retransmission_timeout(uint32_t timeout_ms) -> void
	{
		pimpl_->set_retransmission_timeout(timeout_ms);
	}

	auto reliable_udp_client::get_stats() const -> reliable_udp_stats
	{
		return pimpl_->get_stats();
	}

	auto reliable_udp_client::is_running() const -> bool { return pimpl_->is_running(); }

	auto reliable_udp_client::client_id() const -> const std::string&
	{
		return pimpl_->client_id();
	}

	auto reliable_udp_client::mode() const -> reliability_mode { return pimpl_->mode(); }

} // namespace network_system::core
