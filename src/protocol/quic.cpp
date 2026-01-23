/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
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

#include "kcenon/network/protocol/quic.h"
#include "kcenon/network/unified/adapters/quic_connection_adapter.h"
#include "kcenon/network/unified/adapters/quic_listener_adapter.h"

#include <atomic>
#include <chrono>
#include <sstream>

namespace kcenon::network::protocol::quic {

namespace {

/**
 * @brief Generates a unique ID if none is provided
 */
auto generate_unique_id(std::string_view prefix) -> std::string
{
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto count = counter.fetch_add(1);

    std::ostringstream oss;
    oss << prefix << "-" << now << "-" << count;
    return oss.str();
}

/**
 * @brief Parses URL to extract host and port
 */
auto parse_url(std::string_view url) -> std::pair<std::string, uint16_t>
{
    std::string url_str(url);

    // Remove quic:// prefix if present
    const std::string quic_prefix = "quic://";
    if (url_str.substr(0, quic_prefix.size()) == quic_prefix) {
        url_str = url_str.substr(quic_prefix.size());
    }

    // Find the last colon for port separation
    auto colon_pos = url_str.rfind(':');
    if (colon_pos == std::string::npos) {
        return {"", 0};
    }

    std::string host = url_str.substr(0, colon_pos);
    std::string port_str = url_str.substr(colon_pos + 1);

    uint16_t port = 0;
    try {
        port = static_cast<uint16_t>(std::stoi(port_str));
    } catch (...) {
        return {"", 0};
    }

    return {host, port};
}

/**
 * @brief Extracts server name from URL for SNI
 */
auto extract_server_name(std::string_view url) -> std::string
{
    auto [host, port] = parse_url(url);
    return host;
}

}  // namespace

auto create_connection(const quic_config& config, std::string_view id)
    -> std::unique_ptr<unified::i_connection>
{
    std::string connection_id = id.empty()
        ? generate_unique_id("quic-conn")
        : std::string(id);

    return std::make_unique<unified::adapters::quic_connection_adapter>(
        config, connection_id);
}

auto connect(const unified::endpoint_info& endpoint,
             const quic_config& config,
             std::string_view id) -> std::unique_ptr<unified::i_connection>
{
    // Ensure server_name is set for TLS
    quic_config cfg = config;
    if (cfg.server_name.empty()) {
        cfg.server_name = endpoint.host;
    }

    auto conn = create_connection(cfg, id);
    // Initiate connection (async, will complete in background)
    (void)conn->connect(endpoint);
    return conn;
}

auto connect(std::string_view url,
             const quic_config& config,
             std::string_view id) -> std::unique_ptr<unified::i_connection>
{
    auto [host, port] = parse_url(url);
    if (host.empty() || port == 0) {
        // Return a connection that will fail on connect
        return create_connection(config, id);
    }

    // Ensure server_name is set for TLS
    quic_config cfg = config;
    if (cfg.server_name.empty()) {
        cfg.server_name = host;
    }

    auto conn = create_connection(cfg, id);
    (void)conn->connect(unified::endpoint_info{host, port});
    return conn;
}

auto create_listener(const quic_config& config, std::string_view id)
    -> std::unique_ptr<unified::i_listener>
{
    std::string listener_id = id.empty()
        ? generate_unique_id("quic-listener")
        : std::string(id);

    return std::make_unique<unified::adapters::quic_listener_adapter>(
        config, listener_id);
}

auto listen(const unified::endpoint_info& bind_address,
            const quic_config& config,
            std::string_view id) -> std::unique_ptr<unified::i_listener>
{
    auto listener = create_listener(config, id);
    (void)listener->start(bind_address);
    return listener;
}

auto listen(uint16_t port, const quic_config& config, std::string_view id)
    -> std::unique_ptr<unified::i_listener>
{
    return listen(unified::endpoint_info{"0.0.0.0", port}, config, id);
}

}  // namespace kcenon::network::protocol::quic
