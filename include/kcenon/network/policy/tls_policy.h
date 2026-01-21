/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#include <concepts>
#include <string>
#include <type_traits>

namespace kcenon::network::policy {

/*!
 * \struct no_tls
 * \brief Policy type indicating no TLS/SSL encryption.
 *
 * This policy is used as a template parameter to indicate that
 * plain-text communication should be used without encryption.
 *
 * ### Usage
 * \code
 * messaging_client<tcp_protocol, no_tls> plain_client;
 * \endcode
 */
struct no_tls {
    static constexpr bool enabled = false;
};

/*!
 * \struct tls_enabled
 * \brief Policy type indicating TLS/SSL encryption is enabled.
 *
 * This policy carries configuration for TLS connections including
 * certificate paths and verification settings.
 *
 * ### Usage
 * \code
 * tls_enabled tls_config{
 *     .cert_path = "/path/to/cert.pem",
 *     .key_path = "/path/to/key.pem",
 *     .ca_path = "/path/to/ca.pem",
 *     .verify_peer = true
 * };
 * messaging_client<tcp_protocol, tls_enabled> secure_client;
 * \endcode
 */
struct tls_enabled {
    static constexpr bool enabled = true;

    std::string cert_path{};
    std::string key_path{};
    std::string ca_path{};
    bool verify_peer{true};
};

/*!
 * \concept TlsPolicy
 * \brief Concept that constrains types to be valid TLS policies.
 *
 * A valid TLS policy must have a static constexpr bool member `enabled`
 * that indicates whether TLS is active.
 *
 * ### Satisfied Types
 * - `no_tls` - TLS disabled
 * - `tls_enabled` - TLS enabled with configuration
 *
 * ### Usage
 * \code
 * template<TlsPolicy Policy>
 * class connection_handler { ... };
 * \endcode
 */
template <typename T>
concept TlsPolicy = requires {
    { T::enabled } -> std::convertible_to<bool>;
};

/*!
 * \brief Helper variable template to check if TLS is enabled at compile time.
 */
template <TlsPolicy Policy>
inline constexpr bool is_tls_enabled_v = Policy::enabled;

/*!
 * \brief Type trait to check if a policy enables TLS.
 */
template <TlsPolicy Policy>
struct is_tls_enabled : std::bool_constant<Policy::enabled> {};

}  // namespace kcenon::network::policy
