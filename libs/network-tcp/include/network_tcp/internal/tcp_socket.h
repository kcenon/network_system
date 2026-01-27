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

#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <system_error>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/internal/common_defs.h"

namespace kcenon::network::internal
{
	/*!
	 * \class tcp_socket
	 * \brief A lightweight wrapper around \c asio::ip::tcp::socket,
	 *        enabling asynchronous read and write operations.
	 *
	 * ### Key Features
	 * - Maintains a \c socket_ (from ASIO) for TCP communication.
	 * - Exposes \c set_receive_callback() to handle inbound data
	 *   and \c set_error_callback() for error handling.
	 * - \c start_read() begins an ongoing loop of \c async_read_some().
	 * - \c async_send() performs an \c async_write of a given data buffer.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe. Callback registration is protected
	 *   by callback_mutex_.
	 * - ASIO operations are serialized through the io_context, ensuring
	 *   read_buffer_ is only accessed by one async operation at a time.
	 * - The provided callbacks will be invoked on an ASIO worker thread;
	 *   ensure that your callback logic is thread-safe if it shares data.
	 */
	class tcp_socket : public std::enable_shared_from_this<tcp_socket>
	{
	public:
		/*!
		 * \brief Callback type for backpressure notifications.
		 * \param apply_backpressure True when backpressure should be applied
		 *        (high water mark reached), false when it can be released
		 *        (low water mark reached).
		 */
		using backpressure_callback = std::function<void(bool apply_backpressure)>;
		using send_handler_t = std::function<void(std::error_code, std::size_t)>;

		/*!
		 * \brief Constructs a \c tcp_socket by taking ownership of a moved \p
		 * socket.
		 * \param socket An \c asio::ip::tcp::socket that must be open/connected
		 * or at least valid.
		 *
		 * After construction, you can immediately call \c start_read() to begin
		 * receiving data. For sending, call \c async_send().
		 */
		tcp_socket(asio::ip::tcp::socket socket);

		/*!
		 * \brief Constructs a \c tcp_socket with custom configuration.
		 * \param socket An \c asio::ip::tcp::socket that must be open/connected.
		 * \param config Socket configuration including backpressure settings.
		 */
		tcp_socket(asio::ip::tcp::socket socket, const socket_config& config);

		/*!
		 * \brief Default destructor (no special cleanup needed).
		 */
		~tcp_socket() = default;

		/*!
		 * \brief Sets a callback to receive inbound data chunks.
		 * \param callback A function with signature \c void(const
		 * std::vector<uint8_t>&), called whenever a chunk of data is
		 * successfully read.
		 *
		 * If no callback is set, received data is effectively discarded.
		 *
		 * \note This is the legacy callback API. For better performance,
		 * consider using set_receive_callback_view() instead.
		 */
		auto set_receive_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets a zero-copy callback to receive inbound data as a view.
		 * \param callback A function with signature \c void(std::span<const
		 * uint8_t>), called whenever a chunk of data is successfully read.
		 *
		 * ### Zero-Copy Performance
		 * Unlike set_receive_callback(), this callback receives data as a
		 * non-owning view directly into the internal read buffer, avoiding
		 * per-read std::vector allocations and copies.
		 *
		 * ### Lifetime Contract
		 * - The span is valid **only** until the callback returns.
		 * - Callers must **not** store, capture, or use the span after
		 *   returning from the callback.
		 * - If data must be retained, copy it into your own container
		 *   within the callback.
		 *
		 * ### Dispatch Priority
		 * - If both view and vector callbacks are set, the view callback
		 *   takes priority and the vector callback is not invoked.
		 *
		 * ### Example
		 * \code
		 * sock->set_receive_callback_view([](std::span<const uint8_t> data) {
		 *     // Process data directly (zero-copy)
		 *     process_bytes(data.data(), data.size());
		 *     // If you need to keep the data:
		 *     // my_buffer.insert(my_buffer.end(), data.begin(), data.end());
		 * });
		 * \endcode
		 */
		auto set_receive_callback_view(
			std::function<void(std::span<const uint8_t>)> callback) -> void;

		/*!
		 * \brief Sets a callback to handle socket errors (e.g., read/write
		 * failures).
		 * \param callback A function with signature \c void(std::error_code),
		 *        invoked when any asynchronous operation fails.
		 *
		 * If no callback is set, errors are not explicitly handled here (beyond
		 * stopping reads).
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback)
			-> void;

		/*!
		 * \brief Begins the continuous asynchronous read loop.
		 *
		 * Once called, the class repeatedly calls \c async_read_some().
		 * If an error occurs, \c on_error() is triggered, stopping further
		 * reads.
		 */
		auto start_read() -> void;

		/*!
		 * \brief Initiates an asynchronous write of the given \p data buffer.
		 * \param data The buffer to send over TCP (moved for efficiency).
		 * \param handler A completion handler with signature \c
		 * void(std::error_code, std::size_t) that is invoked upon success or
		 * failure.
		 *
		 * The handler receives:
		 * - \c ec : the \c std::error_code from the write operation,
		 * - \c bytes_transferred : how many bytes were actually written.
		 *
		 * ### Example
		 * \code
		 * auto sock = std::make_shared<network::tcp_socket>(...);
		 * std::vector<uint8_t> buf = {0x01, 0x02, 0x03};
		 * sock->async_send(std::move(buf), [](std::error_code ec, std::size_t len) {
		 *     if(ec) {
		 *         // handle error
		 *     }
		 *     else {
		 *         // handle success
		 *     }
		 * });
		 * \endcode
		 *
		 * \note Data is moved (not copied) to avoid memory allocation.
		 * \note The original vector will be empty after this call.
		 */
        auto async_send(
            std::vector<uint8_t>&& data,
            send_handler_t handler) -> void;

