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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string_view>

// Use nested namespace definition (C++17)
namespace kcenon::network::internal
{
	/*!
	 * \struct socket_config
	 * \brief Configuration for TCP socket backpressure control.
	 *
	 * ### Backpressure Overview
	 * Backpressure prevents memory exhaustion when sending to slow receivers.
	 * When pending bytes exceed high_water_mark, the backpressure callback
	 * is invoked. When bytes drop below low_water_mark, sending can resume.
	 *
	 * ### Default Behavior
	 * With max_pending_bytes=0, backpressure is disabled (unlimited buffering).
	 */
	struct socket_config
	{
		/*!
		 * \brief Maximum bytes allowed in pending send buffer.
		 *
		 * When this limit is reached, try_send() returns false and
		 * new sends are rejected until buffer drains.
		 * Set to 0 for unlimited (default, backward compatible).
		 */
		std::size_t max_pending_bytes{0};

		/*!
		 * \brief High water mark - trigger backpressure callback.
		 *
		 * When pending bytes reach this threshold, the backpressure
		 * callback is invoked with `true` to signal the sender to slow down.
		 * Default: 1MB
		 */
		std::size_t high_water_mark{1024 * 1024};

		/*!
		 * \brief Low water mark - resume sending.
		 *
		 * When pending bytes drop to this threshold after being above
		 * high_water_mark, the backpressure callback is invoked with
		 * `false` to signal that sending can resume.
		 * Default: 256KB
		 */
		std::size_t low_water_mark{256 * 1024};
	};

	/*!
	 * \struct socket_metrics
	 * \brief Runtime metrics for socket monitoring.
	 *
	 * All counters are atomic for thread-safe access.
	 * These metrics help diagnose performance issues and tune backpressure.
	 */
	struct socket_metrics
	{
		std::atomic<std::size_t> total_bytes_sent{0};
		std::atomic<std::size_t> total_bytes_received{0};
		std::atomic<std::size_t> current_pending_bytes{0};
		std::atomic<std::size_t> peak_pending_bytes{0};
		std::atomic<std::size_t> backpressure_events{0};
		std::atomic<std::size_t> rejected_sends{0};
		std::atomic<std::size_t> send_count{0};
		std::atomic<std::size_t> receive_count{0};

		void reset()
		{
			total_bytes_sent.store(0);
			total_bytes_received.store(0);
			current_pending_bytes.store(0);
			peak_pending_bytes.store(0);
			backpressure_events.store(0);
			rejected_sends.store(0);
			send_count.store(0);
			receive_count.store(0);
		}
	};
	/*!
	 * \enum data_mode
	 * \brief Represents a simple enumeration for differentiating data
	 * transmission modes.
	 *
	 * A higher-level code might use these to switch between packet-based,
	 * file-based, or binary data logic. They are optional stubs and can be
	 * extended as needed.
	 */
	enum class data_mode : std::uint8_t {
		packet_mode = 1, /*!< Regular messaging/packet mode. */
		file_mode = 2,	 /*!< File transfer mode. */
		binary_mode = 3	 /*!< Raw binary data mode. */
	};

	// Use inline variables for constants (C++17)
	inline constexpr std::size_t default_buffer_size = 4096;
	inline constexpr std::size_t default_timeout_ms = 5000;
	inline constexpr std::string_view default_client_id = "default_client";
	inline constexpr std::string_view default_server_id = "default_server";

} // namespace kcenon::network::internal