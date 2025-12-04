/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "quic_socket.h"

#include <random>
#include <chrono>
#include <type_traits>

namespace network_system::internal
{

using namespace protocols::quic;

// =============================================================================
// Construction / Destruction
// =============================================================================

quic_socket::quic_socket(asio::ip::udp::socket socket, quic_role role)
	: udp_socket_(std::move(socket))
	, role_(role)
	, retransmit_timer_(udp_socket_.get_executor())
	, idle_timer_(udp_socket_.get_executor())
{
	// Generate local connection ID
	local_conn_id_ = generate_connection_id();

	// Initialize next stream ID based on role
	// Client-initiated bidi streams: 0, 4, 8, ...
	// Server-initiated bidi streams: 1, 5, 9, ...
	next_stream_id_ = (role_ == quic_role::client) ? 0 : 1;
}

quic_socket::~quic_socket()
{
	stop_receive();

	// Cancel timers
	retransmit_timer_.cancel();
	idle_timer_.cancel();
}

quic_socket::quic_socket(quic_socket&& other) noexcept
	: udp_socket_(std::move(other.udp_socket_))
	, remote_endpoint_(std::move(other.remote_endpoint_))
	, recv_buffer_(std::move(other.recv_buffer_))
	, role_(other.role_)
	, state_(other.state_.load())
	, crypto_(std::move(other.crypto_))
	, local_conn_id_(std::move(other.local_conn_id_))
	, remote_conn_id_(std::move(other.remote_conn_id_))
	, next_packet_number_(other.next_packet_number_)
	, largest_received_pn_(other.largest_received_pn_)
	, next_stream_id_(other.next_stream_id_)
	, pending_crypto_data_(std::move(other.pending_crypto_data_))
	, pending_stream_data_(std::move(other.pending_stream_data_))
	, stream_data_cb_(std::move(other.stream_data_cb_))
	, connected_cb_(std::move(other.connected_cb_))
	, error_cb_(std::move(other.error_cb_))
	, close_cb_(std::move(other.close_cb_))
	, is_receiving_(other.is_receiving_.load())
	, handshake_complete_(other.handshake_complete_.load())
	, retransmit_timer_(std::move(other.retransmit_timer_))
	, idle_timer_(std::move(other.idle_timer_))
{
}

quic_socket& quic_socket::operator=(quic_socket&& other) noexcept
{
	if (this != &other)
	{
		stop_receive();

		udp_socket_ = std::move(other.udp_socket_);
		remote_endpoint_ = std::move(other.remote_endpoint_);
		recv_buffer_ = std::move(other.recv_buffer_);
		role_ = other.role_;
		state_.store(other.state_.load());
		crypto_ = std::move(other.crypto_);
		local_conn_id_ = std::move(other.local_conn_id_);
		remote_conn_id_ = std::move(other.remote_conn_id_);
		next_packet_number_ = other.next_packet_number_;
		largest_received_pn_ = other.largest_received_pn_;
		next_stream_id_ = other.next_stream_id_;
		pending_crypto_data_ = std::move(other.pending_crypto_data_);
		pending_stream_data_ = std::move(other.pending_stream_data_);

		std::lock_guard<std::mutex> lock(callback_mutex_);
		stream_data_cb_ = std::move(other.stream_data_cb_);
		connected_cb_ = std::move(other.connected_cb_);
		error_cb_ = std::move(other.error_cb_);
		close_cb_ = std::move(other.close_cb_);

		is_receiving_.store(other.is_receiving_.load());
		handshake_complete_.store(other.handshake_complete_.load());
		retransmit_timer_ = std::move(other.retransmit_timer_);
		idle_timer_ = std::move(other.idle_timer_);
	}
	return *this;
}

// =============================================================================
// Callback Registration
// =============================================================================

auto quic_socket::set_stream_data_callback(stream_data_callback cb) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	stream_data_cb_ = std::move(cb);
}

auto quic_socket::set_connected_callback(connected_callback cb) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	connected_cb_ = std::move(cb);
}

auto quic_socket::set_error_callback(error_callback cb) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	error_cb_ = std::move(cb);
}

