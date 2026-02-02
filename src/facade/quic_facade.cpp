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

// Enable experimental API for QUIC
#ifndef NETWORK_USE_EXPERIMENTAL
#define NETWORK_USE_EXPERIMENTAL
#endif

#include "kcenon/network/experimental/quic_client.h"
#include "kcenon/network/experimental/quic_server.h"

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
	-> std::shared_ptr<core::messaging_quic_client>
{
	validate_client_config(config);

	const auto client_id = config.client_id.empty() ? generate_client_id() : config.client_id;

	return std::make_shared<core::messaging_quic_client>(client_id);
}

auto quic_facade::create_server(const server_config& config) const
	-> std::shared_ptr<core::messaging_quic_server>
{
	validate_server_config(config);

	const auto server_id = config.server_id.empty() ? generate_server_id() : config.server_id;

	return std::make_shared<core::messaging_quic_server>(server_id);
}

} // namespace kcenon::network::facade
