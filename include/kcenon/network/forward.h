#pragma once

/**
 * @file forward.h
 * @brief Forward declarations for network_system types
 *
 * This header provides forward declarations for commonly used types
 * in the network_system module to reduce compilation dependencies.
 */

/// @name Core classes
/// @{
namespace kcenon::network::core {
/// @brief HTTP client for making outbound HTTP requests
class http_client;
/// @brief HTTP server for handling inbound HTTP requests
class http_server;
/// @brief Messaging client for bidirectional message-based communication
class messaging_client;
/// @brief Messaging server for accepting message-based connections
class messaging_server;
/// @brief Central context managing network subsystem lifecycle
class network_context;
/// @brief Manages active connections and their lifecycle
class connection_manager;
/// @brief Manages active sessions across connections
class session_manager;
} // namespace kcenon::network::core
/// @}

/// @name Session classes
/// @{
namespace kcenon::network::session {
/// @brief Session for message-based protocol communication
class messaging_session;
/// @brief Session with TLS/SSL encryption support
class secure_session;
/// @brief Session for WebSocket protocol communication
class websocket_session;
/// @brief Session for raw TCP stream communication
class tcp_session;
/// @brief Session for UDP datagram communication
class udp_session;
} // namespace kcenon::network::session
/// @}

/// @name Protocol support
/// @{
namespace kcenon::network::protocol {
/// @brief Enumeration of supported network protocol types
enum class protocol_type;
/// @brief HTTP protocol handler
class http_protocol;
/// @brief WebSocket protocol handler
class websocket_protocol;
/// @brief MQTT protocol handler
class mqtt_protocol;
/// @brief gRPC protocol handler
class grpc_protocol;
} // namespace kcenon::network::protocol
/// @}

/// @name Internal implementation
/// @{
namespace kcenon::network::internal {
/// @brief Low-level TCP socket wrapper
class tcp_socket;
/// @brief Low-level UDP socket wrapper
class udp_socket;
/// @brief SSL/TLS-wrapped socket
class ssl_socket;
/// @brief Pool of reusable socket connections
class socket_pool;
} // namespace kcenon::network::internal
/// @}

/// @name HTTP specific
/// @{
namespace kcenon::network::http {
/// @brief HTTP request data structure
struct request;
/// @brief HTTP response data structure
struct response;
/// @brief HTTP method enumeration (GET, POST, PUT, etc.)
enum class method;
/// @brief HTTP status code enumeration (200, 404, 500, etc.)
enum class status_code;
/// @brief HTTP header representation
class header;
/// @brief HTTP cookie representation
class cookie;
} // namespace kcenon::network::http
/// @}

/// @name Security
/// @{
namespace kcenon::network::security {
/// @brief SSL/TLS context configuration and management
class ssl_context;
/// @brief X.509 certificate management
class certificate;
/// @brief Authentication handler for connection validation
class authenticator;
/// @brief Handles data encryption and decryption
class encryption_handler;
} // namespace kcenon::network::security
/// @}

/// @name Integration
/// @{
namespace kcenon::network::integration {
/// @brief Interface for monitoring subsystem integration
class monitoring_interface;
/// @brief Manages monitoring integration lifecycle
class monitoring_integration_manager;
/// @brief Integration with thread_system for async I/O
class thread_integration;
/// @brief Integration with container_system for serialization
class container_integration;
} // namespace kcenon::network::integration
/// @}

/// @name Metrics
/// @{
namespace kcenon::network::metrics {
/// @brief Reports network metrics to monitoring backends
class metric_reporter;
/// @brief Tracks per-connection metrics (latency, throughput)
class connection_metrics;
/// @brief Tracks overall network performance metrics
class performance_metrics;
} // namespace kcenon::network::metrics
/// @}

/// @name Utilities
/// @{
namespace kcenon::network::utils {
/// @brief URL parser and builder
class url;
/// @brief URI parser and builder (RFC 3986)
class uri;
/// @brief IPv4/IPv6 address representation
class ip_address;
/// @brief Network port representation and validation
class port;
} // namespace kcenon::network::utils
/// @}