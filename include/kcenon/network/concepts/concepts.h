// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/network/config/feature_flags.h>

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
 * When KCENON_WITH_COMMON_SYSTEM is defined, this header also provides
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
#if KCENON_WITH_COMMON_SYSTEM
#include <kcenon/common/patterns/result.h>
#include <kcenon/common/concepts/core.h>
#include <kcenon/common/concepts/container.h>
#endif

// Network-specific concepts (from detail directory)
#include "kcenon/network/detail/concepts/network_concepts.h"
#include "kcenon/network/detail/concepts/socket_concepts.h"

#if KCENON_WITH_COMMON_SYSTEM

namespace kcenon::network::concepts {

// Re-export commonly used common_system concepts for convenience
using kcenon::common::concepts::Resultable;
using kcenon::common::concepts::Unwrappable;
using kcenon::common::concepts::Mappable;

} // namespace kcenon::network::concepts

#endif // KCENON_WITH_COMMON_SYSTEM

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
