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

/*!
 * \file openssl_compat.h
 * \brief OpenSSL version compatibility layer
 *
 * This header provides compatibility macros and utilities for supporting
 * both OpenSSL 1.1.x and OpenSSL 3.x APIs.
 *
 * OpenSSL Version Support:
 * - OpenSSL 1.1.1: Minimum supported version (EOL: September 11, 2023)
 * - OpenSSL 3.x: Recommended version with full support
 *
 * Note: OpenSSL 1.1.1 reached End-of-Life on September 11, 2023.
 * Users should upgrade to OpenSSL 3.x for continued security support.
 */

#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

// ============================================================================
// Version Detection Macros
// ============================================================================

/*!
 * \brief Check if OpenSSL version is 3.0.0 or newer
 *
 * OpenSSL 3.0 introduced significant API changes including:
 * - Provider-based architecture
 * - Deprecated many low-level APIs
 * - New EVP_MAC API replacing HMAC()
 */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    #define NETWORK_OPENSSL_VERSION_3_X 1
    #define NETWORK_OPENSSL_VERSION_1_1_X 0
#elif OPENSSL_VERSION_NUMBER >= 0x10101000L
    #define NETWORK_OPENSSL_VERSION_3_X 0
    #define NETWORK_OPENSSL_VERSION_1_1_X 1
#else
    #error "OpenSSL version 1.1.1 or newer is required"
#endif

// ============================================================================
// Deprecation Warning Suppression for OpenSSL 3.x
// ============================================================================

/*!
 * \brief Suppress OpenSSL 3.x deprecation warnings
 *
 * OpenSSL 3.x marks many 1.1.x APIs as deprecated but they still work.
 * We use these macros to suppress warnings when using legacy-compatible code
 * that needs to work with both versions.
 *
 * Usage:
 *   NETWORK_OPENSSL_SUPPRESS_DEPRECATED_START
 *   // Code using potentially deprecated APIs
 *   NETWORK_OPENSSL_SUPPRESS_DEPRECATED_END
 */
#if defined(__GNUC__) || defined(__clang__)
    #define NETWORK_OPENSSL_SUPPRESS_DEPRECATED_START \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
    #define NETWORK_OPENSSL_SUPPRESS_DEPRECATED_END \
        _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
    #define NETWORK_OPENSSL_SUPPRESS_DEPRECATED_START \
        __pragma(warning(push)) \
        __pragma(warning(disable: 4996))
    #define NETWORK_OPENSSL_SUPPRESS_DEPRECATED_END \
        __pragma(warning(pop))
#else
    #define NETWORK_OPENSSL_SUPPRESS_DEPRECATED_START
    #define NETWORK_OPENSSL_SUPPRESS_DEPRECATED_END
#endif

// ============================================================================
// API Compatibility Utilities
// ============================================================================

namespace network_system::internal
{

/*!
 * \brief Get OpenSSL version string
 * \return Human-readable OpenSSL version string
 */
inline const char* openssl_version_string() noexcept
{
#if NETWORK_OPENSSL_VERSION_3_X
    return OpenSSL_version(OPENSSL_VERSION);
#else
    return OpenSSL_version(OPENSSL_VERSION);
#endif
}

/*!
 * \brief Check if running on OpenSSL 3.x
 * \return true if OpenSSL 3.x, false otherwise
 */
constexpr bool is_openssl_3x() noexcept
{
#if NETWORK_OPENSSL_VERSION_3_X
    return true;
#else
    return false;
#endif
}

/*!
 * \brief Check if running on deprecated OpenSSL 1.1.x
 * \return true if OpenSSL 1.1.x (EOL), false otherwise
 */
constexpr bool is_openssl_eol() noexcept
{
#if NETWORK_OPENSSL_VERSION_1_1_X
    return true;
#else
    return false;
#endif
}

/*!
 * \brief Get last OpenSSL error as string
 * \return Human-readable error message
 *
 * This function works consistently across OpenSSL versions.
 */
inline std::string get_openssl_error() noexcept
{
    unsigned long err = ERR_get_error();
    if (err == 0)
    {
        return "No OpenSSL error";
    }
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}

/*!
 * \brief Clear all OpenSSL errors from the thread's error queue
 *
 * This should be called before operations where you want to check
 * for fresh errors.
 */
inline void clear_openssl_errors() noexcept
{
    ERR_clear_error();
}

} // namespace network_system::internal

// ============================================================================
// OpenSSL 3.x Provider Notes
// ============================================================================

/*!
 * \note OpenSSL 3.x Provider Architecture
 *
 * OpenSSL 3.0 introduced a provider-based architecture where cryptographic
 * implementations are loaded from providers. The default provider includes
 * all common algorithms.
 *
 * Key differences from 1.1.x:
 *
 * 1. Providers:
 *    - default: Standard cryptographic algorithms
 *    - legacy: Deprecated algorithms (MD4, RC4, DES, etc.)
 *    - fips: FIPS 140-2 validated algorithms
 *
 * 2. API Changes:
 *    - HMAC() function deprecated -> Use EVP_MAC API
 *    - Low-level cipher APIs deprecated -> Use EVP_Cipher APIs
 *    - Engine API deprecated -> Use Provider API
 *
 * 3. Our Approach:
 *    - Use EVP-based APIs which work on both versions
 *    - Avoid deprecated low-level APIs
 *    - No special provider loading needed for standard operations
 *
 * The code in this library uses EVP-based APIs throughout, which provides
 * seamless compatibility between OpenSSL 1.1.x and 3.x without requiring
 * any runtime detection or conditional code paths.
 */
