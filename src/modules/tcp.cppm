// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file tcp.cppm
 * @brief TCP partition for network_system module.
 *
 * This partition provides TCP networking functionality including
 * messaging clients and servers using the TCP protocol.
 *
 * Contents:
 * - messaging_client: TCP client with async I/O and pipeline support
 * - messaging_server: TCP server for handling multiple client connections
 * - messaging_session: Session management for connected clients
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
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Third-party headers
#include <asio.hpp>

export module kcenon.network:tcp;

import :core;

// =============================================================================
// TCP Client
// =============================================================================

export namespace kcenon::network::core {

/**
 * @class messaging_client
 * @brief A basic TCP client for asynchronous messaging
 *
 * This class provides a TCP client that connects to a remote host,
 * sends/receives data using asynchronous operations, and can apply
 * a pipeline for data transformations.
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 * - Socket access is protected by internal mutex.
 * - Atomic flags prevent race conditions during state transitions.
 * - send_packet() can be called from any thread safely.
 *
 * ### Key Features
 * - Uses asio::io_context for async I/O
 * - Automatic reconnection support
 * - Optional compression and encryption pipelines
 * - Heartbeat mechanism for connection health
 *
 * @code
 * import kcenon.network;
 *
 * auto client = std::make_unique<kcenon::network::core::messaging_client>("my_client");
 * client->set_connection_callback([]() {
 *     std::cout << "Connected!" << std::endl;
 * });
 * client->set_data_callback([](std::span<const uint8_t> data) {
 *     std::cout << "Received " << data.size() << " bytes" << std::endl;
 * });
 * client->start_client("localhost", 8080);
 * @endcode
 */
class messaging_client {
public:
    /**
     * @brief Construct a client with a given client_id
     * @param client_id A string identifier for this client instance
     */
    explicit messaging_client(std::string_view client_id);

    /**
     * @brief Destructor - automatically stops the client if running
     */
    virtual ~messaging_client() noexcept;

    // Non-copyable, movable
    messaging_client(const messaging_client&) = delete;
    messaging_client& operator=(const messaging_client&) = delete;
    messaging_client(messaging_client&&) noexcept;
    messaging_client& operator=(messaging_client&&) noexcept;

    /**
     * @brief Start the client and connect to a remote host
     * @param host The host address to connect to
     * @param port The port number to connect to
     * @return true if connection initiated successfully
     */
    bool start_client(std::string_view host, uint16_t port);

    /**
     * @brief Stop the client and disconnect
     */
    void stop_client();

