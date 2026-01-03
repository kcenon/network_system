// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file udp.cppm
 * @brief UDP partition for network_system module.
 *
 * This partition provides UDP networking functionality including
 * unreliable datagram messaging and reliable UDP implementations.
 *
 * Contents:
 * - messaging_udp_client: UDP client for datagram messaging
 * - messaging_udp_server: UDP server for handling multiple endpoints
 * - reliable_udp_client: Reliable UDP with acknowledgment and retransmission
 *
 * @see kcenon.network for the primary module interface
 * @see kcenon.network:core for base classes and interfaces
 */

module;

// =============================================================================
// Global Module Fragment - Standard Library Headers
// =============================================================================
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Third-party headers
#include <asio.hpp>

export module kcenon.network:udp;

import :core;

// =============================================================================
// UDP Client
// =============================================================================

export namespace kcenon::network::core {

/**
 * @class messaging_udp_client
 * @brief A UDP client for datagram messaging
 *
 * This class provides a UDP client that sends and receives datagrams
 * to/from a remote endpoint. Unlike TCP, UDP is connectionless and
 * does not guarantee delivery order or reliability.
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 * - Socket access is protected by internal mutex.
 *
 * ### Key Features
 * - Connectionless datagram messaging
 * - Low latency for real-time applications
 * - Multicast support
 * - Optional reliable delivery layer
 *
 * @code
 * import kcenon.network;
 *
 * auto client = std::make_unique<kcenon::network::core::messaging_udp_client>("udp_client");
 * client->set_data_callback([](std::span<const uint8_t> data,
 *                               const std::string& from_addr,
 *                               uint16_t from_port) {
 *     std::cout << "Received from " << from_addr << ":" << from_port << std::endl;
 * });
 * client->start_client("localhost", 8080);
 * @endcode
 */
class messaging_udp_client {
public:
    /**
     * @brief Callback type for UDP data reception
     */
    using udp_data_callback = std::function<void(
        std::span<const uint8_t> data,
        const std::string& from_address,
        uint16_t from_port)>;

    /**
     * @brief Construct a UDP client with a given client_id
     * @param client_id A string identifier for this client instance
     */
    explicit messaging_udp_client(std::string_view client_id);

    /**
     * @brief Destructor - automatically stops the client if running
     */
    virtual ~messaging_udp_client() noexcept;

    // Non-copyable, movable
    messaging_udp_client(const messaging_udp_client&) = delete;
    messaging_udp_client& operator=(const messaging_udp_client&) = delete;
    messaging_udp_client(messaging_udp_client&&) noexcept;
    messaging_udp_client& operator=(messaging_udp_client&&) noexcept;

    /**
     * @brief Start the client and bind to an endpoint
     * @param remote_host The remote host address
     * @param remote_port The remote port number
     * @param local_port Optional local port to bind (0 for any)
     * @return true if started successfully
     */
    bool start_client(std::string_view remote_host, uint16_t remote_port, uint16_t local_port = 0);

    /**
     * @brief Stop the client
     */
    void stop_client();

    /**
     * @brief Send a datagram to the configured remote endpoint
     * @param data The data to send
     * @return true if data was sent
     */
    bool send_packet(std::span<const uint8_t> data);

    /**
     * @brief Send a string datagram to the configured remote endpoint
     * @param data The string data to send
     * @return true if data was sent
     */
    bool send_packet(std::string_view data);

    /**
     * @brief Send a datagram to a specific endpoint
     * @param data The data to send
     * @param host The target host address
     * @param port The target port number
     * @return true if data was sent
     */
    bool send_to(std::span<const uint8_t> data, std::string_view host, uint16_t port);

    /**
     * @brief Check if the client is running
     * @return true if running, false otherwise
     */
    bool is_running() const noexcept;

    /**
     * @brief Get the client identifier
     * @return Client ID string
     */
    std::string_view client_id() const noexcept;

