// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/network/facade/tcp_facade.h"

#include <atomic>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "internal/core/connection_pool.h"
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

auto tcp_facade::validate_client_config(const client_config& config) -> VoidResult
{
	if (config.host.empty())
	{
		return error_void(-1, "tcp_facade: host cannot be empty", "tcp_facade");
	}

	if (config.port == 0 || config.port > 65535)
	{
		return error_void(-1, "tcp_facade: port must be between 1 and 65535", "tcp_facade");
	}

	if (config.timeout.count() <= 0)
	{
		return error_void(-1, "tcp_facade: timeout must be positive", "tcp_facade");
	}

	return ok();
}

auto tcp_facade::validate_server_config(const server_config& config) -> VoidResult
{
	if (config.port == 0 || config.port > 65535)
	{
		return error_void(-1, "tcp_facade: port must be between 1 and 65535", "tcp_facade");
	}

	if (config.use_ssl)
	{
		if (!config.cert_path.has_value() || config.cert_path->empty())
		{
			return error_void(-1, "tcp_facade: cert_path required when use_ssl=true", "tcp_facade");
		}

		if (!config.key_path.has_value() || config.key_path->empty())
		{
			return error_void(-1, "tcp_facade: key_path required when use_ssl=true", "tcp_facade");
		}
	}

	return ok();
}

auto tcp_facade::create_client(const client_config& config) const
	-> Result<std::shared_ptr<interfaces::i_protocol_client>>
{
	// Validate configuration
	auto validation = validate_client_config(config);
	if (validation.is_err())
	{
		return error<std::shared_ptr<interfaces::i_protocol_client>>(
			validation.error().code, validation.error().message, "tcp_facade");
	}

	// Generate client ID if not provided
	const std::string client_id = config.client_id.empty() ? generate_client_id() : config.client_id;

	// SSL support not yet implemented
	if (config.use_ssl)
	{
		return error<std::shared_ptr<interfaces::i_protocol_client>>(
			-6, "tcp_facade: SSL/TLS support not yet implemented", "tcp_facade");
	}

	auto client = std::make_shared<core::messaging_client>(client_id);

	// Start the client and connect
	auto result = client->start(config.host, config.port);
	if (result.is_err())
	{
		return error<std::shared_ptr<interfaces::i_protocol_client>>(
			-600, "tcp_facade: failed to start client: " + result.error().message, "tcp_facade");
	}

	return ok(std::shared_ptr<interfaces::i_protocol_client>(client));
}

auto tcp_facade::create_server(const server_config& config) const
	-> Result<std::shared_ptr<interfaces::i_protocol_server>>
{
	// Validate configuration
	auto validation = validate_server_config(config);
	if (validation.is_err())
	{
		return error<std::shared_ptr<interfaces::i_protocol_server>>(
			validation.error().code, validation.error().message, "tcp_facade");
	}

	// Generate server ID if not provided
	const std::string server_id = config.server_id.empty() ? generate_server_id() : config.server_id;

	// SSL support not yet implemented
	if (config.use_ssl)
	{
		return error<std::shared_ptr<interfaces::i_protocol_server>>(
			-6, "tcp_facade: SSL/TLS support not yet implemented", "tcp_facade");
	}

	// Create adapter wrapping messaging_server
	auto server = std::make_shared<internal::adapters::tcp_server_adapter>(server_id);

	return ok(std::shared_ptr<interfaces::i_protocol_server>(server));
}

auto tcp_facade::create_connection_pool(const pool_config& config) const
	-> Result<std::shared_ptr<core::connection_pool>>
{
	if (config.host.empty())
	{
		return error<std::shared_ptr<core::connection_pool>>(
			-1, "tcp_facade: host cannot be empty", "tcp_facade");
	}

	if (config.port == 0 || config.port > 65535)
	{
		return error<std::shared_ptr<core::connection_pool>>(
			-1, "tcp_facade: port must be between 1 and 65535", "tcp_facade");
	}

	if (config.pool_size == 0)
	{
		return error<std::shared_ptr<core::connection_pool>>(
			-1, "tcp_facade: pool_size must be greater than 0", "tcp_facade");
	}

	return ok(std::make_shared<core::connection_pool>(
		config.host,
		config.port,
		config.pool_size,
		config.acquire_timeout));
}

} // namespace kcenon::network::facade
