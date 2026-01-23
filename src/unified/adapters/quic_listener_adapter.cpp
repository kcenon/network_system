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

#include "kcenon/network/unified/adapters/quic_listener_adapter.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace kcenon::network::unified::adapters {

namespace {

auto generate_random_cid() -> protocols::quic::connection_id
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    std::vector<uint8_t> cid_bytes(8);
    for (auto& byte : cid_bytes) {
        byte = dist(gen);
    }

    return protocols::quic::connection_id(cid_bytes);
}

auto close_socket(int fd) -> void
{
#ifdef _WIN32
    closesocket(fd);
#else
    ::close(fd);
#endif
}

auto set_nonblocking(int fd) -> bool
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

auto sockaddr_to_string(const sockaddr_in& addr) -> std::string
{
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
    return std::string(ip_str) + ":" + std::to_string(ntohs(addr.sin_port));
}

}  // namespace

quic_listener_adapter::quic_listener_adapter(
    const protocol::quic::quic_config& config,
    std::string_view listener_id)
    : listener_id_(listener_id)
    , config_(config)
{
}

quic_listener_adapter::~quic_listener_adapter()
{
    stop();
}

auto quic_listener_adapter::start(const endpoint_info& bind_address) -> VoidResult
{
    if (running_.load()) {
        return error_void(
            static_cast<int>(std::errc::already_connected),
            "Listener is already running"
        );
    }

    // Validate that cert and key files are provided
    if (config_.cert_file.empty() || config_.key_file.empty()) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "QUIC server requires cert_file and key_file in configuration"
        );
    }

    auto setup_result = setup_socket(bind_address);
    if (!setup_result.is_ok()) {
        return setup_result;
    }

    {
        std::lock_guard lock(endpoint_mutex_);
        local_endpoint_ = bind_address;
    }

    running_ = true;
    stop_requested_ = false;

    // Start accept thread
    accept_thread_ = std::thread(&quic_listener_adapter::accept_thread_func, this);

    return ok();
}

auto quic_listener_adapter::start(uint16_t port) -> VoidResult
{
    return start(endpoint_info{"0.0.0.0", port});
}

auto quic_listener_adapter::stop() noexcept -> void
{
    stop_requested_ = true;

    // Close all connections
    {
        std::lock_guard lock(connections_mutex_);
        for (auto& [conn_id, info] : connections_) {
            if (info.conn && info.conn->is_established()) {
                (void)info.conn->close(0, "Server shutting down");
            }
        }
        connections_.clear();
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    if (socket_fd_ >= 0) {
        close_socket(socket_fd_);
        socket_fd_ = -1;
    }

    running_ = false;

    {
        std::lock_guard lock(stop_mutex_);
    }
    stop_cv_.notify_all();
}

auto quic_listener_adapter::set_callbacks(listener_callbacks callbacks) -> void
{
    std::lock_guard lock(callbacks_mutex_);
    callbacks_ = std::move(callbacks);
}

auto quic_listener_adapter::set_accept_callback(accept_callback_t callback) -> void
{
    std::lock_guard lock(callbacks_mutex_);
    accept_callback_ = std::move(callback);
}

auto quic_listener_adapter::is_listening() const noexcept -> bool
{
    return running_.load();
}

auto quic_listener_adapter::local_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return local_endpoint_;
}

auto quic_listener_adapter::connection_count() const noexcept -> size_t
{
    std::lock_guard lock(connections_mutex_);
    return connections_.size();
}

auto quic_listener_adapter::send_to(std::string_view connection_id,
                                    std::span<const std::byte> data) -> VoidResult
{
    std::lock_guard lock(connections_mutex_);

    auto it = connections_.find(std::string(connection_id));
    if (it == connections_.end()) {
        return error_void(
            static_cast<int>(std::errc::no_such_file_or_directory),
            "Connection not found"
        );
    }

    auto& conn_info = it->second;
    if (!conn_info.conn || !conn_info.conn->is_established()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "Connection is not established"
        );
    }

    // Write data to the first available bidirectional stream
    auto& stream_mgr = conn_info.conn->streams();

    // Create or get stream
    auto stream_result = stream_mgr.create_bidirectional_stream();

    if (!stream_result.is_ok()) {
        return error_void(
            static_cast<int>(std::errc::no_buffer_space),
            "Failed to create stream"
        );
    }

    auto* stream = stream_mgr.get_stream(stream_result.value());
    if (!stream) {
        return error_void(
            static_cast<int>(std::errc::io_error),
            "Failed to get stream"
        );
    }

    std::vector<uint8_t> buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());

    auto write_result = stream->write(buffer);
    if (!write_result.is_ok()) {
        return error_void(
            static_cast<int>(std::errc::io_error),
            "Failed to write to stream"
        );
    }

    conn_info.last_activity = std::chrono::steady_clock::now();

    return ok();
}