    /**
     * @brief Set data received callback
     * @param callback Function to call when data is received
     */
    void set_data_callback(udp_data_callback callback);

    /**
     * @brief Set error callback
     * @param callback Function to call on errors
     */
    void set_error_callback(error_callback callback);

    /**
     * @brief Enable multicast reception
     * @param multicast_address The multicast group address
     * @return true if joined successfully
     */
    bool join_multicast_group(std::string_view multicast_address);

    /**
     * @brief Leave multicast group
     * @param multicast_address The multicast group address
     * @return true if left successfully
     */
    bool leave_multicast_group(std::string_view multicast_address);

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

// =============================================================================
// UDP Server
// =============================================================================

/**
 * @class messaging_udp_server
 * @brief A UDP server for handling datagrams from multiple endpoints
 *
 * This class provides a UDP server that listens for incoming datagrams
 * and can respond to multiple remote endpoints.
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 *
 * ### Key Features
 * - Multiple endpoint support
 * - Multicast support
 * - Broadcast support
 *
 * @code
 * import kcenon.network;
 *
 * auto server = std::make_unique<kcenon::network::core::messaging_udp_server>("udp_server");
 * server->set_data_callback([&server](std::span<const uint8_t> data,
 *                                       const std::string& from_addr,
 *                                       uint16_t from_port) {
 *     // Echo back
 *     server->send_to(data, from_addr, from_port);
 * });
 * server->start_server(8080);
 * @endcode
 */
class messaging_udp_server {
public:
    /**
     * @brief Callback type for UDP data reception
     */
    using udp_data_callback = std::function<void(
        std::span<const uint8_t> data,
        const std::string& from_address,
        uint16_t from_port)>;

    /**
     * @brief Construct a UDP server with a given server_id
     * @param server_id A string identifier for this server instance
     */
    explicit messaging_udp_server(std::string_view server_id);

    /**
     * @brief Destructor - automatically stops the server if running
     */
    virtual ~messaging_udp_server() noexcept;

    // Non-copyable, movable
    messaging_udp_server(const messaging_udp_server&) = delete;
    messaging_udp_server& operator=(const messaging_udp_server&) = delete;
    messaging_udp_server(messaging_udp_server&&) noexcept;
    messaging_udp_server& operator=(messaging_udp_server&&) noexcept;

    /**
     * @brief Start the server on a specified port
     * @param port The port to listen on
     * @return true if server started successfully
     */
    bool start_server(uint16_t port);

    /**
     * @brief Start the server on a specific interface
     * @param address The interface address to bind to
     * @param port The port to listen on
     * @return true if server started successfully
     */
    bool start_server(std::string_view address, uint16_t port);

    /**
     * @brief Stop the server
     */
    void stop_server();

    /**
     * @brief Send a datagram to a specific endpoint
     * @param data The data to send
     * @param host The target host address
     * @param port The target port number
     * @return true if data was sent
     */
    bool send_to(std::span<const uint8_t> data, std::string_view host, uint16_t port);

    /**
     * @brief Send a string datagram to a specific endpoint
     * @param data The string data to send
     * @param host The target host address
     * @param port The target port number
     * @return true if data was sent
     */
    bool send_to(std::string_view data, std::string_view host, uint16_t port);

    /**
     * @brief Broadcast data to all endpoints on the network
     * @param data The data to broadcast
     * @param port The target port number
     * @return true if data was sent
     */
    bool broadcast(std::span<const uint8_t> data, uint16_t port);

    /**
     * @brief Check if the server is running
     * @return true if running, false otherwise
     */
    bool is_running() const noexcept;

    /**
     * @brief Get the server identifier
     * @return Server ID string
     */
    std::string_view server_id() const noexcept;

    /**
     * @brief Set data received callback
     * @param callback Function to call when data is received
     */
    void set_data_callback(udp_data_callback callback);

