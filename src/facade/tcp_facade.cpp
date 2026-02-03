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

#include "kcenon/network/facade/tcp_facade.h"

#include <atomic>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "internal/core/messaging_client.h"
#include "internal/adapters/tcp_server_adapter.h"
// SSL implementations will be added after they implement the protocol interfaces
// #include "internal/core/secure_messaging_client.h"
// #include "internal/core/secure_messaging_server.h"

namespace kcenon::network::facade
{

namespace
{
	//! \brief Atomic counter for generating unique client IDs
	std::atomic<uint64_t> g_client_id_counter{0};

	//! \brief Atomic counter for generating unique server IDs
	std::atomic<uint64_t> g_server_id_counter{0};
} // namespace

auto tcp_facade::generate_client_id() -> std::string
{
	const auto id = g_client_id_counter.fetch_add(1, std::memory_order_relaxed);
	std::ostringstream oss;
	oss << "tcp_client_" << std::setfill('0') << std::setw(8) << id;
	return oss.str();
}

auto tcp_facade::generate_server_id() -> std::string
{
	const auto id = g_server_id_counter.fetch_add(1, std::memory_order_relaxed);
	std::ostringstream oss;
	oss << "tcp_server_" << std::setfill('0') << std::setw(8) << id;
	return oss.str();
}

auto tcp_facade::validate_client_config(const client_config& config) -> void
{
	if (config.host.empty())
	{
		throw std::invalid_argument("tcp_facade: host cannot be empty");
	}

	if (config.port == 0 || config.port > 65535)
	{
		throw std::invalid_argument("tcp_facade: port must be between 1 and 65535");
	}

	if (config.timeout.count() <= 0)
	{
		throw std::invalid_argument("tcp_facade: timeout must be positive");
	}
}

auto tcp_facade::validate_server_config(const server_config& config) -> void
{
	if (config.port == 0 || config.port > 65535)
	{
		throw std::invalid_argument("tcp_facade: port must be between 1 and 65535");
	}

	if (config.use_ssl)
	{
		if (!config.cert_path.has_value() || config.cert_path->empty())
		{
			throw std::invalid_argument("tcp_facade: cert_path required when use_ssl=true");
		}

		if (!config.key_path.has_value() || config.key_path->empty())
		{
			throw std::invalid_argument("tcp_facade: key_path required when use_ssl=true");
		}
	}
}

auto tcp_facade::create_client(const client_config& config) const
	-> std::shared_ptr<interfaces::i_protocol_client>
{
	// Validate configuration
	validate_client_config(config);

	// Generate client ID if not provided
	const std::string client_id = config.client_id.empty() ? generate_client_id() : config.client_id;

	// Create appropriate client type based on SSL flag
	std::shared_ptr<interfaces::i_protocol_client> client;

	// Create plain messaging client
	// Note: SSL support will be added after secure_messaging_client implements IProtocolClient
	if (config.use_ssl)
	{
		throw std::runtime_error(
			"tcp_facade: SSL/TLS support not yet implemented - "
			"requires secure_messaging_client to implement IProtocolClient interface");
	}

	client = std::make_shared<core::messaging_client>(client_id);

	// Start the client and connect
	auto result = client->start(config.host, config.port);
	if (result.is_err())
	{
		throw std::runtime_error("tcp_facade: failed to start client: " + result.error().message);
	}

	return client;
}

auto tcp_facade::create_server(const server_config& config) const
	-> std::shared_ptr<interfaces::i_protocol_server>
{
	// Validate configuration
	validate_server_config(config);

	// Generate server ID if not provided
	const std::string server_id = config.server_id.empty() ? generate_server_id() : config.server_id;

	// Create appropriate server type based on SSL flag
	std::shared_ptr<interfaces::i_protocol_server> server;

	if (config.use_ssl)
	{
		// Note: SSL support will be added after secure_messaging_server implements IProtocolServer
		throw std::runtime_error(
			"tcp_facade: SSL/TLS support not yet implemented - "
			"requires secure_messaging_server to implement IProtocolServer interface");
	}

	// Create adapter wrapping messaging_server
	server = std::make_shared<internal::adapters::tcp_server_adapter>(server_id);

	return server;
}

} // namespace kcenon::network::facade