auto quic_socket::set_close_callback(close_callback cb) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	close_cb_ = std::move(cb);
}

// =============================================================================
// Connection Management
// =============================================================================

auto quic_socket::connect(const asio::ip::udp::endpoint& endpoint,
                          const std::string& server_name) -> VoidResult
{
	if (role_ != quic_role::client)
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"connect() can only be called on client sockets",
			"quic_socket");
	}

	if (state_.load() != quic_connection_state::idle)
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Connection already in progress or established",
			"quic_socket");
	}

	remote_endpoint_ = endpoint;

	// Initialize client-side crypto
	auto init_result = crypto_.init_client(
		server_name.empty() ? endpoint.address().to_string() : server_name);
	if (init_result.is_err())
	{
		return error_void(
			error_codes::network_system::connection_failed,
			"Failed to initialize TLS client",
			"quic_socket",
			init_result.error().message);
	}

	// Generate initial secrets from destination connection ID
	// For client, we use a random connection ID as the destination
	remote_conn_id_ = generate_connection_id();

	auto derive_result = crypto_.derive_initial_secrets(remote_conn_id_);
	if (derive_result.is_err())
	{
		return error_void(
			error_codes::network_system::connection_failed,
			"Failed to derive initial secrets",
			"quic_socket",
			derive_result.error().message);
	}

	transition_state(quic_connection_state::handshake_start);

	// Start handshake - generate ClientHello
	auto handshake_result = crypto_.start_handshake();
	if (handshake_result.is_err())
	{
		transition_state(quic_connection_state::closed);
		return error_void(
			error_codes::network_system::connection_failed,
			"Failed to start TLS handshake",
			"quic_socket",
			handshake_result.error().message);
	}

	// Queue the CRYPTO data for sending
	if (!handshake_result.value().empty())
	{
		queue_crypto_data(std::move(handshake_result.value()));
	}

	transition_state(quic_connection_state::handshake);

	// Start receiving
	start_receive();

	// Send initial packet
	send_pending_packets();

	return ok();
}

auto quic_socket::accept(const std::string& cert_file,
                         const std::string& key_file) -> VoidResult
{
	if (role_ != quic_role::server)
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"accept() can only be called on server sockets",
			"quic_socket");
	}

	if (state_.load() != quic_connection_state::idle)
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Connection already in progress",
			"quic_socket");
	}

	// Initialize server-side crypto
	auto init_result = crypto_.init_server(cert_file, key_file);
	if (init_result.is_err())
	{
		return error_void(
			error_codes::network_system::connection_failed,
			"Failed to initialize TLS server",
			"quic_socket",
			init_result.error().message);
	}

	transition_state(quic_connection_state::handshake_start);

	// Start receiving - server waits for Initial packet from client
	start_receive();

	return ok();
}

auto quic_socket::close(uint64_t error_code, const std::string& reason) -> VoidResult
{
	auto current_state = state_.load();
	if (current_state == quic_connection_state::closed ||
	    current_state == quic_connection_state::closing ||
	    current_state == quic_connection_state::draining)
	{
		return ok();  // Already closing/closed
	}

	transition_state(quic_connection_state::closing);

	// Build CONNECTION_CLOSE frame
	connection_close_frame close_frame;
	close_frame.error_code = error_code;
	close_frame.reason_phrase = reason;
	close_frame.is_application_error = (error_code != 0);

	// Send CONNECTION_CLOSE
	std::vector<frame> frames;
	frames.push_back(close_frame);

	auto level = handshake_complete_.load()
		? encryption_level::application
		: encryption_level::initial;

	auto send_result = send_packet(level, std::move(frames));
	if (send_result.is_err())
	{
		// Log error but continue with close
	}

	transition_state(quic_connection_state::draining);

	// Set a timer for draining period (3 * PTO), then fully close
	idle_timer_.expires_after(std::chrono::milliseconds(300));
	idle_timer_.async_wait(
		[self = shared_from_this()](const std::error_code& ec)
		{
			if (!ec)
			{
				self->transition_state(quic_connection_state::closed);
				self->stop_receive();
			}
		});

	return ok();
}