    /**
     * @brief Set error callback
     * @param callback Function to call on errors
     */
    void set_error_callback(error_callback callback);

    /**
     * @brief Enable multicast reception
     * @param multicast_address The multicast group address
     * @return true if joined successfully
     */
    bool join_multicast_group(std::string_view multicast_address);

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

// =============================================================================
// Reliable UDP Client
// =============================================================================

/**
 * @class reliable_udp_client
 * @brief A reliable UDP client with acknowledgment and retransmission
 *
 * This class provides reliable delivery semantics on top of UDP,
 * implementing acknowledgment, retransmission, and ordering.
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 *
 * ### Key Features
 * - Guaranteed delivery with acknowledgments
 * - Automatic retransmission on timeout
 * - Sequence number based ordering
 * - Congestion control
 *
 * @code
 * import kcenon.network;
 *
 * auto client = std::make_unique<kcenon::network::core::reliable_udp_client>("rudp_client");
 * client->start_client("localhost", 8080);
 * client->send_reliable(data); // Guaranteed delivery
 * @endcode
 */
class reliable_udp_client {
public:
    /**
     * @brief Construct a reliable UDP client
     * @param client_id A string identifier for this client instance
     */
    explicit reliable_udp_client(std::string_view client_id);

    /**
     * @brief Destructor
     */
    virtual ~reliable_udp_client() noexcept;

    // Non-copyable, movable
    reliable_udp_client(const reliable_udp_client&) = delete;
    reliable_udp_client& operator=(const reliable_udp_client&) = delete;
    reliable_udp_client(reliable_udp_client&&) noexcept;
    reliable_udp_client& operator=(reliable_udp_client&&) noexcept;

    /**
     * @brief Start the client
     * @param remote_host The remote host address
     * @param remote_port The remote port number
     * @return true if started successfully
     */
    bool start_client(std::string_view remote_host, uint16_t remote_port);

    /**
     * @brief Stop the client
     */
    void stop_client();

    /**
     * @brief Send data with reliable delivery
     * @param data The data to send
     * @return true if data was queued for reliable delivery
     */
    bool send_reliable(std::span<const uint8_t> data);

    /**
     * @brief Send data unreliably (fire and forget)
     * @param data The data to send
     * @return true if data was sent
     */
    bool send_unreliable(std::span<const uint8_t> data);

    /**
     * @brief Check if the client is connected
     * @return true if connected, false otherwise
     */
    bool is_connected() const noexcept;

    /**
     * @brief Check if the client is running
     * @return true if running, false otherwise
     */
    bool is_running() const noexcept;

    /**
     * @brief Get the client identifier
     * @return Client ID string
     */
    std::string_view client_id() const noexcept;

    /**
     * @brief Set connection callback
     * @param callback Function to call when connected
     */
    void set_connection_callback(connection_callback callback);

    /**
     * @brief Set disconnection callback
     * @param callback Function to call when disconnected
     */
    void set_disconnection_callback(disconnection_callback callback);

    /**
     * @brief Set data received callback
     * @param callback Function to call when data is received
     */
    void set_data_callback(data_callback callback);

    /**
     * @brief Set error callback
     * @param callback Function to call on errors
     */
    void set_error_callback(error_callback callback);

    /**
     * @brief Set retransmission timeout
     * @param timeout Timeout duration
     */
    void set_retransmit_timeout(std::chrono::milliseconds timeout);

    /**
     * @brief Set maximum retransmission attempts
     * @param max_attempts Maximum number of retransmission attempts
     */
    void set_max_retransmit_attempts(size_t max_attempts);

    /**
     * @brief Get current round-trip time estimate
     * @return RTT in milliseconds
     */
    std::chrono::milliseconds get_rtt() const noexcept;

    /**
     * @brief Get packet loss rate
     * @return Packet loss rate (0.0 to 1.0)
     */
    double get_packet_loss_rate() const noexcept;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace kcenon::network::core
