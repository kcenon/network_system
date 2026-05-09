// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#include "ws_server_probe.h"

#include <utility>

namespace kcenon::network::tests::support
{

auto ws_server_probe::invoke_handle_new_connection(
    messaging_ws_server& srv,
    std::shared_ptr<asio::ip::tcp::socket> socket) -> void
{
    srv.handle_new_connection(std::move(socket));
}

auto ws_server_probe::invoke_on_message(
    messaging_ws_server& srv,
    std::shared_ptr<ws_connection> conn,
    const ws_message& msg) -> void
{
    srv.on_message(std::move(conn), msg);
}

auto ws_server_probe::invoke_on_close(
    messaging_ws_server& srv,
    const std::string& conn_id,
    ws_close_code code,
    const std::string& reason) -> void
{
    srv.on_close(conn_id, code, reason);
}

auto ws_server_probe::invoke_on_error(
    messaging_ws_server& srv,
    const std::string& conn_id,
    std::error_code ec) -> void
{
    srv.on_error(conn_id, ec);
}

auto ws_server_probe::invoke_connection_callback_dispatcher(
    messaging_ws_server& srv,
    std::shared_ptr<ws_connection> conn) -> void
{
    srv.invoke_connection_callback(std::move(conn));
}

auto ws_server_probe::invoke_disconnection_callback_dispatcher(
    messaging_ws_server& srv,
    const std::string& conn_id,
    ws_close_code code,
    const std::string& reason) -> void
{
    srv.invoke_disconnection_callback(conn_id, code, reason);
}

auto ws_server_probe::invoke_message_callback_dispatcher(
    messaging_ws_server& srv,
    std::shared_ptr<ws_connection> conn,
    const ws_message& msg) -> void
{
    srv.invoke_message_callback(std::move(conn), msg);
}

auto ws_server_probe::invoke_error_callback_dispatcher(
    messaging_ws_server& srv,
    const std::string& conn_id,
    std::error_code ec) -> void
{
    srv.invoke_error_callback(conn_id, ec);
}

auto ws_server_probe::invoke_do_accept(messaging_ws_server& srv) -> void
{
    srv.do_accept();
}

} // namespace kcenon::network::tests::support