auto quic_listener_adapter::broadcast(std::span<const std::byte> data) -> VoidResult
{
    std::lock_guard lock(connections_mutex_);

    bool any_success = false;
    for (auto& [conn_id, conn_info] : connections_) {
        if (conn_info.conn && conn_info.conn->is_established()) {
            auto& stream_mgr = conn_info.conn->streams();
            auto stream_result = stream_mgr.create_bidirectional_stream();

            if (stream_result.is_ok()) {
                auto* stream = stream_mgr.get_stream(stream_result.value());
                if (stream) {
                    std::vector<uint8_t> buffer(data.size());
                    std::memcpy(buffer.data(), data.data(), data.size());

                    auto write_result = stream->write(buffer);
                    if (write_result.is_ok()) {
                        any_success = true;
                    }
                }
            }
        }
    }

    if (!any_success && !connections_.empty()) {
        return error_void(
            static_cast<int>(std::errc::io_error),
            "Failed to send to any connection"
        );
    }

    return ok();
}

auto quic_listener_adapter::close_connection(std::string_view connection_id) noexcept -> void
{
    std::lock_guard lock(connections_mutex_);

    auto it = connections_.find(std::string(connection_id));
    if (it != connections_.end()) {
        if (it->second.conn) {
            (void)it->second.conn->close(0, "Connection closed by server");
        }

        // Notify callback
        {
            std::lock_guard cb_lock(callbacks_mutex_);
            if (callbacks_.on_disconnect) {
                callbacks_.on_disconnect(connection_id);
            }
        }

        connections_.erase(it);
    }
}

auto quic_listener_adapter::wait_for_stop() -> void
{
    std::unique_lock lock(stop_mutex_);
    stop_cv_.wait(lock, [this] { return !running_.load(); });
}

auto quic_listener_adapter::setup_socket(const endpoint_info& bind_address) -> VoidResult
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return error_void(
            static_cast<int>(std::errc::io_error),
            "Failed to initialize Winsock"
        );
    }
#endif

    socket_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd_ < 0) {
        return error_void(
            static_cast<int>(std::errc::io_error),
            "Failed to create UDP socket"
        );
    }

    // Allow address reuse
    int reuse = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    if (!set_nonblocking(socket_fd_)) {
        close_socket(socket_fd_);
        socket_fd_ = -1;
        return error_void(
            static_cast<int>(std::errc::io_error),
            "Failed to set socket to non-blocking mode"
        );
    }

    // Bind to address
    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(bind_address.port);

    if (bind_address.host == "0.0.0.0" || bind_address.host.empty()) {
        bind_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, bind_address.host.c_str(), &bind_addr.sin_addr);
    }

    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        close_socket(socket_fd_);
        socket_fd_ = -1;
        return error_void(
            static_cast<int>(std::errc::address_in_use),
            "Failed to bind socket to address"
        );
    }

    return ok();
}

auto quic_listener_adapter::accept_thread_func() -> void
{
    constexpr int POLL_TIMEOUT_MS = 10;
    constexpr size_t MAX_PACKET_SIZE = 65535;

    std::vector<uint8_t> recv_buffer(MAX_PACKET_SIZE);

    while (!stop_requested_.load()) {
        // Poll for incoming data
#ifdef _WIN32
        WSAPOLLFD pfd{};
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;

        int poll_result = WSAPoll(&pfd, 1, POLL_TIMEOUT_MS);
#else
        pollfd pfd{};
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;

        int poll_result = poll(&pfd, 1, POLL_TIMEOUT_MS);
#endif

        if (poll_result > 0 && (pfd.revents & POLLIN)) {
            // Receive incoming packet
            sockaddr_in from_addr{};
            socklen_t from_len = sizeof(from_addr);

            auto received = recvfrom(socket_fd_, reinterpret_cast<char*>(recv_buffer.data()),
                                     static_cast<int>(recv_buffer.size()), 0,
                                     reinterpret_cast<sockaddr*>(&from_addr), &from_len);

            if (received > 0) {
                std::span<const uint8_t> packet_data(recv_buffer.data(),
                                                     static_cast<size_t>(received));

                // Identify or create connection based on source address
                std::string client_addr = sockaddr_to_string(from_addr);

                std::lock_guard lock(connections_mutex_);

                auto it = connections_.find(client_addr);
                if (it == connections_.end()) {
                    // New connection - create QUIC connection
                    auto initial_dcid = generate_random_cid();
                    auto new_conn = std::make_unique<protocols::quic::connection>(
                        true, initial_dcid);

                    // Initialize server handshake
                    auto init_result = new_conn->init_server_handshake(
                        config_.cert_file, config_.key_file);

                    if (!init_result.is_ok()) {
                        continue;
                    }

                    // Set transport parameters
                    protocols::quic::transport_parameters local_params;
                    local_params.initial_max_data = config_.initial_max_data;
                    local_params.initial_max_stream_data_bidi_local =
                        config_.initial_max_stream_data_bidi;
                    local_params.initial_max_stream_data_bidi_remote =
                        config_.initial_max_stream_data_bidi;
                    local_params.initial_max_stream_data_uni =
                        config_.initial_max_stream_data_uni;
                    local_params.initial_max_streams_bidi = config_.max_bidi_streams;
                    local_params.initial_max_streams_uni = config_.max_uni_streams;

                    if (config_.idle_timeout.count() > 0) {
                        local_params.max_idle_timeout =
                            static_cast<uint64_t>(config_.idle_timeout.count());
                    }

                    new_conn->set_local_params(local_params);

                    if (config_.enable_pmtud) {
                        new_conn->enable_pmtud();
                    }

                    connection_info info;
                    info.conn = std::move(new_conn);
                    info.last_activity = std::chrono::steady_clock::now();

                    connections_[client_addr] = std::move(info);

                    // Notify accept callback
                    std::lock_guard cb_lock(callbacks_mutex_);
                    if (callbacks_.on_accept) {
                        callbacks_.on_accept(client_addr);
                    }
                }

                // Process packet on existing connection
                auto conn_it = connections_.find(client_addr);
                if (conn_it != connections_.end() && conn_it->second.conn) {
                    auto result = conn_it->second.conn->receive_packet(packet_data);
                    (void)result;
                    conn_it->second.last_activity = std::chrono::steady_clock::now();
                }
            }
        }

        // Process all connections
        process_connections();
    }

    running_ = false;

    {
        std::lock_guard lock(stop_mutex_);
    }
    stop_cv_.notify_all();
}

