// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#include "ws_server_probe.h"

#include <utility>

namespace kcenon::network::tests::support
{

auto ws_server_probe::invoke_handle_new_connection(
    kcenon::network::core::messaging_ws_server& srv,
    std::shared_ptr<asio::ip::tcp::socket> socket) -> void
{
    srv.handle_new_connection(std::move(socket));
}

} // namespace kcenon::network::tests::support
