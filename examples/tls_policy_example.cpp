/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file tls_policy_example.cpp
 * @brief Demonstrates TLS policy configuration for secure connections.
 * @example tls_policy_example.cpp
 *
 * @par Category
 * Security - TLS policy configuration
 *
 * Demonstrates:
 * - no_tls policy for plain-text connections
 * - tls_enabled policy with certificate configuration
 * - Compile-time TLS policy selection
 *
 * @see kcenon::network::policy::no_tls
 * @see kcenon::network::policy::tls_enabled
 */

#include <kcenon/network/policy/tls_policy.h>

#include <iostream>
#include <string>

using namespace kcenon::network;

// Compile-time TLS policy demonstration
template <typename TlsPolicy>
void describe_policy(const std::string& label)
{
	std::cout << "   " << label << ": TLS "
			  << (TlsPolicy::enabled ? "enabled" : "disabled") << std::endl;
}

int main()
{
	std::cout << "=== TLS Policy Example ===" << std::endl;

	// 1. No TLS (plain-text)
	std::cout << "\n1. Compile-time policy inspection:" << std::endl;
	describe_policy<policy::no_tls>("no_tls");
	describe_policy<policy::tls_enabled>("tls_enabled");

	// 2. TLS configuration
	std::cout << "\n2. TLS enabled configuration:" << std::endl;
	policy::tls_enabled tls_config;
	tls_config.cert_path = "/etc/ssl/certs/server.crt";
	tls_config.key_path = "/etc/ssl/private/server.key";
	tls_config.ca_path = "/etc/ssl/certs/ca-bundle.crt";
	tls_config.verify_peer = true;

	std::cout << "   Certificate: " << tls_config.cert_path << std::endl;
	std::cout << "   Private key: " << tls_config.key_path << std::endl;
	std::cout << "   CA bundle: " << tls_config.ca_path << std::endl;
	std::cout << "   Verify peer: " << (tls_config.verify_peer ? "yes" : "no") << std::endl;

	// 3. Static assertions
	std::cout << "\n3. Compile-time checks:" << std::endl;
	static_assert(!policy::no_tls::enabled, "no_tls must have enabled=false");
	static_assert(policy::tls_enabled::enabled, "tls_enabled must have enabled=true");
	std::cout << "   All static assertions passed" << std::endl;

	std::cout << "\nDone." << std::endl;
	return 0;
}
