/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#pragma once

/**
 * @file concepts.h
 * @brief Unified header for all C++20 concepts in network_system.
 *
 * This header provides a single include point for all concepts defined
 * in the network_system library. Including this header gives access to
 * all concept definitions for compile-time type validation.
 *
 * Available concept categories:
 * - Buffer concepts: ByteBuffer, MutableByteBuffer
 * - Callback concepts: DataReceiveHandler, ErrorHandler, SessionHandler
 * - Component concepts: NetworkClient, NetworkServer, NetworkSession
 * - Pipeline concepts: DataTransformer, ReversibleDataTransformer
 *
 * When BUILD_WITH_COMMON_SYSTEM is defined, this header also provides
 * access to common_system concepts:
 * - Result/Optional: Resultable, Unwrappable, Mappable
 * - Callable: Invocable, VoidCallable, Predicate
 *
 * Requirements:
 * - C++20 compiler with concepts support
 * - GCC 10+, Clang 10+, MSVC 2022+
 *
 * Example usage:
 * @code
 * #include <kcenon/network/concepts/concepts.h>
 *
 * using namespace kcenon::network::concepts;
 *
 * template<DataReceiveHandler Handler>
 * void set_handler(Handler&& handler) {
 *     // Handler will be called with const std::vector<uint8_t>&
 * }
 *
 * template<NetworkClient Client>
 * void send_data(Client& client, std::vector<uint8_t> data) {
 *     if (client.is_connected()) {
 *         client.send_packet(std::move(data));
 *     }
 * }
 * @endcode
 *
 * Benefits of using concepts:
 * - **Clearer error messages**: Template errors are displayed as concept
 *   violations instead of verbose SFINAE failures
 * - **Self-documenting code**: Concepts express type requirements explicitly
 * - **Better IDE support**: More accurate auto-completion and type hints
 *
 * @see network_concepts.h for network-specific concepts
 * @see common_system/concepts/concepts.h for common concepts (when available)
 */

// Re-export common_system concepts when available
// Note: Include core.h directly to avoid forward declaration conflicts in callable.h
// The callable.h has `class VoidResult;` forward decl that conflicts with
// `using VoidResult = Result<std::monostate>;` in result.h
#ifdef BUILD_WITH_COMMON_SYSTEM
#include <kcenon/common/patterns/result.h>
#include <kcenon/common/concepts/core.h>
#include <kcenon/common/concepts/container.h>
#endif

// Network-specific concepts (after common_system to avoid forward decl conflicts)
#include "network_concepts.h"

#ifdef BUILD_WITH_COMMON_SYSTEM

namespace kcenon::network::concepts {

// Re-export commonly used common_system concepts for convenience
using kcenon::common::concepts::Resultable;
using kcenon::common::concepts::Unwrappable;
using kcenon::common::concepts::Mappable;

} // namespace kcenon::network::concepts

#endif // BUILD_WITH_COMMON_SYSTEM

/**
 * @namespace kcenon::network::concepts
 * @brief C++20 concepts for compile-time type validation in network_system.
 *
 * This namespace contains all concept definitions used throughout
 * the network_system library. Concepts provide:
 *
 * - Compile-time type checking with clear error messages
 * - Self-documenting interface requirements
 * - Simplified template constraints
 * - Better IDE support for auto-completion
 *
 * Concept Categories:
 *
 * **Data Buffers**:
 * - ByteBuffer: Types providing data() and size()
 * - MutableByteBuffer: Resizable byte buffers
 *
 * **Callbacks**:
 * - DataReceiveHandler: Handlers for received data
 * - ErrorHandler: Handlers for error codes
 * - ConnectionHandler: Handlers for connection events
 * - SessionHandler: Handlers with session context
 * - SessionDataHandler: Data handlers with session
 * - SessionErrorHandler: Error handlers with session
 * - DisconnectionHandler: Handlers for disconnection
 * - RetryCallback: Handlers for retry notifications
 *
 * **Network Components**:
 * - NetworkClient: Client interface requirements
 * - NetworkServer: Server interface requirements
 * - NetworkSession: Session interface requirements
 *
 * **Pipeline**:
 * - DataTransformer: Data transformation types
 * - ReversibleDataTransformer: Bidirectional transformers
 *
 * **Utility**:
 * - Duration: Time duration types
 *
 * When common_system is available:
 * - NetworkResultHandler: Result-aware callbacks
 * - AsyncNetworkCallback: Async-suitable callbacks
 */
namespace kcenon::network::concepts {
// All concepts are defined in their respective headers
// This namespace block serves as documentation
} // namespace kcenon::network::concepts
