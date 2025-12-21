#pragma once

#include <kcenon/network/forward.h>

// Backward compatibility namespace aliases for commonly used types
// These allow code using network_system::* namespace to work with
// the new kcenon::network::* namespace without including full headers.

// Additional forward declarations needed for pacs_system
namespace kcenon::network::core {
class secure_messaging_server;
}  // namespace kcenon::network::core

namespace network_system::core {
using messaging_client = kcenon::network::core::messaging_client;
using messaging_server = kcenon::network::core::messaging_server;
using secure_messaging_server = kcenon::network::core::secure_messaging_server;
using network_context = kcenon::network::core::network_context;
using session_manager = kcenon::network::core::session_manager;
}  // namespace network_system::core

namespace network_system::session {
using messaging_session = kcenon::network::session::messaging_session;
using secure_session = kcenon::network::session::secure_session;
}  // namespace network_system::session
