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
 * @file socket_concepts.h
 * @brief C++20 concepts for unified socket abstraction.
 *
 * This header provides concepts for compile-time validation of socket types,
 * enabling generic algorithms to work with different socket implementations
 * (tcp_socket, secure_tcp_socket, udp_socket, websocket_socket).
 *
 * Concept hierarchy:
 * - Socket: Base concept for all socket types (close, is_closed)
 * - StreamSocket: Connected sockets with async send/receive (tcp, secure_tcp)
 * - DatagramSocket: Connectionless sockets with send_to (udp)
 * - MessageSocket: Message-oriented sockets (websocket)
 *
 * Requirements:
 * - C++20 compiler with concepts support
 * - GCC 10+, Clang 10+, MSVC 2022+
 *
 * @see tcp_socket, secure_tcp_socket, udp_socket, websocket_socket
 */

#include <concepts>
#include <cstdint>
#include <functional>
#include <span>
#include <system_error>
#include <type_traits>
#include <vector>

namespace kcenon::network::concepts {

// ============================================================================
// Type traits for socket handler signatures
// ============================================================================

/**
 * @brief Standard completion handler signature for async operations.
 */
template <typename F>
concept AsyncCompletionHandler =
    std::invocable<F, std::error_code, std::size_t>;

/**
 * @brief Error-only completion handler signature.
 */
template <typename F>
concept ErrorCompletionHandler = std::invocable<F, std::error_code>;

// ============================================================================
// Base Socket Concept
// ============================================================================

/**
 * @concept Socket
 * @brief Base concept for all socket types.
 *
 * All socket implementations must provide:
 * - close(): Safely close the socket and stop async operations
 * - is_closed(): Check if the socket has been closed
 *
 * Example usage:
 * @code
 * template<Socket S>
 * void shutdown_socket(S& socket) {
 *     if (!socket.is_closed()) {
 *         socket.close();
 *     }
 * }
 * @endcode
 */
template <typename T>
concept Socket = requires(T& socket) {
    { socket.close() } -> std::same_as<void>;
    { socket.is_closed() } -> std::convertible_to<bool>;
};

// ============================================================================
// Stream Socket Concept (TCP, Secure TCP)
// ============================================================================

/**
 * @concept StreamSocket
 * @brief Concept for connected stream sockets (TCP, TLS).
 *
 * Stream sockets provide bidirectional byte stream communication.
 * They support:
 * - Asynchronous send with completion handler
 * - Continuous read loop via start_read/stop_read
 * - Receive and error callback registration
 *
 * Types satisfying this concept: tcp_socket, secure_tcp_socket
 *
 * Example usage:
 * @code
 * template<StreamSocket S>
 * void send_message(S& socket, std::vector<uint8_t> data) {
 *     socket.async_send(std::move(data), [](std::error_code ec, size_t n) {
 *         if (ec) {
 *             // handle error
 *         }
 *     });
 * }
 * @endcode
 */
template <typename T>
concept StreamSocket = Socket<T> && requires(
    T& socket, std::vector<uint8_t> data,
    std::function<void(std::error_code, std::size_t)> send_handler,
    std::function<void(const std::vector<uint8_t>&)> recv_callback,
    std::function<void(std::span<const uint8_t>)> recv_view_callback,
    std::function<void(std::error_code)> error_callback) {
    // Async send operation
    { socket.async_send(std::move(data), send_handler) } -> std::same_as<void>;

    // Read loop control
    { socket.start_read() } -> std::same_as<void>;
    { socket.stop_read() } -> std::same_as<void>;

    // Callback registration
    { socket.set_receive_callback(recv_callback) } -> std::same_as<void>;
    { socket.set_receive_callback_view(recv_view_callback) } -> std::same_as<void>;
    { socket.set_error_callback(error_callback) } -> std::same_as<void>;
};

// ============================================================================
// Datagram Socket Concept (UDP)
// ============================================================================

/**
 * @concept DatagramSocket
 * @brief Concept for connectionless datagram sockets (UDP).
 *
 * Datagram sockets provide message-oriented, connectionless communication.
 * Each send operation targets a specific endpoint.
 *
 * Types satisfying this concept: udp_socket
 *
 * Example usage:
 * @code
 * template<DatagramSocket S, typename Endpoint>
 * void send_datagram(S& socket, std::vector<uint8_t> data, const Endpoint& dest) {
 *     socket.async_send_to(std::move(data), dest, [](std::error_code ec, size_t n) {
 *         if (ec) {
 *             // handle error
 *         }
 *     });
 * }
 * @endcode
 *
 * @note The endpoint type is intentionally not constrained to allow
 *       flexibility with different address family implementations.
 */
template <typename T>
concept DatagramSocket = Socket<T> && requires(T& socket) {
    // Receive loop control
    { socket.start_receive() } -> std::same_as<void>;
    { socket.stop_receive() } -> std::same_as<void>;

    // Error callback registration
    {
        socket.set_error_callback(std::function<void(std::error_code)>{})
    } -> std::same_as<void>;
};

/**
 * @concept DatagramSocketWithEndpoint
 * @brief Datagram socket with specific endpoint type.
 *
 * This concept validates that a datagram socket can send to a specific
 * endpoint type.
 *
 * Example usage:
 * @code
 * template<DatagramSocketWithEndpoint<asio::ip::udp::endpoint> S>
 * void send_udp(S& socket, std::vector<uint8_t> data,
 *               const asio::ip::udp::endpoint& dest) {
 *     socket.async_send_to(std::move(data), dest, handler);
 * }
 * @endcode
 */
template <typename T, typename Endpoint>
concept DatagramSocketWithEndpoint =
    DatagramSocket<T> && requires(
        T& socket, std::vector<uint8_t> data, const Endpoint& endpoint,
        std::function<void(std::error_code, std::size_t)> handler) {
        { socket.async_send_to(std::move(data), endpoint, handler) } -> std::same_as<void>;
    };

// ============================================================================
// Message Socket Concept (WebSocket)
// ============================================================================

/**
 * @concept MessageSocket
 * @brief Concept for message-oriented sockets (WebSocket).
 *
 * Message sockets provide framed, message-based communication.
 * They distinguish between text and binary messages.
 *
 * Types satisfying this concept: websocket_socket
 *
 * Example usage:
 * @code
 * template<MessageSocket S>
 * void send_json(S& socket, const std::string& json) {
 *     socket.async_send_text(std::string{json}, [](std::error_code ec, size_t n) {
 *         if (ec) {
 *             // handle error
 *         }
 *     });
 * }
 * @endcode
 */
template <typename T>
concept MessageSocket = requires(
    T& socket, std::string text, std::vector<uint8_t> binary,
    std::function<void(std::error_code, std::size_t)> handler) {
    // Connection state
    { socket.is_open() } -> std::convertible_to<bool>;

    // Read control
    { socket.start_read() } -> std::same_as<void>;

    // Error callback
    {
        socket.set_error_callback(std::function<void(std::error_code)>{})
    } -> std::same_as<void>;
};

// ============================================================================
// Backpressure-Aware Socket Concept
// ============================================================================

/**
 * @concept BackpressureAwareSocket
 * @brief Concept for sockets with backpressure control.
 *
 * Sockets satisfying this concept provide flow control mechanisms
 * to prevent memory exhaustion when sending to slow receivers.
 *
 * Types satisfying this concept: tcp_socket (with backpressure enabled)
 *
 * Example usage:
 * @code
 * template<BackpressureAwareSocket S>
 * void send_with_flow_control(S& socket, std::vector<uint8_t> data) {
 *     if (socket.is_backpressure_active()) {
 *         // Queue or drop data
 *         return;
 *     }
 *     if (!socket.try_send(std::move(data), handler)) {
 *         // Backpressure limit reached
 *     }
 * }
 * @endcode
 */
template <typename T>
concept BackpressureAwareSocket = StreamSocket<T> && requires(
    T& socket, std::vector<uint8_t> data,
    std::function<void(std::error_code, std::size_t)> handler,
    std::function<void(bool)> bp_callback) {
    // Backpressure state
    { socket.pending_bytes() } -> std::convertible_to<std::size_t>;
    { socket.is_backpressure_active() } -> std::convertible_to<bool>;

    // Non-blocking send
    { socket.try_send(std::move(data), handler) } -> std::convertible_to<bool>;

    // Backpressure notification
    { socket.set_backpressure_callback(bp_callback) } -> std::same_as<void>;
};

// ============================================================================
// Socket Metrics Concept
// ============================================================================

/**
 * @concept MetricsAwareSocket
 * @brief Concept for sockets with runtime metrics.
 *
 * Sockets satisfying this concept provide monitoring capabilities
 * for performance analysis and debugging.
 *
 * Example usage:
 * @code
 * template<MetricsAwareSocket S>
 * void log_stats(const S& socket) {
 *     const auto& m = socket.metrics();
 *     std::cout << "Bytes sent: " << m.total_bytes_sent << std::endl;
 * }
 * @endcode
 */
template <typename T>
concept MetricsAwareSocket = requires(T& socket, const T& const_socket) {
    { const_socket.metrics() };
    { socket.reset_metrics() } -> std::same_as<void>;
};

// ============================================================================
// Secure Socket Concept
// ============================================================================

/**
 * @concept SecureSocket
 * @brief Concept for TLS/SSL-enabled sockets.
 *
 * Secure sockets require a handshake before data transmission.
 *
 * Types satisfying this concept: secure_tcp_socket
 *
 * Example usage:
 * @code
 * template<SecureSocket S, typename HandshakeType>
 * void establish_secure_connection(S& socket, HandshakeType type) {
 *     socket.async_handshake(type, [](std::error_code ec) {
 *         if (ec) {
 *             // Handshake failed
 *         }
 *     });
 * }
 * @endcode
 */
template <typename T>
concept SecureSocket = StreamSocket<T> && requires(T& socket) {
    // Note: The actual async_handshake requires SSL-specific types,
    // so we only check for stream socket capabilities here.
    // Type-specific handshake validation should be done at usage site.
    requires true;
};

// ============================================================================
// Generic Socket Utilities
// ============================================================================

/**
 * @brief RAII guard for socket read loop.
 *
 * Ensures stop_read() is called when the guard goes out of scope.
 *
 * Example usage:
 * @code
 * template<StreamSocket S>
 * void process_socket(S& socket) {
 *     socket_read_guard guard{socket};
 *     socket.start_read();
 *     // ... socket will stop reading when guard is destroyed
 * }
 * @endcode
 */
template <StreamSocket S>
class socket_read_guard
{
public:
    explicit socket_read_guard(S& socket) : socket_(socket) {}
    ~socket_read_guard() { socket_.stop_read(); }

    socket_read_guard(const socket_read_guard&) = delete;
    socket_read_guard& operator=(const socket_read_guard&) = delete;

private:
    S& socket_;
};

/**
 * @brief RAII guard for datagram socket receive loop.
 *
 * Ensures stop_receive() is called when the guard goes out of scope.
 */
template <DatagramSocket S>
class datagram_receive_guard
{
public:
    explicit datagram_receive_guard(S& socket) : socket_(socket) {}
    ~datagram_receive_guard() { socket_.stop_receive(); }

    datagram_receive_guard(const datagram_receive_guard&) = delete;
    datagram_receive_guard& operator=(const datagram_receive_guard&) = delete;

private:
    S& socket_;
};

} // namespace kcenon::network::concepts
