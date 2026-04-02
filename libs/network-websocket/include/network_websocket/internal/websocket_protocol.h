// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

// Wrapper header that includes the main project's websocket_protocol.h
// This allows network-websocket to be used both standalone and integrated

#include "internal/websocket/websocket_protocol.h"

namespace network_websocket
{
	// Re-export types from the main namespace for convenience
	using kcenon::network::internal::websocket_protocol;
	using kcenon::network::internal::ws_close_code;
	using kcenon::network::internal::ws_message;
	using kcenon::network::internal::ws_message_type;
} // namespace network_websocket