// =============================================================================
// I/O Operations
// =============================================================================

auto quic_socket::start_receive() -> void
{
	is_receiving_.store(true);
	do_receive();
}

auto quic_socket::stop_receive() -> void
{
	is_receiving_.store(false);
}

auto quic_socket::send_stream_data(uint64_t stream_id,
                                   std::vector<uint8_t>&& data,
                                   bool fin) -> VoidResult
{
	if (!is_connected())
	{
		return error_void(
			error_codes::network_system::connection_failed,
			"Connection not established",
			"quic_socket");
	}

	{
		std::lock_guard<std::mutex> lock(state_mutex_);
		pending_stream_data_[stream_id].push_back({std::move(data), fin});
	}

	send_pending_packets();

	return ok();
}

// =============================================================================
// Stream Management
// =============================================================================

auto quic_socket::create_stream(bool unidirectional) -> Result<uint64_t>
{
	if (!is_connected())
	{
		return error<uint64_t>(
			error_codes::network_system::connection_failed,
			"Connection not established",
			"quic_socket");
	}

	std::lock_guard<std::mutex> lock(state_mutex_);

	// Stream ID encoding (RFC 9000 Section 2.1):
	// - Bits 0-1: Type (0=client-initiated bidi, 1=server-initiated bidi,
	//                   2=client-initiated uni, 3=server-initiated uni)
	// - Bits 2+: Sequence number

	uint64_t type_bits = 0;
	if (role_ == quic_role::server)
	{
		type_bits |= 0x01;  // Server-initiated
	}
	if (unidirectional)
	{
		type_bits |= 0x02;  // Unidirectional
	}

	uint64_t stream_id = (next_stream_id_ << 2) | type_bits;
	next_stream_id_++;

	// Initialize the stream's pending data queue
	pending_stream_data_[stream_id] = {};

	return ok(std::move(stream_id));
}

auto quic_socket::close_stream(uint64_t stream_id) -> VoidResult
{
	std::lock_guard<std::mutex> lock(state_mutex_);

	auto it = pending_stream_data_.find(stream_id);
	if (it == pending_stream_data_.end())
	{
		return error_void(
			error_codes::common_errors::not_found,
			"Stream not found",
			"quic_socket");
	}

	// Send FIN on the stream
	it->second.push_back({{}, true});

	send_pending_packets();

	return ok();
}

// =============================================================================
// State Queries
// =============================================================================

auto quic_socket::is_connected() const noexcept -> bool
{
	return state_.load() == quic_connection_state::connected;
}

auto quic_socket::is_handshake_complete() const noexcept -> bool
{
	return handshake_complete_.load();
}

auto quic_socket::state() const noexcept -> quic_connection_state
{
	return state_.load();
}

auto quic_socket::role() const noexcept -> quic_role
{
	return role_;
}

auto quic_socket::remote_endpoint() const -> asio::ip::udp::endpoint
{
	return remote_endpoint_;
}

auto quic_socket::local_connection_id() const -> const connection_id&
{
	return local_conn_id_;
}

auto quic_socket::remote_connection_id() const -> const connection_id&
{
	return remote_conn_id_;
}

// =============================================================================
// Internal Methods
// =============================================================================

auto quic_socket::do_receive() -> void
{
	if (!is_receiving_.load())
	{
		return;
	}

	auto self = shared_from_this();
	udp_socket_.async_receive_from(
		asio::buffer(recv_buffer_),
		remote_endpoint_,
		[this, self](std::error_code ec, std::size_t bytes_transferred)
		{
			if (!is_receiving_.load())
			{
				return;
			}

			if (ec)
			{
				if (ec != asio::error::operation_aborted)
				{
					std::lock_guard<std::mutex> lock(callback_mutex_);
					if (error_cb_)
					{
						error_cb_(ec);
					}
				}
				return;
			}

			if (bytes_transferred > 0)
			{
				handle_packet(std::span(recv_buffer_.data(), bytes_transferred));
			}

			// Continue receiving
			if (is_receiving_.load())
			{
				do_receive();
			}
		});
}

