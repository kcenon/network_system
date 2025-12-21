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

#pragma once

/**
 * @file network_concepts.h
 * @brief C++20 concepts for network_system types and callbacks.
 *
 * This header provides concepts for compile-time validation of network-related
 * types, improving error messages and code documentation.
 *
 * Requirements:
 * - C++20 compiler with concepts support
 * - GCC 10+, Clang 10+, MSVC 2022+
 *
 * @see common_system concepts for Result/Optional and callable concepts
 */

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

// Note: common_system concepts are included via the parent concepts.h header
// to avoid forward declaration conflicts. Do not include concepts.h here directly.

namespace kcenon::network::concepts {

// ============================================================================
// Data Buffer Concepts
// ============================================================================

/**
 * @concept ByteBuffer
 * @brief A type that can serve as a network data buffer.
 *
 * Types satisfying this concept can store and provide access to byte data
 * for network transmission.
 *
 * Example usage:
 * @code
 * template<ByteBuffer Buffer>
 * void send_data(Buffer&& buffer) {
 *     // buffer.data() returns pointer to bytes
 *     // buffer.size() returns number of bytes
 * }
 * @endcode
 */
template <typename T>
concept ByteBuffer = requires(const T t) {
    { t.data() } -> std::convertible_to<const void*>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

/**
 * @concept MutableByteBuffer
 * @brief A mutable byte buffer that can be resized.
 *
 * Example usage:
 * @code
 * template<MutableByteBuffer Buffer>
 * void receive_data(Buffer& buffer, size_t expected_size) {
 *     buffer.resize(expected_size);
 *     // ... fill buffer with received data
 * }
 * @endcode
 */
template <typename T>
concept MutableByteBuffer = ByteBuffer<T> && requires(T t, std::size_t n) {
    { t.resize(n) };
    { t.data() } -> std::convertible_to<void*>;
};

// ============================================================================
// Callback Concepts
// ============================================================================

/**
 * @concept DataReceiveHandler
 * @brief A callback type for handling received data.
 *
 * Functions satisfying this concept can be used as receive callbacks
 * for network data.
 *
 * Example usage:
 * @code
 * template<DataReceiveHandler Handler>
 * void set_receive_handler(Handler&& handler) {
 *     // handler will be called with const std::vector<uint8_t>&
 * }
 * @endcode
 */
template <typename F>
concept DataReceiveHandler =
    std::invocable<F, const std::vector<uint8_t>&>;

/**
 * @concept ErrorHandler
 * @brief A callback type for handling network errors.
 *
 * Example usage:
 * @code
 * template<ErrorHandler Handler>
 * void set_error_handler(Handler&& handler) {
 *     // handler will be called with std::error_code
 * }
 * @endcode
 */
template <typename F>
concept ErrorHandler = std::invocable<F, std::error_code>;

/**
 * @concept ConnectionHandler
 * @brief A callback type for handling connection state changes.
 *
 * Example usage:
 * @code
 * template<ConnectionHandler Handler>
 * void set_connected_handler(Handler&& handler) {
 *     // handler will be called with no arguments
 * }
 * @endcode
 */
template <typename F>
concept ConnectionHandler = std::invocable<F>;

/**
 * @concept SessionHandler
 * @brief A callback type for handling session events with a session pointer.
 *
 * Example usage:
 * @code
 * template<typename Session, SessionHandler<Session> Handler>
 * void set_session_handler(Handler&& handler) {
 *     // handler will be called with std::shared_ptr<Session>
 * }
 * @endcode
 */
template <typename F, typename Session>
concept SessionHandler = std::invocable<F, std::shared_ptr<Session>>;

/**
 * @concept SessionDataHandler
 * @brief A callback type for handling data received on a specific session.
 *
 * Example usage:
 * @code
 * template<typename Session, SessionDataHandler<Session> Handler>
 * void set_receive_handler(Handler&& handler) {
 *     // handler receives session and data
 * }
 * @endcode
 */
template <typename F, typename Session>
concept SessionDataHandler =
    std::invocable<F, std::shared_ptr<Session>, const std::vector<uint8_t>&>;

/**
 * @concept SessionErrorHandler
 * @brief A callback type for handling errors on a specific session.
 *
 * Example usage:
 * @code
 * template<typename Session, SessionErrorHandler<Session> Handler>
 * void set_error_handler(Handler&& handler) {
 *     // handler receives session and error code
 * }
 * @endcode
 */
template <typename F, typename Session>
concept SessionErrorHandler =
    std::invocable<F, std::shared_ptr<Session>, std::error_code>;

/**
 * @concept DisconnectionHandler
 * @brief A callback type for handling disconnection events with session ID.
 *
 * Example usage:
 * @code
 * template<DisconnectionHandler Handler>
 * void set_disconnect_handler(Handler&& handler) {
 *     // handler receives session ID string
 * }
 * @endcode
 */
template <typename F>
concept DisconnectionHandler = std::invocable<F, const std::string&>;

/**
 * @concept RetryCallback
 * @brief A callback type for reconnection attempt notifications.
 *
 * Example usage:
 * @code
 * template<RetryCallback Handler>
 * void set_retry_handler(Handler&& handler) {
 *     // handler receives attempt number
 * }
 * @endcode
 */
template <typename F>
concept RetryCallback = std::invocable<F, std::size_t>;

// ============================================================================
// Network Component Concepts
// ============================================================================

/**
 * @concept NetworkClient
 * @brief A type that satisfies basic network client requirements.
 *
 * Types satisfying this concept can connect to servers, send data,
 * and check connection status.
 *
 * Example usage:
 * @code
 * template<NetworkClient Client>
 * void use_client(Client& client) {
 *     if (client.is_connected()) {
 *         std::vector<uint8_t> data = {1, 2, 3};
 *         client.send_packet(std::move(data));
 *     }
 * }
 * @endcode
 */
template <typename T>
concept NetworkClient = requires(T t, std::vector<uint8_t> data) {
    { t.is_connected() } -> std::convertible_to<bool>;
    { t.send_packet(std::move(data)) };
    { t.stop_client() };
};

/**
 * @concept NetworkServer
 * @brief A type that satisfies basic network server requirements.
 *
 * Types satisfying this concept can start/stop listening and
 * report their running status.
 *
 * Example usage:
 * @code
 * template<NetworkServer Server>
 * void manage_server(Server& server, unsigned short port) {
 *     server.start_server(port);
 *     // ... do work
 *     server.stop_server();
 * }
 * @endcode
 */
template <typename T>
concept NetworkServer = requires(T t, unsigned short port) {
    { t.start_server(port) };
    { t.stop_server() };
};

/**
 * @concept NetworkSession
 * @brief A type that represents a network session.
 *
 * Example usage:
 * @code
 * template<NetworkSession Session>
 * void handle_session(std::shared_ptr<Session> session) {
 *     auto id = session->get_session_id();
 *     session->start_session();
 * }
 * @endcode
 */
template <typename T>
concept NetworkSession = requires(T t) {
    { t.get_session_id() } -> std::convertible_to<std::string>;
    { t.start_session() };
    { t.stop_session() };
};

// ============================================================================
// Pipeline Concepts
// ============================================================================

/**
 * @concept DataTransformer
 * @brief A type that can transform data (e.g., compression, encryption).
 *
 * Types satisfying this concept can process data for transmission
 * or reception.
 *
 * Example usage:
 * @code
 * template<DataTransformer Transformer>
 * auto apply_transform(Transformer& t, std::vector<uint8_t>& data) {
 *     return t.transform(data);
 * }
 * @endcode
 */
template <typename T>
concept DataTransformer = requires(T t, std::vector<uint8_t>& data) {
    { t.transform(data) } -> std::convertible_to<bool>;
};

/**
 * @concept ReversibleDataTransformer
 * @brief A transformer that supports both forward and reverse operations.
 *
 * Example usage:
 * @code
 * template<ReversibleDataTransformer Transformer>
 * void process_bidirectional(Transformer& t, std::vector<uint8_t>& data) {
 *     t.transform(data);      // e.g., compress
 *     t.reverse_transform(data);  // e.g., decompress
 * }
 * @endcode
 */
template <typename T>
concept ReversibleDataTransformer =
    DataTransformer<T> && requires(T t, std::vector<uint8_t>& data) {
        { t.reverse_transform(data) } -> std::convertible_to<bool>;
    };

// ============================================================================
// Duration Concepts
// ============================================================================

/**
 * @concept Duration
 * @brief A type that represents a time duration.
 *
 * Example usage:
 * @code
 * template<Duration D>
 * void set_timeout(D duration) {
 *     auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
 *     // use ms.count()
 * }
 * @endcode
 */
template <typename T>
concept Duration = requires {
    typename T::rep;
    typename T::period;
} && std::is_arithmetic_v<typename T::rep>;

// ============================================================================
// Integration with common_system concepts (when available)
// ============================================================================
// Note: Additional common_system integration concepts are defined in the
// parent concepts.h header after proper include order is established.

} // namespace kcenon::network::concepts
