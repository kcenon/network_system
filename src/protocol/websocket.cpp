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

#include "kcenon/network/protocol/websocket.h"
#include "internal/unified/adapters/ws_connection_adapter.h"
#include "internal/unified/adapters/ws_listener_adapter.h"

#include <atomic>
#include <chrono>
#include <sstream>

namespace kcenon::network::protocol::websocket {

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
    std::string connection_id = id.empty() ? generate_unique_id("ws-conn") : std::string(id);
    return std::make_unique<unified::adapters::ws_connection_adapter>(connection_id);
}

auto connect(std::string_view url, std::string_view id)
    -> std::unique_ptr<unified::i_connection>
{
    auto conn = create_connection(id);
    (void)conn->connect(url);
    return conn;
}

auto connect(const unified::endpoint_info& endpoint,
             std::string_view path,
             std::string_view id)
    -> std::unique_ptr<unified::i_connection>
{
    std::string connection_id = id.empty() ? generate_unique_id("ws-conn") : std::string(id);
    auto adapter = std::make_unique<unified::adapters::ws_connection_adapter>(connection_id);
    adapter->set_path(path);
    (void)adapter->connect(endpoint);
    return adapter;
}

auto create_listener(std::string_view id) -> std::unique_ptr<unified::i_listener>
{
    std::string listener_id = id.empty() ? generate_unique_id("ws-listener") : std::string(id);
    return std::make_unique<unified::adapters::ws_listener_adapter>(listener_id);
}

auto listen(const unified::endpoint_info& bind_address,
            std::string_view path,
            std::string_view id)
    -> std::unique_ptr<unified::i_listener>
{
    std::string listener_id = id.empty() ? generate_unique_id("ws-listener") : std::string(id);
    auto adapter = std::make_unique<unified::adapters::ws_listener_adapter>(listener_id);
    adapter->set_path(path);
    (void)adapter->start(bind_address);
    return adapter;
}

auto listen(uint16_t port, std::string_view path, std::string_view id)
    -> std::unique_ptr<unified::i_listener>
{
    return listen(unified::endpoint_info{"0.0.0.0", port}, path, id);
}

}  // namespace kcenon::network::protocol::websocket