auto quic_socket::handle_packet(std::span<const uint8_t> data) -> void
{
	// Parse packet header
	auto header_result = packet_parser::parse_header(data);
	if (header_result.is_err())
	{
		// Invalid packet - silently ignore
		return;
	}

	auto& [header, header_length] = header_result.value();

	// Determine encryption level
	auto level = determine_encryption_level(header);

	// For server, derive initial secrets on first Initial packet
	if (role_ == quic_role::server &&
	    state_.load() == quic_connection_state::handshake_start)
	{
		if (std::holds_alternative<long_header>(header))
		{
			const auto& lh = std::get<long_header>(header);
			if (lh.type() == packet_type::initial)
			{
				// Use client's DCID as our remote connection ID
				remote_conn_id_ = lh.src_conn_id;

				// Derive initial secrets from client's DCID
				auto derive_result = crypto_.derive_initial_secrets(lh.dest_conn_id);
				if (derive_result.is_err())
				{
					return;
				}

				transition_state(quic_connection_state::handshake);
			}
		}
	}

	// Get read keys for this level
	auto keys_result = crypto_.get_read_keys(level);
	if (keys_result.is_err())
	{
		// Keys not available yet - queue packet for later processing
		return;
	}

	// Unprotect (decrypt) the packet
	// First we need to get the packet number offset
	size_t pn_offset = header_length;
	if (std::holds_alternative<long_header>(header))
	{
		// For long headers, packet number is after the header
		// (already accounted for in header_length for Initial/Handshake)
	}

	// Determine packet number length from header (after removing header protection)
	size_t sample_offset = pn_offset + 4;  // Sample is 4 bytes after PN start
	if (sample_offset + hp_sample_size > data.size())
	{
		return;  // Not enough data for sample
	}

	std::span<const uint8_t> sample(data.data() + sample_offset, hp_sample_size);

	// Create mutable copy for header unprotection
	std::vector<uint8_t> packet_copy(data.begin(), data.end());

	auto unprotect_header_result = packet_protection::unprotect_header(
		keys_result.value(),
		std::span(packet_copy.data(), header_length + 4),  // Include space for max PN
		pn_offset,
		sample);

	if (unprotect_header_result.is_err())
	{
		return;
	}

	auto& [first_byte, pn_length] = unprotect_header_result.value();

	// Extract packet number
	uint64_t truncated_pn = 0;
	for (size_t i = 0; i < pn_length; ++i)
	{
		truncated_pn = (truncated_pn << 8) | packet_copy[pn_offset + i];
	}

	// Decode full packet number
	auto level_idx = static_cast<size_t>(level);
	uint64_t full_pn = packet_number::decode(
		truncated_pn, pn_length, largest_received_pn_[level_idx]);

	// Update largest received
	if (full_pn > largest_received_pn_[level_idx])
	{
		largest_received_pn_[level_idx] = full_pn;
	}

	// Decrypt packet payload
	size_t payload_offset = pn_offset + pn_length;
	auto unprotect_result = packet_protection::unprotect(
		keys_result.value(),
		std::span(packet_copy),
		payload_offset,
		full_pn);

	if (unprotect_result.is_err())
	{
		return;
	}

	auto& [unprotected_header, payload] = unprotect_result.value();

	// Parse frames from payload
	auto frames_result = frame_parser::parse_all(payload);
	if (frames_result.is_err())
	{
		return;
	}

	// Process each frame
	for (const auto& f : frames_result.value())
	{
		process_frame(f);
	}

	// Send any pending responses
	send_pending_packets();
}

auto quic_socket::process_frame(const frame& f) -> void
{
	std::visit([this](auto&& arg) {
		using T = std::decay_t<decltype(arg)>;

		if constexpr (std::is_same_v<T, crypto_frame>)
		{
			process_crypto_frame(arg);
		}
		else if constexpr (std::is_same_v<T, stream_frame>)
		{
			process_stream_frame(arg);
		}
		else if constexpr (std::is_same_v<T, ack_frame>)
		{
			process_ack_frame(arg);
		}
		else if constexpr (std::is_same_v<T, connection_close_frame>)
		{
			process_connection_close_frame(arg);
		}
		else if constexpr (std::is_same_v<T, handshake_done_frame>)
		{
			process_handshake_done_frame();
		}
		else if constexpr (std::is_same_v<T, ping_frame>)
		{
			// PING requires ACK - will be sent with next packet
		}
		else if constexpr (std::is_same_v<T, padding_frame>)
		{
			// PADDING is ignored
		}
		// Other frame types can be added as needed
	}, f);
}

