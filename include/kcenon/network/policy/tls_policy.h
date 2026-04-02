// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
 * - `no_tls` - Plain-text communication (no TLS)
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
