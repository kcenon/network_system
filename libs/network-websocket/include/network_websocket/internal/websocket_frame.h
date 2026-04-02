// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

// Wrapper header that includes the main project's websocket_frame.h
// This allows network-websocket to be used both standalone and integrated

#include "internal/websocket/websocket_frame.h"

namespace network_websocket
{
	// Re-export types from the main namespace for convenience
	using kcenon::network::internal::websocket_frame;
	using kcenon::network::internal::ws_frame_header;
	using kcenon::network::internal::ws_opcode;
} // namespace network_websocket
