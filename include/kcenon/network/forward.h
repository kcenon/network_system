#pragma once

/**
 * @file forward.h
 * @brief Forward declarations for network_system types
 *
 * This header provides forward declarations for commonly used types
 * in the network_system module to reduce compilation dependencies.
 */

namespace kcenon::network {

// Core classes
namespace core {
    class http_client;
    class http_server;
    class messaging_client;
    class messaging_server;
    class network_context;
    class connection_manager;
    class session_manager;
}

// Session classes
namespace session {
    class messaging_session;
    class secure_session;
    class websocket_session;
    class tcp_session;
    class udp_session;
}

// Protocol support
namespace protocol {
    enum class protocol_type;
    class http_protocol;
    class websocket_protocol;
    class mqtt_protocol;
    class grpc_protocol;
}

// Internal implementation
namespace internal {
    class tcp_socket;
    class udp_socket;
    class ssl_socket;
    class socket_pool;
}

// HTTP specific
namespace http {
    struct request;
    struct response;
    enum class method;
    enum class status_code;
    class header;
    class cookie;
}

// Security
namespace security {
    class ssl_context;
    class certificate;
    class authenticator;
    class encryption_handler;
}

// Integration
namespace integration {
    class monitoring_interface;
    class monitoring_integration_manager;
    class thread_integration;
    class container_integration;
}

// Metrics
namespace metrics {
    class metric_reporter;
    class connection_metrics;
    class performance_metrics;
}

// Utilities
namespace utils {
    class url;
    class uri;
    class ip_address;
    class port;
}

} // namespace kcenon::network