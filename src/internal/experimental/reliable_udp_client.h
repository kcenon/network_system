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

#pragma once

// Experimental API marker - users must opt-in to use this header
#include "internal/experimental/experimental_api.h"
NETWORK_REQUIRE_EXPERIMENTAL

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::core
{
	/*!
	 * \enum reliability_mode
	 * \brief Defines the reliability level for UDP packet transmission.
	 */
	enum class reliability_mode
	{
		unreliable,         /*!< Pure UDP - no guarantees (lowest latency) */
		reliable_ordered,   /*!< TCP-like reliability with in-order delivery */
		reliable_unordered, /*!< Guaranteed delivery without ordering */
		sequenced           /*!< Drop old packets, no retransmission (for real-time) */
	};

	/*!
	 * \struct reliable_udp_stats
	 * \brief Statistics for monitoring reliable UDP connection performance.
	 */
	struct reliable_udp_stats
	{
		uint64_t packets_sent{0};          /*!< Total packets sent */
		uint64_t packets_received{0};      /*!< Total packets received */
		uint64_t packets_retransmitted{0}; /*!< Packets that required retransmission */
		uint64_t packets_dropped{0};       /*!< Packets dropped (sequenced mode) */
		uint64_t acks_sent{0};             /*!< Total ACKs sent */
		uint64_t acks_received{0};         /*!< Total ACKs received */
		double average_rtt_ms{0.0};        /*!< Average round-trip time in milliseconds */
	};

	/*!
	 * \class reliable_udp_client
	 * \brief A UDP client with optional reliability layer for configurable delivery guarantees.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe
	 * - Internal state is protected by mutexes
	 * - Callbacks may be invoked from internal threads
	 *
	 * ### Key Features
	 * - **Selective Acknowledgment (SACK)**: Efficient ACK mechanism
	 * - **Packet Retransmission**: Automatic retry for lost packets
	 * - **In-order Delivery**: Optional sequence guarantee
	 * - **Congestion Control**: Sliding window flow control
	 * - **Flexible Modes**: Choose reliability vs performance trade-off
	 *
	 * ### Reliability Modes
	 * 1. **unreliable**: Pure UDP, no overhead (use for non-critical data)
	 * 2. **reliable_ordered**: Like TCP - all packets arrive in order
	 * 3. **reliable_unordered**: All packets arrive, order doesn't matter
	 * 4. **sequenced**: Latest packets only, old ones dropped (real-time)
	 *
	 * ### Usage Example
	 * \code
	 * // Create reliable ordered UDP client
	 * auto client = std::make_shared<reliable_udp_client>(
	 *     "GameClient",
	 *     reliability_mode::reliable_ordered
	 * );
	 *
	 * // Set receive callback
	 * client->set_receive_callback([](const std::vector<uint8_t>& data) {
	 *     std::cout << "Received " << data.size() << " bytes\n";
	 * });
	 *
	 * // Start client
	 * auto result = client->start_client("game-server.example.com", 7777);
	 * if (!result) {
	 *     std::cerr << "Failed to start: " << result.error().message << "\n";
	 *     return -1;
	 * }
	 *
	 * // Send data with automatic reliability handling
	 * std::vector<uint8_t> game_state = {...};
	 * client->send_packet(std::move(game_state));
	 *
	 * // Get statistics
	 * auto stats = client->get_stats();
	 * std::cout << "RTT: " << stats.average_rtt_ms << " ms\n";
	 * \endcode
	 */
	class reliable_udp_client : public std::enable_shared_from_this<reliable_udp_client>
	{
	public:
		/*!
		 * \brief Constructs a reliable UDP client with specified mode.
		 * \param client_id Unique identifier for this client instance.
		 * \param mode Reliability mode (default: reliable_ordered).
		 */
		reliable_udp_client(
			std::string_view client_id,
			reliability_mode mode = reliability_mode::reliable_ordered);

		/*!
		 * \brief Destructor. Automatically stops the client if running.
		 */
		~reliable_udp_client() noexcept;

		/*!
		 * \brief Starts the client and connects to the target endpoint.
		 * \param host Target hostname or IP address.
		 * \param port Target port number.
		 * \return Result<void> - Success or error with details.
		 *
		 * Creates underlying UDP socket, starts receive loop, and initializes
		 * reliability mechanisms based on the configured mode.
		 */
		auto start_client(std::string_view host, uint16_t port) -> VoidResult;

		/*!
		 * \brief Stops the client and releases resources.
		 * \return Result<void> - Success or error with details.
		 *
		 * Flushes pending packets, stops timers, and closes socket.
		 */
		auto stop_client() -> VoidResult;

		/*!
		 * \brief Sends a packet with reliability handling based on mode.
		 * \param data Packet data to send (moved for efficiency).
		 * \return Result<void> - Success if packet queued/sent, error otherwise.
		 *
		 * Behavior varies by reliability mode:
		 * - unreliable: Immediate send, no tracking
		 * - reliable_ordered: Queued, retransmitted until ACK, delivered in order
		 * - reliable_unordered: Retransmitted until ACK, no ordering
		 * - sequenced: Immediate send with sequence number, old packets dropped
		 */
		auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

		/*!
		 * \brief Blocks until the client is stopped.
		 */
		auto wait_for_stop() -> void;

		/*!
		 * \brief Sets callback for received data.
		 * \param callback Function called when data is received and processed.
		 *
		 * In reliable_ordered mode, data is delivered in order.
		 * In other modes, data is delivered as it arrives.
		 */
		auto set_receive_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets callback for connection errors.
		 * \param callback Function called when errors occur.
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback) -> void;

		/*!
		 * \brief Sets the congestion window size (maximum unacknowledged packets).
		 * \param packets Number of packets (default: 32).
		 *
		 * Larger window = higher throughput but more memory usage.
		 * Smaller window = lower throughput but better for constrained networks.
		 */
		auto set_congestion_window(size_t packets) -> void;

		/*!
		 * \brief Sets maximum retransmission attempts before giving up.
		 * \param retries Number of retries (default: 5).
		 *
		 * Only applies to reliable modes. After max retries, packet is dropped
		 * and error callback is invoked.
		 */
		auto set_max_retries(size_t retries) -> void;

		/*!
		 * \brief Sets retransmission timeout in milliseconds.
		 * \param timeout_ms Timeout value (default: 200ms).
		 *
		 * Dynamically adjusted based on measured RTT.
		 */
		auto set_retransmission_timeout(uint32_t timeout_ms) -> void;

		/*!
		 * \brief Returns current connection statistics.
		 * \return Statistics structure with counters and metrics.
		 */
		auto get_stats() const -> reliable_udp_stats;

		/*!
		 * \brief Checks if client is currently running.
		 * \return true if running, false otherwise.
		 */
		auto is_running() const -> bool;

		/*!
		 * \brief Returns the client identifier.
		 * \return Client ID string.
		 */
		auto client_id() const -> const std::string&;

		/*!
		 * \brief Returns the current reliability mode.
		 * \return Reliability mode.
		 */
		auto mode() const -> reliability_mode;

	private:
		class impl;
		std::unique_ptr<impl> pimpl_; /*!< Implementation (PIMPL idiom) */
	};

} // namespace kcenon::network::core
