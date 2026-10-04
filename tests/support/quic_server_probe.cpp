// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#include "quic_server_probe.h"

#include "kcenon/network/detail/session/quic_session.h"

namespace kcenon::network::tests::support
{

auto quic_server_probe::invoke_handle_packet(
    messaging_quic_server& srv,
    std::span<const std::uint8_t> bytes,
    const asio::ip::udp::endpoint& from) -> void
{
    srv.handle_packet(bytes, from);
}

auto quic_server_probe::invoke_generate_session_id(
    messaging_quic_server& srv) -> std::string
{
    return srv.generate_session_id();
}

auto quic_server_probe::invoke_on_session_close(
    messaging_quic_server& srv,
    const std::string& session_id) -> void
{
    srv.on_session_close(session_id);
}

auto quic_server_probe::invoke_cleanup_dead_sessions(
    messaging_quic_server& srv) -> void
{
    srv.cleanup_dead_sessions();
}

auto quic_server_probe::invoke_start_receive(
    messaging_quic_server& srv) -> void
{
    srv.start_receive();
}

auto quic_server_probe::invoke_start_cleanup_timer(
    messaging_quic_server& srv) -> void
{
    srv.start_cleanup_timer();
}

auto quic_server_probe::invoke_connection_callback_dispatcher(
    messaging_quic_server& srv,
    std::shared_ptr<quic_session> session) -> void
{
    srv.invoke_connection_callback(std::move(session));
}

auto quic_server_probe::invoke_disconnection_callback_dispatcher(
    messaging_quic_server& srv,
    std::shared_ptr<quic_session> session) -> void
{
    srv.invoke_disconnection_callback(std::move(session));
}

auto quic_server_probe::invoke_receive_callback_dispatcher(
    messaging_quic_server& srv,
    std::shared_ptr<quic_session> session,
    const std::vector<std::uint8_t>& data) -> void
{
    srv.invoke_receive_callback(std::move(session), data);
}

auto quic_server_probe::invoke_stream_receive_callback_dispatcher(
    messaging_quic_server& srv,
    std::shared_ptr<quic_session> session,
    std::uint64_t stream_id,
    const std::vector<std::uint8_t>& data,
    bool fin) -> void
{
    srv.invoke_stream_receive_callback(std::move(session), stream_id, data,
                                       fin);
}

auto quic_server_probe::invoke_error_callback_dispatcher(
    messaging_quic_server& srv,
    std::error_code ec) -> void
{
    srv.invoke_error_callback(ec);
}

} // namespace kcenon::network::tests::support