    /**
     * @brief Wait for the client to stop
     * @param timeout Maximum wait time
     * @return true if stopped within timeout
     */
    bool wait_for_stop(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /**
     * @brief Send data to the connected server
     * @param data The data to send
     * @return true if data was queued for sending
     */
    bool send_packet(std::span<const uint8_t> data);

    /**
     * @brief Send string data to the connected server
     * @param data The string data to send
     * @return true if data was queued for sending
     */
    bool send_packet(std::string_view data);

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
     * @brief Get the current connection state
     * @return Current connection state
     */
    connection_state get_state() const noexcept;

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
     * @brief Set error callback
     * @param callback Function to call on errors
     */
    void set_error_callback(error_callback callback);

    /**
     * @brief Set data received callback
     * @param callback Function to call when data is received
     */
    void set_data_callback(data_callback callback);

    /**
     * @brief Enable automatic reconnection
     * @param enable true to enable, false to disable
     * @param delay Delay between reconnection attempts
     */
    void set_auto_reconnect(bool enable, std::chrono::seconds delay = std::chrono::seconds(5));

    /**
     * @brief Enable heartbeat mechanism
     * @param enable true to enable, false to disable
     * @param interval Heartbeat interval
     */
    void set_heartbeat(bool enable, std::chrono::seconds interval = std::chrono::seconds(30));

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

// =============================================================================
// TCP Server
// =============================================================================

/**
 * @class messaging_server
 * @brief A TCP server for handling multiple client connections
 *
 * This class provides a TCP server that accepts client connections,
 * manages sessions, and handles data exchange with multiple clients
 * concurrently.
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 * - Session management is internally synchronized.
 * - Callbacks may be invoked from multiple threads.
 *
 * ### Key Features
 * - Multiple concurrent client support
 * - Session-based connection management
 * - Broadcast capability to all connected clients
 * - Optional SSL/TLS upgrade support
 *
 * @code
 * import kcenon.network;
 *
 * auto server = std::make_unique<kcenon::network::core::messaging_server>("my_server");
 * server->set_client_connected_callback([](const std::string& session_id) {
 *     std::cout << "Client connected: " << session_id << std::endl;
 * });
 * server->start_server(8080);
 * @endcode
 */
class messaging_server {
public:
    /**
     * @brief Callback type for client connection events
     */
    using client_connected_callback = std::function<void(const std::string& session_id)>;

    /**
     * @brief Callback type for client disconnection events
     */
    using client_disconnected_callback = std::function<void(const std::string& session_id)>;

    /**
     * @brief Callback type for client data events
     */
    using client_data_callback = std::function<void(
        const std::string& session_id,
        std::span<const uint8_t> data)>;

    /**
     * @brief Construct a server with a given server_id
     * @param server_id A string identifier for this server instance
     */
    explicit messaging_server(std::string_view server_id);

    /**
     * @brief Destructor - automatically stops the server if running
     */
    virtual ~messaging_server() noexcept;

    // Non-copyable, movable
    messaging_server(const messaging_server&) = delete;
    messaging_server& operator=(const messaging_server&) = delete;
    messaging_server(messaging_server&&) noexcept;
    messaging_server& operator=(messaging_server&&) noexcept;

    /**
     * @brief Start the server on a specified port
     * @param port The port to listen on
     * @param backlog The listen backlog size
     * @return true if server started successfully
     */
    bool start_server(uint16_t port, int backlog = 128);

    /**
     * @brief Start the server on a specific interface
     * @param address The interface address to bind to
     * @param port The port to listen on
     * @param backlog The listen backlog size
     * @return true if server started successfully
     */
    bool start_server(std::string_view address, uint16_t port, int backlog = 128);

    /**
     * @brief Stop the server and disconnect all clients
     */
    void stop_server();

    /**
     * @brief Wait for the server to stop
     * @param timeout Maximum wait time
     * @return true if stopped within timeout
     */
    bool wait_for_stop(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /**
     * @brief Send data to a specific client
     * @param session_id The target client's session ID
     * @param data The data to send
     * @return true if data was queued for sending
     */
    bool send_to(std::string_view session_id, std::span<const uint8_t> data);

    /**
     * @brief Send data to a specific client
     * @param session_id The target client's session ID
     * @param data The string data to send
     * @return true if data was queued for sending
     */
    bool send_to(std::string_view session_id, std::string_view data);

    /**
     * @brief Broadcast data to all connected clients
     * @param data The data to broadcast
     */
    void broadcast(std::span<const uint8_t> data);

    /**
     * @brief Broadcast string data to all connected clients
     * @param data The string data to broadcast
     */
    void broadcast(std::string_view data);

    /**
     * @brief Disconnect a specific client
     * @param session_id The client's session ID to disconnect
     */
    void disconnect(std::string_view session_id);

    /**
     * @brief Check if the server is running
     * @return true if running, false otherwise
     */
    bool is_running() const noexcept;

    /**
     * @brief Get the number of connected clients
     * @return Number of connected clients
     */
    size_t client_count() const noexcept;

    /**
     * @brief Get the server identifier
     * @return Server ID string
     */
    std::string_view server_id() const noexcept;

    /**
     * @brief Set client connected callback
     * @param callback Function to call when a client connects
     */
    void set_client_connected_callback(client_connected_callback callback);

    /**
     * @brief Set client disconnected callback
     * @param callback Function to call when a client disconnects
     */
    void set_client_disconnected_callback(client_disconnected_callback callback);

    /**
     * @brief Set client data callback
     * @param callback Function to call when data is received from a client
     */
    void set_client_data_callback(client_data_callback callback);

    /**
     * @brief Set error callback
     * @param callback Function to call on errors
     */
    void set_error_callback(error_callback callback);

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

// =============================================================================
// Messaging Session
// =============================================================================

/**
 * @class messaging_session
 * @brief Represents a client session on the server
 *
 * This class manages the state and communication for a single
 * connected client on the server side.
 */
class messaging_session : public std::enable_shared_from_this<messaging_session> {
public:
    /**
     * @brief Construct a session with a unique ID
     * @param session_id Unique session identifier
     */
    explicit messaging_session(std::string session_id);

    /**
     * @brief Destructor
     */
    virtual ~messaging_session() noexcept;

    /**
     * @brief Get the session ID
     * @return Session ID string
     */
    const std::string& session_id() const noexcept;

    /**
     * @brief Check if the session is active
     * @return true if active, false otherwise
     */
    bool is_active() const noexcept;

    /**
     * @brief Get the remote endpoint address
     * @return Remote address string
     */
    std::string remote_address() const;

    /**
     * @brief Get the remote endpoint port
     * @return Remote port number
     */
    uint16_t remote_port() const;

    /**
     * @brief Send data to the client
     * @param data The data to send
     * @return true if data was queued for sending
     */
    bool send(std::span<const uint8_t> data);

    /**
     * @brief Close the session
     */
    void close();

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace kcenon::network::core
