// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

/**
 * @file network_system.h
 * @brief Main header for the Network System
 *
 * This header provides access to all core Network System functionality
 * including messaging clients, servers, and session management.
 *
 * @author kcenon
 * @date 2025-09-19
 */

// Core networking components
#include "network_system/core/messaging_client.h"
#include "network_system/core/messaging_server.h"

// Session management
#include "network_system/session/messaging_session.h"

// Integration interfaces
#include "network_system/integration/messaging_bridge.h"
#include "network_system/integration/thread_integration.h"
#include "network_system/integration/container_integration.h"

// Compatibility layer
#include "network_system/compatibility.h"

/**
 * @namespace network_system
 * @brief Main namespace for all Network System components
 */
namespace network_system {

/**
 * @brief Initialize the network system
 * @return true if initialization successful, false otherwise
 */
bool initialize();

/**
 * @brief Shutdown the network system
 */
void shutdown();

} // namespace network_system