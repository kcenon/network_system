#pragma once

/**
 * @file forward.h
 * @brief Forward declarations for network_system types
 *
 * This header provides forward declarations for commonly used types
 * in the network_system module to reduce compilation dependencies.
 */

// Core classes
namespace kcenon::network::core {
class http_client;
class http_server;
class messaging_client;
class messaging_server;
class network_context;
class connection_manager;
class session_manager;
} // namespace kcenon::network::core

// Session classes
namespace kcenon::network::session {
class messaging_session;
class secure_session;
class websocket_session;
class tcp_session;
class udp_session;
} // namespace kcenon::network::session

// Protocol support
namespace kcenon::network::protocol {
enum class protocol_type;
class http_protocol;
class websocket_protocol;
class mqtt_protocol;
class grpc_protocol;
} // namespace kcenon::network::protocol

// Internal implementation
namespace kcenon::network::internal {
class tcp_socket;
class udp_socket;
class ssl_socket;
class socket_pool;
} // namespace kcenon::network::internal

// HTTP specific
namespace kcenon::network::http {
struct request;
struct response;
enum class method;
enum class status_code;
class header;
class cookie;
} // namespace kcenon::network::http

// Security
namespace kcenon::network::security {
class ssl_context;
class certificate;
class authenticator;
class encryption_handler;
} // namespace kcenon::network::security

// Integration
namespace kcenon::network::integration {
class monitoring_interface;
class monitoring_integration_manager;
class thread_integration;
class container_integration;
} // namespace kcenon::network::integration

// Metrics
namespace kcenon::network::metrics {
class metric_reporter;
class connection_metrics;
class performance_metrics;
} // namespace kcenon::network::metrics

// Utilities
namespace kcenon::network::utils {
class url;
class uri;
class ip_address;
class port;
} // namespace kcenon::network::utils