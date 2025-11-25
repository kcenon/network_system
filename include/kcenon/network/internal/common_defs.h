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

#include <cstdint>
#include <string_view>
#include <string>
#include <optional>

// Use nested namespace definition (C++17)
namespace network_system::internal
{
	/*!
	 * \enum data_mode
	 * \brief Represents a simple enumeration for differentiating data
	 * transmission modes.
	 *
	 * A higher-level code might use these to switch between packet-based,
	 * file-based, or binary data logic. They are optional stubs and can be
	 * extended as needed.
	 */
	enum class data_mode : std::uint8_t {
		packet_mode = 1, /*!< Regular messaging/packet mode. */
		file_mode = 2,	 /*!< File transfer mode. */
		binary_mode = 3	 /*!< Raw binary data mode. */
	};

	/*!
	 * \enum tls_version
	 * \brief TLS protocol versions
	 *
	 * Specifies which TLS version to use for secure connections.
	 * Modern applications should use TLS 1.2 or 1.3.
	 */
	enum class tls_version : std::uint8_t {
		tls_1_0 = 10,  /*!< TLS 1.0 (deprecated, insecure) */
		tls_1_1 = 11,  /*!< TLS 1.1 (deprecated, insecure) */
		tls_1_2 = 12,  /*!< TLS 1.2 (secure, widely supported) */
		tls_1_3 = 13   /*!< TLS 1.3 (most secure, recommended) */
	};

	/*!
	 * \enum certificate_verification
	 * \brief Certificate verification modes
	 */
	enum class certificate_verification : std::uint8_t {
		none = 0,           /*!< No verification (insecure, testing only) */
		verify_peer = 1,    /*!< Verify peer certificate */
		verify_fail_if_no_peer_cert = 2  /*!< Fail if peer doesn't provide cert */
	};

	/*!
	 * \struct tls_config
	 * \brief Configuration for TLS/SSL connections
	 *
	 * This structure provides all necessary configuration for establishing
	 * secure TLS connections using ASIO SSL support.
	 *
	 * ### Example Usage (Server)
	 * \code
	 * tls_config server_tls;
	 * server_tls.enabled = true;
	 * server_tls.min_version = tls_version::tls_1_2;
	 * server_tls.certificate_file = "/path/to/server.crt";
	 * server_tls.private_key_file = "/path/to/server.key";
	 * server_tls.verify_mode = certificate_verification::verify_peer;
	 * server_tls.ca_file = "/path/to/ca.crt";
	 * \endcode
	 *
	 * ### Example Usage (Client)
	 * \code
	 * tls_config client_tls;
	 * client_tls.enabled = true;
	 * client_tls.min_version = tls_version::tls_1_2;
	 * client_tls.verify_mode = certificate_verification::verify_peer;
	 * client_tls.ca_file = "/path/to/ca.crt";
	 * \endcode
	 *
	 * ### Security Notes
	 * - Always use TLS 1.2 or 1.3 in production
	 * - Always verify peer certificates in production
	 * - Protect private key files with appropriate file permissions
	 * - Use strong cipher suites (configured via cipher_list)
	 */
	struct tls_config {
		/// Enable TLS/SSL for this connection (default: false)
		bool enabled = false;

		/// Minimum TLS version to accept (default: TLS 1.3)
		/// Note: TLS 1.3 is enforced by default to prevent downgrade attacks (TICKET-009)
		tls_version min_version = tls_version::tls_1_3;

		/// Certificate verification mode (default: verify_peer)
		certificate_verification verify_mode = certificate_verification::verify_peer;

		/// Path to server certificate file (PEM format)
		/// Required for servers when TLS is enabled
		std::optional<std::string> certificate_file;

		/// Path to server private key file (PEM format)
		/// Required for servers when TLS is enabled
		std::optional<std::string> private_key_file;

		/// Password for encrypted private key (if applicable)
		std::optional<std::string> private_key_password;

		/// Path to CA certificate file for verification (PEM format)
		/// Required when verify_mode != none
		std::optional<std::string> ca_file;

		/// Path to directory containing CA certificates
		std::optional<std::string> ca_path;

		/// Cipher suite list (OpenSSL format)
		/// Default: Use strong ciphers (TLS 1.2+)
		/// Example: "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256"
		std::optional<std::string> cipher_list;

		/// Server Name Indication (SNI) hostname for clients
		/// Used for virtual hosting and certificate selection
		std::optional<std::string> sni_hostname;

		/// Enable session resumption for performance
		bool enable_session_resumption = true;

		/// Timeout for TLS handshake in milliseconds
		std::size_t handshake_timeout_ms = 10000;

		/*!
		 * \brief Validates the TLS configuration
		 * \return true if configuration is valid, false otherwise
		 *
		 * Checks that required files are specified for the chosen mode.
		 */
		[[nodiscard]] auto is_valid() const -> bool {
			if (!enabled) {
				return true;  // Valid if disabled
			}

			// If verification is enabled, CA file/path is required
			if (verify_mode != certificate_verification::none) {
				if (!ca_file.has_value() && !ca_path.has_value()) {
					return false;
				}
			}

			// Note: Certificate and private key validation depends on whether
			// this is a server or client configuration, which is context-dependent.
			// Server-specific validation should be done by the server class.

			return true;
		}

		/*!
		 * \brief Creates a default insecure configuration (testing only)
		 * \return TLS config with verification disabled
		 *
		 * WARNING: This configuration is INSECURE and should only be used
		 * for development and testing. Never use in production!
		 */
		[[nodiscard]] static auto insecure_for_testing() -> tls_config {
			tls_config config;
			config.enabled = true;
			config.verify_mode = certificate_verification::none;
			return config;
		}

		/*!
		 * \brief Creates a secure default configuration
		 * \return TLS config with secure defaults (TLS 1.3 minimum)
		 *
		 * You must still set certificate/key files and CA certificates.
		 * Uses TLS 1.3 by default to prevent protocol downgrade attacks.
		 */
		[[nodiscard]] static auto secure_defaults() -> tls_config {
			tls_config config;
			config.enabled = true;
			config.min_version = tls_version::tls_1_3;
			config.verify_mode = certificate_verification::verify_peer;
			config.enable_session_resumption = true;
			return config;
		}

		/*!
		 * \brief Creates a backwards-compatible configuration (TLS 1.2+)
		 * \return TLS config allowing TLS 1.2 for legacy client compatibility
		 *
		 * WARNING: This allows TLS 1.2 which may be vulnerable to downgrade attacks.
		 * Use only when TLS 1.3 is not supported by all clients.
		 */
		[[nodiscard]] static auto legacy_compatible() -> tls_config {
			tls_config config;
			config.enabled = true;
			config.min_version = tls_version::tls_1_2;
			config.verify_mode = certificate_verification::verify_peer;
			config.enable_session_resumption = true;
			return config;
		}
	};

	// Use inline variables for constants (C++17)
	inline constexpr std::size_t default_buffer_size = 4096;
	inline constexpr std::size_t default_timeout_ms = 5000;
	inline constexpr std::string_view default_client_id = "default_client";
	inline constexpr std::string_view default_server_id = "default_server";

	// TLS defaults
	inline constexpr std::string_view default_tls_cipher_list =
		"ECDHE-RSA-AES256-GCM-SHA384:"
		"ECDHE-RSA-AES128-GCM-SHA256:"
		"ECDHE-RSA-CHACHA20-POLY1305";

} // namespace network_system::internal