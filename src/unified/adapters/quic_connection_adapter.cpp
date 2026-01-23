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

#include "kcenon/network/unified/adapters/quic_connection_adapter.h"

#include <algorithm>
#include <cstring>
#include <random>

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
    // Use unsigned short instead of uint8_t for MSVC compatibility
    // C++ standard only allows short, int, long, long long and their unsigned versions
    std::uniform_int_distribution<unsigned short> dist(0, 255);

    std::vector<uint8_t> cid_bytes(8);
    for (auto& byte : cid_bytes) {
        byte = static_cast<uint8_t>(dist(gen));
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

}  // namespace

quic_connection_adapter::quic_connection_adapter(
    const protocol::quic::quic_config& config,
    std::string_view connection_id)
    : connection_id_(connection_id)
    , config_(config)
{
}

quic_connection_adapter::~quic_connection_adapter()
{
    close();
}

auto quic_connection_adapter::send(std::span<const std::byte> data) -> VoidResult
{
    if (!is_connected_.load()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "QUIC connection is not established"
        );
    }

    if (!quic_conn_) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "QUIC connection object is null"
        );
    }

    // Queue data for sending on the default stream (stream 0)
    std::vector<uint8_t> buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());

    {
        std::lock_guard lock(send_queue_mutex_);
        send_queue_.push_back(std::move(buffer));
    }

    return ok();
}

auto quic_connection_adapter::send(std::vector<uint8_t>&& data) -> VoidResult
{
    if (!is_connected_.load()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "QUIC connection is not established"
        );
    }

    if (!quic_conn_) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "QUIC connection object is null"
        );
    }

    {
        std::lock_guard lock(send_queue_mutex_);
        send_queue_.push_back(std::move(data));
    }

    return ok();
}

auto quic_connection_adapter::is_connected() const noexcept -> bool
{
    return is_connected_.load();
}

auto quic_connection_adapter::id() const noexcept -> std::string_view
{
    return connection_id_;
}

auto quic_connection_adapter::remote_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return remote_endpoint_;
}

auto quic_connection_adapter::local_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return local_endpoint_;
}

auto quic_connection_adapter::connect(const endpoint_info& endpoint) -> VoidResult
{
    if (is_connected_.load() || running_.load()) {
        return error_void(
            static_cast<int>(std::errc::already_connected),
            "Already connected or connecting"
        );
    }

    {
        std::lock_guard lock(endpoint_mutex_);
        remote_endpoint_ = endpoint;
    }

    // Create UDP socket
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

    if (!set_nonblocking(socket_fd_)) {
        close_socket(socket_fd_);
        socket_fd_ = -1;
        return error_void(
            static_cast<int>(std::errc::io_error),
            "Failed to set socket to non-blocking mode"
        );
    }

    // Create QUIC connection (client side)
    auto initial_dcid = generate_random_cid();
    quic_conn_ = std::make_unique<protocols::quic::connection>(false, initial_dcid);

    // Set transport parameters from config
    protocols::quic::transport_parameters local_params;
    local_params.initial_max_data = config_.initial_max_data;
    local_params.initial_max_stream_data_bidi_local = config_.initial_max_stream_data_bidi;
    local_params.initial_max_stream_data_bidi_remote = config_.initial_max_stream_data_bidi;
    local_params.initial_max_stream_data_uni = config_.initial_max_stream_data_uni;
    local_params.initial_max_streams_bidi = config_.max_bidi_streams;
    local_params.initial_max_streams_uni = config_.max_uni_streams;

    if (config_.idle_timeout.count() > 0) {
        local_params.max_idle_timeout =
            static_cast<uint64_t>(config_.idle_timeout.count());
    }

    quic_conn_->set_local_params(local_params);

    // Enable PMTUD if configured
    if (config_.enable_pmtud) {
        quic_conn_->enable_pmtud();
    }

    // Start handshake
    std::string server_name = config_.server_name;
    if (server_name.empty()) {
        server_name = endpoint.host;
    }

    auto hs_result = quic_conn_->start_handshake(server_name);
    if (!hs_result.is_ok()) {
        quic_conn_.reset();
        close_socket(socket_fd_);
        socket_fd_ = -1;
        return error_void(
            static_cast<int>(std::errc::connection_refused),
            "Failed to start QUIC handshake"
        );
    }

    is_connecting_ = true;
    running_ = true;
    stop_requested_ = false;

    // Start I/O thread
    io_thread_ = std::thread(&quic_connection_adapter::io_thread_func, this);

    return ok();
}

auto quic_connection_adapter::connect(std::string_view url) -> VoidResult
{
    auto [host, port] = parse_url(url);
    if (host.empty() || port == 0) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "Invalid QUIC URL format (expected: quic://host:port or host:port)"
        );
    }

    return connect(endpoint_info{host, port});
}

auto quic_connection_adapter::close() noexcept -> void
{
    stop_requested_ = true;

    if (quic_conn_ && quic_conn_->is_established()) {
        (void)quic_conn_->close(0, "Connection closed by client");
    }

    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    if (socket_fd_ >= 0) {
        close_socket(socket_fd_);
        socket_fd_ = -1;
    }

    quic_conn_.reset();
    is_connected_ = false;
    is_connecting_ = false;
    running_ = false;

    {
        std::lock_guard lock(stop_mutex_);
    }
    stop_cv_.notify_all();
}

