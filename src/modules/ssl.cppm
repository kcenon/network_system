// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file ssl.cppm
 * @brief SSL/TLS partition for network_system module.
 *
 * This partition provides secure networking functionality using
 * SSL/TLS for encrypted communication over TCP and DTLS for UDP.
 *
 * Contents:
 * - secure_messaging_client: SSL/TLS encrypted TCP client
 * - secure_messaging_server: SSL/TLS encrypted TCP server
 * - secure_messaging_udp_client: DTLS encrypted UDP client
 * - secure_messaging_udp_server: DTLS encrypted UDP server
 *
 * @note This partition requires OpenSSL to be available.
 *
 * @see kcenon.network for the primary module interface
 * @see kcenon.network:tcp for base TCP functionality
 * @see kcenon.network:udp for base UDP functionality
 */

module;

// =============================================================================
// Global Module Fragment - Standard Library Headers
// =============================================================================
#include <atomic>
#include <chrono>
#include <filesystem>
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
#include <asio/ssl.hpp>

export module kcenon.network:ssl;

import :core;

// =============================================================================
// SSL Configuration Types
// =============================================================================

export namespace kcenon::network::core {

/**
 * @brief SSL/TLS protocol version enumeration
 */
enum class ssl_protocol {
    tls_1_2,    ///< TLS 1.2
    tls_1_3,    ///< TLS 1.3 (recommended)
    dtls_1_2,   ///< DTLS 1.2 for UDP
    dtls_1_3    ///< DTLS 1.3 for UDP
};

/**
 * @brief SSL verification mode
 */
enum class ssl_verify_mode {
    none,               ///< No verification
    peer,               ///< Verify peer certificate
    fail_if_no_peer_cert, ///< Fail if no peer certificate
    client_once         ///< Request client certificate once
};

/**
 * @brief SSL/TLS configuration structure
 */
struct ssl_config {
    /// Path to certificate file (PEM format)
    std::filesystem::path certificate_path;

    /// Path to private key file (PEM format)
    std::filesystem::path private_key_path;

    /// Path to CA certificate file for verification (PEM format)
    std::filesystem::path ca_certificate_path;

    /// Password for encrypted private key (empty if not encrypted)
    std::string private_key_password;

    /// SSL protocol version
    ssl_protocol protocol = ssl_protocol::tls_1_3;

    /// Verification mode
    ssl_verify_mode verify_mode = ssl_verify_mode::peer;

    /// Server name for SNI (Server Name Indication)
    std::string server_name;

    /// Cipher list (OpenSSL format, empty for default)
    std::string cipher_list;

    /// Enable session resumption
    bool enable_session_resumption = true;

    /// Enable OCSP stapling
    bool enable_ocsp_stapling = false;
};

// =============================================================================
// Secure TCP Client
// =============================================================================

/**
 * @class secure_messaging_client
 * @brief An SSL/TLS encrypted TCP client
 *
 * This class provides a secure TCP client with SSL/TLS encryption,
 * certificate verification, and modern TLS features.
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 *
 * ### Key Features
 * - TLS 1.2/1.3 support
 * - Certificate verification
 * - SNI support
 * - Session resumption
 *
 * @code
 * import kcenon.network;
 *
 * using namespace kcenon::network::core;
 *
 * ssl_config config;
 * config.ca_certificate_path = "/path/to/ca.pem";
 * config.server_name = "example.com";
 *
 * auto client = std::make_unique<secure_messaging_client>("secure_client", config);
 * client->start_client("example.com", 443);
 * @endcode
 */
class secure_messaging_client {
public:
    /**
     * @brief Construct a secure client with SSL configuration
     * @param client_id A string identifier for this client instance
     * @param config SSL/TLS configuration
     */
    explicit secure_messaging_client(std::string_view client_id, const ssl_config& config = {});

    /**
     * @brief Destructor
     */
    virtual ~secure_messaging_client() noexcept;

