// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/network/facade/udp_facade.h"

#include <atomic>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "internal/adapters/udp_client_adapter.h"
#include "internal/adapters/udp_server_adapter.h"

namespace kcenon::network::facade
{

namespace
{
	//! \brief Atomic counter for generating unique client IDs
	std::atomic<uint64_t> g_client_id_counter{0};

	//! \brief Atomic counter for generating unique server IDs
	std::atomic<uint64_t> g_server_id_counter{0};
} // namespace

auto udp_facade::generate_client_id() -> std::string
{
	const auto id = g_client_id_counter.fetch_add(1, std::memory_order_relaxed);
	std::ostringstream oss;
	oss << "udp_client_" << std::setfill('0') << std::setw(8) << id;
	return oss.str();
}

auto udp_facade::generate_server_id() -> std::string
{
	const auto id = g_server_id_counter.fetch_add(1, std::memory_order_relaxed);
	std::ostringstream oss;
	oss << "udp_server_" << std::setfill('0') << std::setw(8) << id;
	return oss.str();
}

auto udp_facade::validate_client_config(const client_config& config) -> VoidResult
{
	if (config.host.empty())
	{
		return error_void(-1, "udp_facade: host cannot be empty", "udp_facade");
	}

	if (config.port == 0 || config.port > 65535)
	{
		return error_void(-1, "udp_facade: port must be between 1 and 65535", "udp_facade");
	}

	return ok();
}

auto udp_facade::validate_server_config(const server_config& config) -> VoidResult
{
	if (config.port == 0 || config.port > 65535)
	{
		return error_void(-1, "udp_facade: port must be between 1 and 65535", "udp_facade");
	}

	return ok();
}

auto udp_facade::create_client(const client_config& config) const
	-> Result<std::shared_ptr<interfaces::i_protocol_client>>
{
	// Validate configuration
	auto validation = validate_client_config(config);
	if (validation.is_err())
	{
		return error<std::shared_ptr<interfaces::i_protocol_client>>(
			validation.error().code, validation.error().message, "udp_facade");
	}

	// Generate client ID if not provided
	const std::string client_id = config.client_id.empty() ? generate_client_id() : config.client_id;

	// Create UDP client adapter that wraps messaging_udp_client
	auto client = std::make_shared<internal::adapters::udp_client_adapter>(client_id);

	// Start the client and connect to target
	auto result = client->start(config.host, config.port);
	if (result.is_err())
	{
		return error<std::shared_ptr<interfaces::i_protocol_client>>(
			-600, "udp_facade: failed to start client: " + result.error().message, "udp_facade");
	}

	return ok(std::shared_ptr<interfaces::i_protocol_client>(client));
}

auto udp_facade::create_server(const server_config& config) const
	-> Result<std::shared_ptr<interfaces::i_protocol_server>>
{
	// Validate configuration
	auto validation = validate_server_config(config);
	if (validation.is_err())
	{
		return error<std::shared_ptr<interfaces::i_protocol_server>>(
			validation.error().code, validation.error().message, "udp_facade");
	}

	// Generate server ID if not provided
	const std::string server_id = config.server_id.empty() ? generate_server_id() : config.server_id;

	// Create UDP server adapter that wraps messaging_udp_server
	auto server = std::make_shared<internal::adapters::udp_server_adapter>(server_id);

	// Start the server
	auto result = server->start(config.port);
	if (result.is_err())
	{
		return error<std::shared_ptr<interfaces::i_protocol_server>>(
			-660, "udp_facade: failed to start server: " + result.error().message, "udp_facade");
	}

	return ok(std::shared_ptr<interfaces::i_protocol_server>(server));
}

} // namespace kcenon::network::facade
