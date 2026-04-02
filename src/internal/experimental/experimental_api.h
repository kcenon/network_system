// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/*!
 * \file experimental_api.h
 * \brief Macros and utilities for marking experimental APIs
 *
 * This header provides compile-time markers for experimental APIs that may
 * change without notice. Users must explicitly opt-in to use experimental
 * features by defining NETWORK_USE_EXPERIMENTAL before including headers.
 *
 * ### Stability Levels
 * - **STABLE**: APIs in core/ and http/ directories
 * - **EXPERIMENTAL**: APIs in experimental/ directory
 *
 * ### Usage
 * To use experimental APIs, define NETWORK_USE_EXPERIMENTAL:
 * \code
 * #define NETWORK_USE_EXPERIMENTAL
 * #include "internal/experimental/quic_client.h"
 * \endcode
 *
 * Without the define, a clear compiler error will guide users.
 */

// Check if user has explicitly opted in to experimental APIs
#ifndef NETWORK_USE_EXPERIMENTAL

#if defined(__GNUC__) || defined(__clang__)
#define NETWORK_EXPERIMENTAL_STATIC_ASSERT_MSG                                                     \
	"This is an experimental API. Define NETWORK_USE_EXPERIMENTAL before including this header. " \
	"Example: #define NETWORK_USE_EXPERIMENTAL"
#else
#define NETWORK_EXPERIMENTAL_STATIC_ASSERT_MSG                                                     \
	"Experimental API - define NETWORK_USE_EXPERIMENTAL to use"
#endif

/*!
 * \def NETWORK_REQUIRE_EXPERIMENTAL
 * \brief Enforces opt-in for experimental APIs at compile time
 *
 * Place this macro at the top of experimental header files.
 * It will cause a compilation error if NETWORK_USE_EXPERIMENTAL is not defined.
 */
#define NETWORK_REQUIRE_EXPERIMENTAL                                                               \
	static_assert(false, NETWORK_EXPERIMENTAL_STATIC_ASSERT_MSG)

#else // NETWORK_USE_EXPERIMENTAL is defined

#define NETWORK_REQUIRE_EXPERIMENTAL // No-op when opted in

#endif // NETWORK_USE_EXPERIMENTAL

/*!
 * \def NETWORK_EXPERIMENTAL_API
 * \brief Marks a class or function as experimental
 *
 * Applies the [[deprecated]] attribute with a clear message indicating
 * that the API is experimental and may change without notice.
 *
 * ### Example
 * \code
 * class NETWORK_EXPERIMENTAL_API quic_client {
 *     // ...
 * };
 * \endcode
 */
#if defined(__cplusplus) && __cplusplus >= 201402L
#define NETWORK_EXPERIMENTAL_API                                                                   \
	[[deprecated("Experimental API - may change between minor versions without notice")]]
#else
#define NETWORK_EXPERIMENTAL_API
#endif

/*!
 * \def NETWORK_EXPERIMENTAL_WARNING
 * \brief Prints a compile-time warning for experimental API usage
 */
#if defined(__GNUC__) || defined(__clang__)
#define NETWORK_EXPERIMENTAL_WARNING(msg) _Pragma(#msg)
#elif defined(_MSC_VER)
#define NETWORK_EXPERIMENTAL_WARNING(msg) __pragma(message(msg))
#else
#define NETWORK_EXPERIMENTAL_WARNING(msg)
#endif

namespace kcenon::network::experimental
{
	/*!
	 * \brief Namespace for experimental APIs
	 *
	 * All classes and functions in this namespace are experimental and may
	 * change between minor versions without notice. Use at your own risk
	 * in production environments.
	 *
	 * ### Included Protocols
	 * - QUIC (RFC 9000) - Modern transport protocol
	 * - Reliable UDP - Custom reliability layer over UDP
	 *
	 * ### Migration Path
	 * When an experimental API becomes stable, it will be moved to the
	 * appropriate namespace (core:: or http::) with a compatibility alias
	 * in experimental:: that is marked as deprecated.
	 */
} // namespace kcenon::network::experimental