auto quic_socket::process_crypto_frame(const crypto_frame& f) -> void
{
	auto level = crypto_.current_level();

	// Process the crypto data through TLS
	auto response_result = crypto_.process_crypto_data(level, f.data);
	if (response_result.is_err())
	{
		return;
	}

	// Queue any response crypto data
	if (!response_result.value().empty())
	{
		queue_crypto_data(std::move(response_result.value()));
	}

	// Check if handshake is now complete
	if (crypto_.is_handshake_complete() && !handshake_complete_.load())
	{
		handshake_complete_.store(true);

		// For client, transition to connected
		// For server, we send HANDSHAKE_DONE
		if (role_ == quic_role::client)
		{
			transition_state(quic_connection_state::connected);

			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (connected_cb_)
			{
				connected_cb_();
			}
		}
		else
		{
			// Server sends HANDSHAKE_DONE
			std::vector<frame> frames;
			frames.push_back(handshake_done_frame{});

			(void)send_packet(encryption_level::application, std::move(frames));

			transition_state(quic_connection_state::connected);

			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (connected_cb_)
			{
				connected_cb_();
			}
		}
	}
}

auto quic_socket::process_stream_frame(const stream_frame& f) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	if (stream_data_cb_)
	{
		stream_data_cb_(f.stream_id, f.data, f.fin);
	}
}

auto quic_socket::process_ack_frame(const ack_frame& f) -> void
{
	// Process ACK - remove acknowledged packets from retransmission queue
	// For now, just track the largest acknowledged
	(void)f;  // Placeholder for full implementation
}

auto quic_socket::process_connection_close_frame(const connection_close_frame& f) -> void
{
	transition_state(quic_connection_state::draining);

	std::lock_guard<std::mutex> lock(callback_mutex_);
	if (close_cb_)
	{
		close_cb_(f.error_code, f.reason_phrase);
	}

	// Enter draining period
	idle_timer_.expires_after(std::chrono::milliseconds(300));
	idle_timer_.async_wait(
		[self = shared_from_this()](const std::error_code& ec)
		{
			if (!ec)
			{
				self->transition_state(quic_connection_state::closed);
				self->stop_receive();
			}
		});
}

auto quic_socket::process_handshake_done_frame() -> void
{
	// Client receives HANDSHAKE_DONE
	if (role_ == quic_role::client && !handshake_complete_.load())
	{
		handshake_complete_.store(true);
		transition_state(quic_connection_state::connected);

		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (connected_cb_)
		{
			connected_cb_();
		}
	}
}

auto quic_socket::send_pending_packets() -> void
{
	auto current_state = state_.load();
	if (current_state == quic_connection_state::closed ||
	    current_state == quic_connection_state::idle)
	{
		return;
	}

	// Determine which encryption level to use
	encryption_level level = crypto_.current_level();

	std::vector<frame> frames;

	// Add pending CRYPTO data
	{
		std::lock_guard<std::mutex> lock(state_mutex_);
		auto level_idx = static_cast<size_t>(level);
		while (!pending_crypto_data_[level_idx].empty())
		{
			auto& data = pending_crypto_data_[level_idx].front();
			crypto_frame cf;
			cf.offset = 0;  // Simplified - full impl needs offset tracking
			cf.data = std::move(data);
			frames.push_back(std::move(cf));
			pending_crypto_data_[level_idx].pop_front();
		}
	}

	// Add pending STREAM data (only if connected)
	if (is_connected())
	{
		std::lock_guard<std::mutex> lock(state_mutex_);
		for (auto& [stream_id, queue] : pending_stream_data_)
		{
			while (!queue.empty())
			{
				auto& [data, fin] = queue.front();
				stream_frame sf;
				sf.stream_id = stream_id;
				sf.offset = 0;  // Simplified - full impl needs offset tracking
				sf.data = std::move(data);
				sf.fin = fin;
				frames.push_back(std::move(sf));
				queue.pop_front();
			}
		}
	}

	if (!frames.empty())
	{
		(void)send_packet(level, std::move(frames));
	}
}