auto quic_listener_adapter::process_connections() -> void
{
    std::lock_guard lock(connections_mutex_);

    std::vector<std::string> to_remove;

    for (auto& [conn_id, conn_info] : connections_) {
        if (!conn_info.conn) {
            to_remove.push_back(conn_id);
            continue;
        }

        // Check for timeouts
        auto timeout = conn_info.conn->next_timeout();
        if (timeout && std::chrono::steady_clock::now() >= *timeout) {
            conn_info.conn->on_timeout();
        }

        // Generate and send packets
        auto packets = conn_info.conn->generate_packets();

        // Parse connection ID to get address for sending
        auto colon_pos = conn_id.rfind(':');
        if (colon_pos != std::string::npos && !packets.empty()) {
            std::string host = conn_id.substr(0, colon_pos);
            uint16_t port = static_cast<uint16_t>(
                std::stoi(conn_id.substr(colon_pos + 1)));

            sockaddr_in dest_addr{};
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(port);
            inet_pton(AF_INET, host.c_str(), &dest_addr.sin_addr);

            for (const auto& packet : packets) {
                sendto(socket_fd_, reinterpret_cast<const char*>(packet.data()),
                       static_cast<int>(packet.size()), 0,
                       reinterpret_cast<const sockaddr*>(&dest_addr),
                       sizeof(dest_addr));
            }
        }

        // Check for received data on streams
        if (conn_info.conn->is_established()) {
            auto& stream_mgr = conn_info.conn->streams();

            // Check bidirectional streams (client-initiated: 0, 4, 8, ...)
            for (uint64_t stream_id = 0; stream_id < 1000; stream_id += 4) {
                auto* stream = stream_mgr.get_stream(stream_id);
                if (stream && stream->has_data()) {
                    std::vector<uint8_t> buffer(4096);
                    auto read_result = stream->read(buffer);
                    if (read_result.is_ok() && read_result.value() > 0) {
                        buffer.resize(read_result.value());

                        std::lock_guard cb_lock(callbacks_mutex_);
                        if (callbacks_.on_data) {
                            std::span<const std::byte> byte_span(
                                reinterpret_cast<const std::byte*>(buffer.data()),
                                buffer.size()
                            );
                            callbacks_.on_data(conn_id, byte_span);
                        }
                    }
                }
            }
        }

        // Check if connection is closed
        if (conn_info.conn->is_closed()) {
            to_remove.push_back(conn_id);

            std::lock_guard cb_lock(callbacks_mutex_);
            if (callbacks_.on_disconnect) {
                callbacks_.on_disconnect(conn_id);
            }
        }
    }

    // Remove closed connections
    for (const auto& conn_id : to_remove) {
        connections_.erase(conn_id);
    }
}

auto quic_listener_adapter::generate_connection_id(
    const protocols::quic::connection_id& cid) -> std::string
{
    std::ostringstream oss;
    oss << std::hex;
    for (size_t i = 0; i < cid.length(); ++i) {
        oss << std::setfill('0') << std::setw(2)
            << static_cast<int>(cid.data()[i]);
    }
    return oss.str();
}

auto quic_listener_adapter::handle_new_connection(
    std::unique_ptr<protocols::quic::connection> conn,
    const std::string& conn_id) -> void
{
    connection_info info;
    info.conn = std::move(conn);
    info.last_activity = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(connections_mutex_);
        connections_[conn_id] = std::move(info);
    }

    std::lock_guard cb_lock(callbacks_mutex_);
    if (callbacks_.on_accept) {
        callbacks_.on_accept(conn_id);
    }
}

}  // namespace kcenon::network::unified::adapters
