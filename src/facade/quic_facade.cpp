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

auto quic_facade::validate_client_config(const client_config& config) -> void
{
	if (config.host.empty())
	{
		throw std::invalid_argument("quic_facade: host cannot be empty");
	}

	if (config.port == 0 || config.port > 65535)
	{
		throw std::invalid_argument("quic_facade: port must be between 1 and 65535");
	}

	if (config.max_idle_timeout_ms == 0)
	{
		throw std::invalid_argument("quic_facade: max_idle_timeout_ms must be positive");
	}
}

auto quic_facade::validate_server_config(const server_config& config) -> void
{
	if (config.port == 0 || config.port > 65535)
	{
		throw std::invalid_argument("quic_facade: port must be between 1 and 65535");
	}

	if (config.cert_path.empty())
	{
		throw std::invalid_argument("quic_facade: cert_path cannot be empty");
	}

	if (config.key_path.empty())
	{
		throw std::invalid_argument("quic_facade: key_path cannot be empty");
	}

	if (config.max_idle_timeout_ms == 0)
	{
		throw std::invalid_argument("quic_facade: max_idle_timeout_ms must be positive");
	}

	if (config.max_connections == 0)
	{
		throw std::invalid_argument("quic_facade: max_connections must be positive");
	}
}

auto quic_facade::create_client(const client_config& config) const
	-> std::shared_ptr<interfaces::i_protocol_client>
{
	validate_client_config(config);

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

	return adapter;
}

auto quic_facade::create_server(const server_config& config) const
	-> std::shared_ptr<interfaces::i_protocol_server>
{
	validate_server_config(config);

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

	return adapter;
}

} // namespace kcenon::network::facade