    // Non-copyable, movable
    secure_messaging_client(const secure_messaging_client&) = delete;
    secure_messaging_client& operator=(const secure_messaging_client&) = delete;
    secure_messaging_client(secure_messaging_client&&) noexcept;
    secure_messaging_client& operator=(secure_messaging_client&&) noexcept;

    /**
     * @brief Start the client and connect securely
     * @param host The host address to connect to
     * @param port The port number to connect to
     * @return true if connection initiated successfully
     */
    bool start_client(std::string_view host, uint16_t port);

    /**
     * @brief Stop the client
     */
    void stop_client();

    /**
     * @brief Wait for the client to stop
     * @param timeout Maximum wait time
     * @return true if stopped within timeout
     */
    bool wait_for_stop(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /**
     * @brief Send encrypted data
     * @param data The data to send
     * @return true if data was queued for sending
     */
    bool send_packet(std::span<const uint8_t> data);

    /**
     * @brief Send encrypted string data
     * @param data The string data to send
     * @return true if data was queued for sending
     */
    bool send_packet(std::string_view data);

    /**
     * @brief Check if the client is connected and TLS handshake completed
     * @return true if securely connected
     */
    bool is_connected() const noexcept;

    /**
     * @brief Check if the client is running
     * @return true if running
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
     * @brief Set connection callback (called after TLS handshake)
     * @param callback Function to call when securely connected
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
     * @param callback Function to call when decrypted data is received
     */
    void set_data_callback(data_callback callback);

    /**
     * @brief Get peer certificate information
     * @return Peer certificate subject name, or empty if not available
     */
    std::optional<std::string> get_peer_certificate_subject() const;

    /**
     * @brief Get the negotiated TLS protocol version
     * @return Protocol version string
     */
    std::string get_protocol_version() const;

    /**
     * @brief Get the negotiated cipher suite
     * @return Cipher suite name
     */
    std::string get_cipher_suite() const;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

// =============================================================================
// Secure TCP Server
// =============================================================================

/**
 * @class secure_messaging_server
 * @brief An SSL/TLS encrypted TCP server
 *
 * This class provides a secure TCP server with SSL/TLS encryption,
 * supporting multiple encrypted client connections.
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 *
 * ### Key Features
 * - TLS 1.2/1.3 support
 * - Client certificate verification
 * - Session resumption
 * - ALPN negotiation
 *
 * @code
 * import kcenon.network;
 *
 * using namespace kcenon::network::core;
 *
 * ssl_config config;
 * config.certificate_path = "/path/to/server.pem";
 * config.private_key_path = "/path/to/server.key";
 *
 * auto server = std::make_unique<secure_messaging_server>("secure_server", config);
 * server->start_server(443);
 * @endcode
 */
class secure_messaging_server {
public:
    /**
     * @brief Callback type for secure client connection events
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
     * @brief Construct a secure server with SSL configuration
     * @param server_id A string identifier for this server instance
     * @param config SSL/TLS configuration
     */
    explicit secure_messaging_server(std::string_view server_id, const ssl_config& config);

    /**
     * @brief Destructor
     */
    virtual ~secure_messaging_server() noexcept;

    // Non-copyable, movable
    secure_messaging_server(const secure_messaging_server&) = delete;
    secure_messaging_server& operator=(const secure_messaging_server&) = delete;
    secure_messaging_server(secure_messaging_server&&) noexcept;
    secure_messaging_server& operator=(secure_messaging_server&&) noexcept;

    /**
     * @brief Start the secure server
     * @param port The port to listen on
     * @param backlog The listen backlog size
     * @return true if server started successfully
     */
    bool start_server(uint16_t port, int backlog = 128);

    /**
     * @brief Start the secure server on a specific interface
     * @param address The interface address to bind to
     * @param port The port to listen on
     * @param backlog The listen backlog size
     * @return true if server started successfully
     */
    bool start_server(std::string_view address, uint16_t port, int backlog = 128);

    /**
     * @brief Stop the server
     */
    void stop_server();

    /**
     * @brief Wait for the server to stop
     * @param timeout Maximum wait time
     * @return true if stopped within timeout
     */
    bool wait_for_stop(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /**
     * @brief Send encrypted data to a specific client
     * @param session_id The target client's session ID
     * @param data The data to send
     * @return true if data was queued for sending
     */
    bool send_to(std::string_view session_id, std::span<const uint8_t> data);

    /**
     * @brief Broadcast encrypted data to all connected clients
     * @param data The data to broadcast
     */
    void broadcast(std::span<const uint8_t> data);

    /**
     * @brief Disconnect a specific client
     * @param session_id The client's session ID to disconnect
     */
    void disconnect(std::string_view session_id);

    /**
     * @brief Check if the server is running
     * @return true if running
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
     * @param callback Function to call when a client connects and TLS handshake completes
     */
    void set_client_connected_callback(client_connected_callback callback);

    /**
     * @brief Set client disconnected callback
     * @param callback Function to call when a client disconnects
     */
    void set_client_disconnected_callback(client_disconnected_callback callback);

    /**
     * @brief Set client data callback
     * @param callback Function to call when decrypted data is received
     */
    void set_client_data_callback(client_data_callback callback);

    /**
     * @brief Set error callback
     * @param callback Function to call on errors
     */
    void set_error_callback(error_callback callback);

    /**
     * @brief Get peer certificate for a session
     * @param session_id The session ID
     * @return Certificate subject name, or empty if not available
     */
    std::optional<std::string> get_peer_certificate_subject(std::string_view session_id) const;

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

// =============================================================================
// Secure UDP Client (DTLS)
// =============================================================================

/**
 * @class secure_messaging_udp_client
 * @brief A DTLS encrypted UDP client
 *
 * This class provides secure UDP communication using DTLS (Datagram TLS),
 * combining the benefits of UDP with encryption.
 *
 * ### Key Features
 * - DTLS 1.2/1.3 support
 * - Replay protection
 * - Certificate verification
 *
 * @code
 * import kcenon.network;
 *
 * using namespace kcenon::network::core;
 *
 * ssl_config config;
 * config.protocol = ssl_protocol::dtls_1_2;
 * config.ca_certificate_path = "/path/to/ca.pem";
 *
 * auto client = std::make_unique<secure_messaging_udp_client>("dtls_client", config);
 * client->start_client("example.com", 5684);
 * @endcode
 */
class secure_messaging_udp_client {
public:
    /**
     * @brief Construct a secure UDP client with DTLS configuration
     * @param client_id A string identifier for this client instance
     * @param config SSL/DTLS configuration
     */
    explicit secure_messaging_udp_client(std::string_view client_id, const ssl_config& config = {});

    /**
     * @brief Destructor
     */
    virtual ~secure_messaging_udp_client() noexcept;

    // Non-copyable, movable
    secure_messaging_udp_client(const secure_messaging_udp_client&) = delete;
    secure_messaging_udp_client& operator=(const secure_messaging_udp_client&) = delete;
    secure_messaging_udp_client(secure_messaging_udp_client&&) noexcept;
    secure_messaging_udp_client& operator=(secure_messaging_udp_client&&) noexcept;

    /**
     * @brief Start the client and perform DTLS handshake
     * @param remote_host The remote host address
     * @param remote_port The remote port number
     * @return true if connection initiated successfully
     */
    bool start_client(std::string_view remote_host, uint16_t remote_port);

    /**
     * @brief Stop the client
     */
    void stop_client();

    /**
     * @brief Send encrypted datagram
     * @param data The data to send
     * @return true if data was sent
     */
    bool send_packet(std::span<const uint8_t> data);

    /**
     * @brief Check if the client is connected and DTLS handshake completed
     * @return true if securely connected
     */
    bool is_connected() const noexcept;

    /**
     * @brief Check if the client is running
     * @return true if running
     */
    bool is_running() const noexcept;

    /**
     * @brief Get the client identifier
     * @return Client ID string
     */
    std::string_view client_id() const noexcept;

    /**
     * @brief Set connection callback (called after DTLS handshake)
     * @param callback Function to call when securely connected
     */
    void set_connection_callback(connection_callback callback);

    /**
     * @brief Set disconnection callback
     * @param callback Function to call when disconnected
     */
    void set_disconnection_callback(disconnection_callback callback);

    /**
     * @brief Set data received callback
     * @param callback Function to call when decrypted data is received
     */
    void set_data_callback(data_callback callback);

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
// Secure UDP Server (DTLS)
// =============================================================================

/**
 * @class secure_messaging_udp_server
 * @brief A DTLS encrypted UDP server
 *
 * This class provides secure UDP server using DTLS,
 * supporting multiple encrypted client connections.
 *
 * @code
 * import kcenon.network;
 *
 * using namespace kcenon::network::core;
 *
 * ssl_config config;
 * config.protocol = ssl_protocol::dtls_1_2;
 * config.certificate_path = "/path/to/server.pem";
 * config.private_key_path = "/path/to/server.key";
 *
 * auto server = std::make_unique<secure_messaging_udp_server>("dtls_server", config);
 * server->start_server(5684);
 * @endcode
 */
class secure_messaging_udp_server {
public:
    /**
     * @brief Callback type for DTLS data reception
     */
    using dtls_data_callback = std::function<void(
        const std::string& session_id,
        std::span<const uint8_t> data)>;

    /**
     * @brief Construct a secure UDP server with DTLS configuration
     * @param server_id A string identifier for this server instance
     * @param config SSL/DTLS configuration
     */
    explicit secure_messaging_udp_server(std::string_view server_id, const ssl_config& config);

    /**
     * @brief Destructor
     */
    virtual ~secure_messaging_udp_server() noexcept;

    // Non-copyable, movable
    secure_messaging_udp_server(const secure_messaging_udp_server&) = delete;
    secure_messaging_udp_server& operator=(const secure_messaging_udp_server&) = delete;
    secure_messaging_udp_server(secure_messaging_udp_server&&) noexcept;
    secure_messaging_udp_server& operator=(secure_messaging_udp_server&&) noexcept;

    /**
     * @brief Start the secure UDP server
     * @param port The port to listen on
     * @return true if server started successfully
     */
    bool start_server(uint16_t port);

    /**
     * @brief Start the secure UDP server on a specific interface
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
     * @brief Send encrypted datagram to a specific session
     * @param session_id The target session ID
     * @param data The data to send
     * @return true if data was sent
     */
    bool send_to(std::string_view session_id, std::span<const uint8_t> data);

    /**
     * @brief Check if the server is running
     * @return true if running
     */
    bool is_running() const noexcept;

    /**
     * @brief Get the number of active DTLS sessions
     * @return Number of active sessions
     */
    size_t session_count() const noexcept;

    /**
     * @brief Get the server identifier
     * @return Server ID string
     */
    std::string_view server_id() const noexcept;

    /**
     * @brief Set session connected callback
     * @param callback Function to call when a DTLS session is established
     */
    void set_session_connected_callback(std::function<void(const std::string&)> callback);

    /**
     * @brief Set session disconnected callback
     * @param callback Function to call when a DTLS session ends
     */
    void set_session_disconnected_callback(std::function<void(const std::string&)> callback);

    /**
     * @brief Set data received callback
     * @param callback Function to call when decrypted data is received
     */
    void set_data_callback(dtls_data_callback callback);

    /**
     * @brief Set error callback
     * @param callback Function to call on errors
     */
    void set_error_callback(error_callback callback);

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace kcenon::network::core