auto quic_connection_adapter::set_callbacks(connection_callbacks callbacks) -> void
{
    std::lock_guard lock(callbacks_mutex_);
    callbacks_ = std::move(callbacks);
}

auto quic_connection_adapter::set_options(connection_options options) -> void
{
    options_ = options;
}

auto quic_connection_adapter::set_timeout(std::chrono::milliseconds timeout) -> void
{
    options_.connect_timeout = timeout;
}

auto quic_connection_adapter::is_connecting() const noexcept -> bool
{
    return is_connecting_.load();
}

auto quic_connection_adapter::wait_for_stop() -> void
{
    std::unique_lock lock(stop_mutex_);
    stop_cv_.wait(lock, [this] { return !running_.load(); });
}

auto quic_connection_adapter::io_thread_func() -> void
{
    constexpr int POLL_TIMEOUT_MS = 10;
    constexpr size_t MAX_PACKET_SIZE = 65535;

    std::vector<uint8_t> recv_buffer(MAX_PACKET_SIZE);
    sockaddr_in remote_addr{};
    remote_addr.sin_family = AF_INET;

    // Resolve remote address
    {
        std::lock_guard lock(endpoint_mutex_);
        remote_addr.sin_port = htons(remote_endpoint_.port);

        struct addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo(remote_endpoint_.host.c_str(), nullptr, &hints, &result) == 0
            && result) {
            remote_addr.sin_addr =
                reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;
            freeaddrinfo(result);
        } else {
            inet_pton(AF_INET, remote_endpoint_.host.c_str(), &remote_addr.sin_addr);
        }
    }

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
                auto result = quic_conn_->receive_packet(packet_data);
                (void)result;

                // Check for state changes
                handle_state_change();
            }
        }

        // Process outgoing data
        process_outgoing();

        // Check for timeouts
        if (quic_conn_) {
            auto timeout = quic_conn_->next_timeout();
            if (timeout && std::chrono::steady_clock::now() >= *timeout) {
                quic_conn_->on_timeout();
            }
        }

        // Handle state changes
        handle_state_change();

        // Generate and send QUIC packets
        if (quic_conn_) {
            auto packets = quic_conn_->generate_packets();
            for (const auto& packet : packets) {
                sendto(socket_fd_, reinterpret_cast<const char*>(packet.data()),
                       static_cast<int>(packet.size()), 0,
                       reinterpret_cast<const sockaddr*>(&remote_addr),
                       sizeof(remote_addr));
            }
        }

        // Check if connection is closed
        if (quic_conn_ && quic_conn_->is_closed()) {
            break;
        }
    }

    running_ = false;
    is_connected_ = false;
    is_connecting_ = false;

    // Notify callbacks
    {
        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_disconnected) {
            callbacks_.on_disconnected();
        }
    }

    {
        std::lock_guard lock(stop_mutex_);
    }
    stop_cv_.notify_all();
}

auto quic_connection_adapter::process_outgoing() -> void
{
    if (!quic_conn_ || !quic_conn_->is_established()) {
        return;
    }

    std::deque<std::vector<uint8_t>> data_to_send;
    {
        std::lock_guard lock(send_queue_mutex_);
        data_to_send.swap(send_queue_);
    }

    for (auto& data : data_to_send) {
        // Get or create bidirectional stream
        auto& stream_mgr = quic_conn_->streams();
        auto stream_result = stream_mgr.create_bidirectional_stream();

        if (stream_result.is_ok()) {
            uint64_t stream_id = stream_result.value();
            auto* stream = stream_mgr.get_stream(stream_id);
            if (stream) {
                (void)stream->write(data);
            }
        }
    }
}

auto quic_connection_adapter::process_incoming() -> void
{
    if (!quic_conn_ || !quic_conn_->is_established()) {
        return;
    }

    auto& stream_mgr = quic_conn_->streams();

    // Read data from all readable streams
    for (uint64_t stream_id = 0; stream_id < 1000; stream_id += 4) {
        auto* stream = stream_mgr.get_stream(stream_id);
        if (stream && stream->has_data()) {
            std::vector<uint8_t> buffer(4096);
            auto read_result = stream->read(buffer);
            if (read_result.is_ok() && read_result.value() > 0) {
                buffer.resize(read_result.value());

                std::lock_guard lock(callbacks_mutex_);
                if (callbacks_.on_data) {
                    std::span<const std::byte> byte_span(
                        reinterpret_cast<const std::byte*>(buffer.data()),
                        buffer.size()
                    );
                    callbacks_.on_data(byte_span);
                }
            }
        }
    }
}

auto quic_connection_adapter::handle_state_change() -> void
{
    if (!quic_conn_) {
        return;
    }

    bool was_connecting = is_connecting_.load();
    bool is_now_established = quic_conn_->is_established();

    if (was_connecting && is_now_established) {
        is_connecting_ = false;
        is_connected_ = true;

        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_connected) {
            callbacks_.on_connected();
        }
    }

    if (quic_conn_->is_draining() || quic_conn_->is_closed()) {
        is_connected_ = false;

        auto error_code = quic_conn_->close_error_code();
        if (error_code && *error_code != 0) {
            std::lock_guard lock(callbacks_mutex_);
            if (callbacks_.on_error) {
                callbacks_.on_error(std::make_error_code(std::errc::connection_reset));
            }
        }
    }

    // Process incoming data
    process_incoming();
}

auto quic_connection_adapter::parse_url(std::string_view url)
    -> std::pair<std::string, uint16_t>
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

}  // namespace kcenon::network::unified::adapters