auto quic_socket::send_packet(encryption_level level,
                              std::vector<frame>&& frames) -> VoidResult
{
	// Get write keys
	auto keys_result = crypto_.get_write_keys(level);
	if (keys_result.is_err())
	{
		return error_void(
			error_codes::common_errors::not_initialized,
			"Write keys not available",
			"quic_socket");
	}

	// Build frames payload
	std::vector<uint8_t> payload;
	for (const auto& f : frames)
	{
		auto frame_bytes = frame_builder::build(f);
		payload.insert(payload.end(), frame_bytes.begin(), frame_bytes.end());
	}

	// Get next packet number
	auto level_idx = static_cast<size_t>(level);
	uint64_t pn = next_packet_number_[level_idx]++;

	// Build header
	std::vector<uint8_t> header;
	if (level == encryption_level::initial)
	{
		header = packet_builder::build_initial(
			remote_conn_id_, local_conn_id_, {}, pn);
	}
	else if (level == encryption_level::handshake)
	{
		header = packet_builder::build_handshake(
			remote_conn_id_, local_conn_id_, pn);
	}
	else
	{
		header = packet_builder::build_short(
			remote_conn_id_, pn, crypto_.key_phase());
	}

	// Protect (encrypt) the packet
	auto protect_result = packet_protection::protect(
		keys_result.value(), header, payload, pn);

	if (protect_result.is_err())
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Failed to protect packet",
			"quic_socket",
			protect_result.error().message);
	}

	// Send the packet
	auto& protected_packet = protect_result.value();

	auto self = shared_from_this();
	auto buffer = std::make_shared<std::vector<uint8_t>>(std::move(protected_packet));

	udp_socket_.async_send_to(
		asio::buffer(*buffer),
		remote_endpoint_,
		[self, buffer](std::error_code ec, std::size_t /*bytes_sent*/)
		{
			if (ec && ec != asio::error::operation_aborted)
			{
				std::lock_guard<std::mutex> lock(self->callback_mutex_);
				if (self->error_cb_)
				{
					self->error_cb_(ec);
				}
			}
		});

	return ok();
}

auto quic_socket::queue_crypto_data(std::vector<uint8_t>&& data) -> void
{
	auto level = crypto_.current_level();
	auto level_idx = static_cast<size_t>(level);

	std::lock_guard<std::mutex> lock(state_mutex_);
	pending_crypto_data_[level_idx].push_back(std::move(data));
}

auto quic_socket::determine_encryption_level(const packet_header& header) const noexcept
	-> encryption_level
{
	if (std::holds_alternative<long_header>(header))
	{
		const auto& lh = std::get<long_header>(header);
		switch (lh.type())
		{
			case packet_type::initial:
				return encryption_level::initial;
			case packet_type::zero_rtt:
				return encryption_level::zero_rtt;
			case packet_type::handshake:
				return encryption_level::handshake;
			default:
				return encryption_level::initial;
		}
	}
	else
	{
		// Short header = 1-RTT = application level
		return encryption_level::application;
	}
}

auto quic_socket::generate_connection_id() -> connection_id
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint8_t> dis(0, 255);

	std::array<uint8_t, 8> id_bytes;
	for (auto& byte : id_bytes)
	{
		byte = dis(gen);
	}

	return connection_id(std::span<const uint8_t>(id_bytes));
}

auto quic_socket::on_retransmit_timeout() -> void
{
	// Retransmission logic would go here
	// For now, just resend pending packets
	send_pending_packets();
}

auto quic_socket::transition_state(quic_connection_state new_state) -> void
{
	state_.store(new_state);
}

} // namespace network_system::internal
