// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "network_udp/protocol/udp.h"
#include "network_udp/unified/adapters/udp_connection_adapter.h"
#include "network_udp/unified/adapters/udp_listener_adapter.h"

#include <atomic>
#include <chrono>
#include <sstream>

namespace kcenon::network::protocol::udp {

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

}  // namespace

auto create_connection(std::string_view id) -> std::unique_ptr<unified::i_connection>
{
    std::string connection_id = id.empty() ? generate_unique_id("udp-conn") : std::string(id);
    return std::make_unique<unified::adapters::udp_connection_adapter>(connection_id);
}

auto connect(const unified::endpoint_info& endpoint, std::string_view id)
    -> std::unique_ptr<unified::i_connection>
{
    auto conn = create_connection(id);
    // Initiate "connection" (for UDP, this starts the client with target endpoint)
    (void)conn->connect(endpoint);
    return conn;
}

auto connect(std::string_view url, std::string_view id)
    -> std::unique_ptr<unified::i_connection>
{
    auto conn = create_connection(id);
    (void)conn->connect(url);
    return conn;
}

auto create_listener(std::string_view id) -> std::unique_ptr<unified::i_listener>
{
    std::string listener_id = id.empty() ? generate_unique_id("udp-listener") : std::string(id);
    return std::make_unique<unified::adapters::udp_listener_adapter>(listener_id);
}

auto listen(const unified::endpoint_info& bind_address, std::string_view id)
    -> std::unique_ptr<unified::i_listener>
{
    auto listener = create_listener(id);
    (void)listener->start(bind_address);
    return listener;
}

auto listen(uint16_t port, std::string_view id) -> std::unique_ptr<unified::i_listener>
{
    return listen(unified::endpoint_info{"0.0.0.0", port}, id);
}

}  // namespace kcenon::network::protocol::udp