		/*!
		 * \brief Sets a callback for backpressure notifications.
		 * \param callback Function invoked when backpressure state changes.
		 *
		 * The callback receives `true` when pending bytes exceed high_water_mark
		 * (apply backpressure), and `false` when they drop below low_water_mark
		 * (release backpressure).
		 */
		auto set_backpressure_callback(backpressure_callback callback) -> void;

		/*!
		 * \brief Attempts to send data without blocking.
		 * \param data The buffer to send (moved for efficiency).
		 * \param handler Completion handler for async operation.
		 * \return true if send was initiated, false if backpressure is active.
		 *
		 * Unlike async_send(), this method checks backpressure limits before
		 * initiating the send. Returns false immediately if:
		 * - max_pending_bytes is set and would be exceeded
		 * - Backpressure is currently active
		 *
		 * ### Example
		 * \code
		 * if (!sock->try_send(std::move(data), handler)) {
		 *     // Queue data for later or drop it
		 * }
		 * \endcode
		 */
		[[nodiscard]] auto try_send(
			std::vector<uint8_t>&& data,
			std::function<void(std::error_code, std::size_t)> handler) -> bool;

		/*!
		 * \brief Returns current pending bytes count.
		 * \return Number of bytes currently pending in send buffer.
		 */
		[[nodiscard]] auto pending_bytes() const -> std::size_t;

		/*!
		 * \brief Checks if backpressure is currently active.
		 * \return true if pending bytes exceed high_water_mark.
		 */
		[[nodiscard]] auto is_backpressure_active() const -> bool;

		/*!
		 * \brief Returns socket metrics for monitoring.
		 * \return Const reference to socket_metrics struct.
		 */
		[[nodiscard]] auto metrics() const -> const socket_metrics&;

		/*!
		 * \brief Resets socket metrics to zero.
		 */
		auto reset_metrics() -> void;

		/*!
		 * \brief Returns the current socket configuration.
		 * \return Const reference to socket_config struct.
		 */
		[[nodiscard]] auto config() const -> const socket_config&;

		/*!
		 * \brief Provides direct access to the underlying \c
		 * asio::ip::tcp::socket in case advanced operations are needed.
		 * \return A reference to the wrapped \c asio::ip::tcp::socket.
		 */
		auto socket() -> asio::ip::tcp::socket& { return socket_; }

		/*!
		 * \brief Stops the read loop to prevent further async operations
		 */
		auto stop_read() -> void;

		/*!
		 * \brief Safely closes the socket and stops all async operations.
		 *
		 * This method atomically sets the closed flag before closing the socket,
		 * preventing data races between the close operation and async read operations.
		 * Thread-safe with respect to concurrent async operations.
		 */
		auto close() -> void;

		/*!
		 * \brief Checks if the socket has been closed.
		 * \return true if close() has been called on this socket.
		 */
		[[nodiscard]] auto is_closed() const -> bool;

	private:
		/*!
		 * \brief Internal function to handle the read logic with \c
		 * async_read_some().
		 *
		 * Upon success, it calls \c receive_callback_ if set, then schedules
		 * another read. On error, it calls \c error_callback_ if available.
		 */
		auto do_read() -> void;

	private:
		/*! \brief Callback type aliases for lock-free storage */
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;
		using receive_callback_view_t = std::function<void(std::span<const uint8_t>)>;
		using error_callback_t = std::function<void(std::error_code)>;
		struct pending_write {
			std::shared_ptr<std::vector<uint8_t>> buffer;
			std::shared_ptr<send_handler_t> handler;
			std::size_t data_size{0};
		};

		asio::ip::tcp::socket socket_; /*!< The underlying ASIO TCP socket. */

		std::array<uint8_t, 4096>
			read_buffer_; /*!< Buffer for receiving data in \c do_read(). */

		/*!
		 * Lock-free callback storage using shared_ptr + atomic operations.
		 * This eliminates mutex contention on the receive hot path.
		 */
		std::shared_ptr<receive_callback_t> receive_callback_;
		std::shared_ptr<receive_callback_view_t> receive_callback_view_;
		std::shared_ptr<error_callback_t> error_callback_;

		std::mutex callback_mutex_; /*!< Protects callback registration only. */

		std::atomic<bool> is_reading_{false}; /*!< Flag to prevent read after stop. */
		std::atomic<bool> is_closed_{false};  /*!< Flag to indicate socket is closed. */

		/*! \brief Backpressure configuration */
		socket_config config_;

		/*! \brief Socket runtime metrics */
		mutable socket_metrics metrics_;

		/*! \brief Current pending bytes in send buffer */
		std::atomic<std::size_t> pending_bytes_{0};

		/*! \brief Backpressure state flag */
		std::atomic<bool> backpressure_active_{false};

		/*! \brief Backpressure notification callback */
		std::shared_ptr<backpressure_callback> backpressure_callback_;

		/*! \brief Pending write queue (serialized on the socket executor) */
		std::deque<pending_write> write_queue_;

		/*! \brief True when an async_write is in flight */
		bool write_in_progress_{false};

		/*! \brief Drain pending writes after close completes */
		bool drain_on_close_{false};

		/*! \brief Start or continue queued writes (executor thread only) */
		auto start_write() -> void;

		/*! \brief Finalize a send attempt and update metrics */
		auto finalize_send(std::error_code ec,
		                   std::size_t bytes_transferred,
		                   std::size_t data_size,
		                   const std::shared_ptr<send_handler_t>& handler_ptr) -> void;

		/*! \brief Fail all queued writes with an error (executor thread only) */
		auto drain_write_queue(std::error_code ec) -> void;
	};
} // namespace kcenon::network::internal
