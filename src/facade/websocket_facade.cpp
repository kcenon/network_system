// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/network/facade/websocket_facade.h"

#include <atomic>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "internal/adapters/ws_client_adapter.h"
#include "internal/adapters/ws_server_adapter.h"

namespace kcenon::network::facade
{

namespace
{
	//! \brief Atomic counter for generating unique client IDs
	std::atomic<uint64_t> g_client_id_counter{0};

	//! \brief Atomic counter for generating unique server IDs
	std::atomic<uint64_t> g_server_id_counter{0};
} // namespace

auto websocket_facade::generate_client_id() -> std::string
{
	const auto id = g_client_id_counter.fetch_add(1, std::memory_order_relaxed);
	std::ostringstream oss;
	oss << "ws_client_" << std::setfill('0') << std::setw(8) << id;
	return oss.str();
}

auto websocket_facade::generate_server_id() -> std::string
{
	const auto id = g_server_id_counter.fetch_add(1, std::memory_order_relaxed);
	std::ostringstream oss;
	oss << "ws_server_" << std::setfill('0') << std::setw(8) << id;
	return oss.str();
}

auto websocket_facade::validate_client_config(const client_config& config) -> void
{
	if (config.ping_interval.count() <= 0)
	{
		throw std::invalid_argument("websocket_facade: ping_interval must be positive");
	}
}

auto websocket_facade::validate_server_config(const server_config& config) -> void
{
	if (config.port == 0 || config.port > 65535)
	{
		throw std::invalid_argument("websocket_facade: port must be between 1 and 65535");
	}

	if (config.path.empty() || config.path[0] != '/')
	{
		throw std::invalid_argument("websocket_facade: path must start with '/'");
	}
}

auto websocket_facade::create_client(const client_config& config) const
	-> std::shared_ptr<interfaces::i_protocol_client>
{
	validate_client_config(config);

	const auto client_id = config.client_id.empty() ? generate_client_id() : config.client_id;

	// Create adapter with ping interval from config
	// Note: Path must be set by caller via dynamic_cast if non-default path needed
	return std::make_shared<internal::adapters::ws_client_adapter>(
		client_id, config.ping_interval);
}

auto websocket_facade::create_server(const server_config& config) const
	-> std::shared_ptr<interfaces::i_protocol_server>
{
	validate_server_config(config);

	const auto server_id = config.server_id.empty() ? generate_server_id() : config.server_id;

	// Create adapter and configure path
	auto adapter = std::make_shared<internal::adapters::ws_server_adapter>(server_id);
	adapter->set_path(config.path);

	return adapter;
}

} // namespace kcenon::network::facade
