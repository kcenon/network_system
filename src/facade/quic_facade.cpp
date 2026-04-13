// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/network/facade/quic_facade.h"

#include <atomic>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "internal/adapters/quic_client_adapter.h"
#include "internal/adapters/quic_server_adapter.h"

namespace kcenon::network::facade
{

namespace
{
	//! \brief Atomic counter for generating unique client IDs
	std::atomic<uint64_t> g_client_id_counter{0};

	//! \brief Atomic counter for generating unique server IDs
	std::atomic<uint64_t> g_server_id_counter{0};
} // namespace

auto quic_facade::generate_client_id() -> std::string
{
	const auto id = g_client_id_counter.fetch_add(1, std::memory_order_relaxed);
	std::ostringstream oss;
	oss << "quic_client_" << std::setfill('0') << std::setw(8) << id;
	return oss.str();
}

auto quic_facade::generate_server_id() -> std::string
{
	const auto id = g_server_id_counter.fetch_add(1, std::memory_order_relaxed);
	std::ostringstream oss;
	oss << "quic_server_" << std::setfill('0') << std::setw(8) << id;
	return oss.str();
}

auto quic_facade::validate_client_config(const client_config& config) -> VoidResult
{
	if (config.host.empty())
	{
		return error_void(-1, "quic_facade: host cannot be empty", "quic_facade");
	}

	if (config.port == 0 || config.port > 65535)
	{
		return error_void(-1, "quic_facade: port must be between 1 and 65535", "quic_facade");
	}

	if (config.max_idle_timeout_ms == 0)
	{
		return error_void(-1, "quic_facade: max_idle_timeout_ms must be positive", "quic_facade");
	}

	return ok();
}

auto quic_facade::validate_server_config(const server_config& config) -> VoidResult
{
	if (config.port == 0 || config.port > 65535)
	{
		return error_void(-1, "quic_facade: port must be between 1 and 65535", "quic_facade");
	}

	if (config.cert_path.empty())
	{
		return error_void(-1, "quic_facade: cert_path cannot be empty", "quic_facade");
	}

	if (config.key_path.empty())
	{
		return error_void(-1, "quic_facade: key_path cannot be empty", "quic_facade");
	}

	if (config.max_idle_timeout_ms == 0)
	{
		return error_void(-1, "quic_facade: max_idle_timeout_ms must be positive", "quic_facade");
	}

	if (config.max_connections == 0)
	{
		return error_void(-1, "quic_facade: max_connections must be positive", "quic_facade");
	}

	return ok();
}

auto quic_facade::create_client(const client_config& config) const
	-> Result<std::shared_ptr<interfaces::i_protocol_client>>
{
	auto validation = validate_client_config(config);
	if (validation.is_err())
	{
		return error<std::shared_ptr<interfaces::i_protocol_client>>(
			validation.error().code, validation.error().message, "quic_facade");
	}

	const auto client_id = config.client_id.empty() ? generate_client_id() : config.client_id;

	auto adapter = std::make_shared<internal::adapters::quic_client_adapter>(client_id);

	// Configure QUIC-specific options
	if (!config.alpn.empty())
	{
		adapter->set_alpn_protocols({config.alpn});
	}

	if (config.ca_cert_path)
	{
		adapter->set_ca_cert_path(*config.ca_cert_path);
	}

	if (config.client_cert_path && config.client_key_path)
	{
		adapter->set_client_cert(*config.client_cert_path, *config.client_key_path);
	}

	adapter->set_verify_server(config.verify_server);
	adapter->set_max_idle_timeout(config.max_idle_timeout_ms);

	return ok(std::shared_ptr<interfaces::i_protocol_client>(adapter));
}

auto quic_facade::create_server(const server_config& config) const
	-> Result<std::shared_ptr<interfaces::i_protocol_server>>
{
	auto validation = validate_server_config(config);
	if (validation.is_err())
	{
		return error<std::shared_ptr<interfaces::i_protocol_server>>(
			validation.error().code, validation.error().message, "quic_facade");
	}

	const auto server_id = config.server_id.empty() ? generate_server_id() : config.server_id;

	auto adapter = std::make_shared<internal::adapters::quic_server_adapter>(server_id);

	// Configure TLS certificates (required for QUIC)
	adapter->set_cert_path(config.cert_path);
	adapter->set_key_path(config.key_path);

	// Configure QUIC-specific options
	if (!config.alpn.empty())
	{
		adapter->set_alpn_protocols({config.alpn});
	}

	if (config.ca_cert_path)
	{
		adapter->set_ca_cert_path(*config.ca_cert_path);
	}

	adapter->set_require_client_cert(config.require_client_cert);
	adapter->set_max_idle_timeout(config.max_idle_timeout_ms);
	adapter->set_max_connections(config.max_connections);

	return ok(std::shared_ptr<interfaces::i_protocol_server>(adapter));
}

} // namespace kcenon::network::facade
