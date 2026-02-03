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

#pragma once

#include "internal/core/network_context.h"
#include "internal/udp/udp_socket.h"
#include "kcenon/network/integration/logger_integration.h"

#ifdef BUILD_TLS_SUPPORT
#include "internal/tcp/dtls_socket.h"
#include <chrono>
#include <condition_variable>
#endif

namespace kcenon::network::core
{

using udp = asio::ip::udp;

// =====================================================================
// Constructor / Destructor
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
unified_udp_messaging_client<TlsPolicy>::unified_udp_messaging_client(std::string_view client_id)
    requires(!policy::is_tls_enabled_v<TlsPolicy>)
    : client_id_(client_id)
{
}

#ifdef BUILD_TLS_SUPPORT
template <policy::TlsPolicy TlsPolicy>
unified_udp_messaging_client<TlsPolicy>::unified_udp_messaging_client(
    std::string_view client_id,
    const TlsPolicy& tls_config)
    requires policy::is_tls_enabled_v<TlsPolicy>
    : client_id_(client_id)
    , tls_config_(tls_config)
{
}
#endif

template <policy::TlsPolicy TlsPolicy>
unified_udp_messaging_client<TlsPolicy>::~unified_udp_messaging_client() noexcept
{
    if (lifecycle_.is_running())
    {
        [[maybe_unused]] auto _ = stop_client();
    }
}

// =====================================================================
// Lifecycle Management
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::start_client(std::string_view host, uint16_t port)
    -> VoidResult
{
    if (lifecycle_.is_running())
    {
        return error_void(error_codes::common_errors::already_exists,
                          "UDP client is already running",
                          "unified_udp_messaging_client::start_client");
    }

    if (host.empty())
    {
        return error_void(error_codes::common_errors::invalid_argument,
                          "Host cannot be empty",
                          "unified_udp_messaging_client::start_client");
    }

    if (!lifecycle_.try_start())
    {
        return error_void(error_codes::common_errors::already_exists,
                          "UDP client is already starting",
                          "unified_udp_messaging_client::start_client",
                          "Client ID: " + client_id_);
    }

    is_connected_.store(false, std::memory_order_release);

    auto result = do_start_impl(host, port);
    if (result.is_err())
    {
        lifecycle_.mark_stopped();
    }

    return result;
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::stop_client() -> VoidResult
{
    if (!lifecycle_.prepare_stop())
    {
        return ok();  // Not running or already stopping
    }

    is_connected_.store(false, std::memory_order_release);

    auto result = do_stop_impl();

    invoke_disconnected_callback();
    lifecycle_.mark_stopped();

    return result;
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::client_id() const -> const std::string&
{
    return client_id_;
}

// =====================================================================
// i_network_component interface implementation
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::is_running() const -> bool
{
    return lifecycle_.is_running();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::wait_for_stop() -> void
{
    lifecycle_.wait_for_stop();
}

// =====================================================================
// i_udp_client interface implementation
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::start(std::string_view host, uint16_t port)
    -> VoidResult
{
    return start_client(host, port);
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::stop() -> VoidResult
{
    return stop_client();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::send(
    std::vector<uint8_t>&& data,
    interfaces::i_udp_client::send_callback_t handler) -> VoidResult
{
    if (!lifecycle_.is_running())
    {
        return error_void(error_codes::common_errors::internal_error,
                          "UDP client is not running",
                          "unified_udp_messaging_client::send");
    }

    std::lock_guard<std::mutex> socket_lock(socket_mutex_);
    if (!socket_)
    {
        return error_void(error_codes::common_errors::internal_error,
                          "Socket not available",
                          "unified_udp_messaging_client::send");
    }

    std::lock_guard<std::mutex> endpoint_lock(endpoint_mutex_);
    if constexpr (is_secure)
    {
#ifdef BUILD_TLS_SUPPORT
        socket_->async_send(std::move(data), std::move(handler));
#endif
    }
    else
    {
        socket_->async_send_to(std::move(data), target_endpoint_, std::move(handler));
    }

    return ok();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::set_target(std::string_view host, uint16_t port)
    -> VoidResult
{
    if (!lifecycle_.is_running())
    {
        return error_void(error_codes::common_errors::internal_error,
                          "UDP client is not running",
                          "unified_udp_messaging_client::set_target");
    }

    try
    {
        udp::resolver resolver(*io_context_);
        auto endpoints = resolver.resolve(udp::v4(), std::string(host), std::to_string(port));

        if (endpoints.empty())
        {
            return error_void(error_codes::common_errors::internal_error,
                              "Failed to resolve host",
                              "unified_udp_messaging_client::set_target",
                              "Host: " + std::string(host));
        }

        std::lock_guard<std::mutex> lock(endpoint_mutex_);
        target_endpoint_ = *endpoints.begin();

        NETWORK_LOG_INFO("Target updated to " + std::string(host) + ":" + std::to_string(port));

        return ok();
    }
    catch (const std::exception& e)
    {
        return error_void(error_codes::common_errors::internal_error,
                          std::string("Failed to set target: ") + e.what(),
                          "unified_udp_messaging_client::set_target",
                          "Host: " + std::string(host) + ":" + std::to_string(port));
    }
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::set_receive_callback(
    interfaces::i_udp_client::receive_callback_t callback) -> void
{
    if (!callback)
    {
        callbacks_.template set<to_index(callback_index::receive)>(nullptr);
        return;
    }

    callbacks_.template set<to_index(callback_index::receive)>(
        [callback = std::move(callback)](const std::vector<uint8_t>& data,
                                         const asio::ip::udp::endpoint& endpoint) {
            interfaces::i_udp_client::endpoint_info info;
            info.address = endpoint.address().to_string();
            info.port = endpoint.port();
            callback(data, info);
        });
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::set_receive_callback(receive_callback_t callback)
    -> void
{
    callbacks_.template set<to_index(callback_index::receive)>(std::move(callback));
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::set_error_callback(error_callback_t callback) -> void
{
    callbacks_.template set<to_index(callback_index::error)>(std::move(callback));
}

// =====================================================================
// Extended API
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::is_connected() const noexcept -> bool
{
    if constexpr (is_secure)
    {
        return is_connected_.load(std::memory_order_acquire);
    }
    else
    {
        return lifecycle_.is_running();
    }
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::send_packet(std::vector<uint8_t>&& data) -> VoidResult
{
    if constexpr (is_secure)
    {
        if (!is_connected_.load(std::memory_order_acquire))
        {
            return error_void(error_codes::network_system::connection_closed,
                              "Not connected",
                              "unified_udp_messaging_client::send_packet");
        }
    }
    else
    {
        if (!lifecycle_.is_running())
        {
            return error_void(error_codes::common_errors::internal_error,
                              "UDP client is not running",
                              "unified_udp_messaging_client::send_packet");
        }
    }

    if (data.empty())
    {
        return error_void(error_codes::common_errors::invalid_argument,
                          "Data cannot be empty",
                          "unified_udp_messaging_client::send_packet");
    }

    return send(std::move(data), nullptr);
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::set_connected_callback(connected_callback_t callback)
    -> void
{
    callbacks_.template set<to_index(callback_index::connected)>(std::move(callback));
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::set_disconnected_callback(
    disconnected_callback_t callback) -> void
{
    callbacks_.template set<to_index(callback_index::disconnected)>(std::move(callback));
}

// =====================================================================
// Internal Callback Helpers
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::set_connected(bool connected) -> void
{
    is_connected_.store(connected, std::memory_order_release);
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::invoke_receive_callback(
    const std::vector<uint8_t>& data,
    const asio::ip::udp::endpoint& endpoint) -> void
{
    callbacks_.template invoke<to_index(callback_index::receive)>(data, endpoint);
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::invoke_connected_callback() -> void
{
    callbacks_.template invoke<to_index(callback_index::connected)>();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::invoke_disconnected_callback() -> void
{
    callbacks_.template invoke<to_index(callback_index::disconnected)>();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::invoke_error_callback(std::error_code ec) -> void
{
    callbacks_.template invoke<to_index(callback_index::error)>(ec);
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::get_receive_callback() const -> receive_callback_t
{
    return callbacks_.template get<to_index(callback_index::receive)>();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::get_error_callback() const -> error_callback_t
{
    return callbacks_.template get<to_index(callback_index::error)>();
}

// =====================================================================
// Internal Implementation Methods
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::do_start_impl(std::string_view host, uint16_t port)
    -> VoidResult
{
    try
    {
#ifdef BUILD_TLS_SUPPORT
        if constexpr (is_secure)
        {
            auto ssl_result = init_ssl_context();
            if (!ssl_result.is_ok())
            {
                return ssl_result;
            }
        }
#endif

        io_context_ = std::make_unique<asio::io_context>();

        udp::resolver resolver(*io_context_);
        auto endpoints = resolver.resolve(udp::v4(), std::string(host), std::to_string(port));

        if (endpoints.empty())
        {
            return error_void(error_codes::common_errors::internal_error,
                              "Failed to resolve host",
                              "unified_udp_messaging_client::do_start_impl",
                              "Host: " + std::string(host));
        }

        {
            std::lock_guard<std::mutex> lock(endpoint_mutex_);
            target_endpoint_ = *endpoints.begin();
        }

        asio::ip::udp::socket raw_socket(*io_context_, udp::endpoint(udp::v4(), 0));

        if constexpr (is_secure)
        {
#ifdef BUILD_TLS_SUPPORT
            socket_ = std::make_shared<internal::dtls_socket>(std::move(raw_socket), ssl_ctx_);

            {
                std::lock_guard<std::mutex> lock(endpoint_mutex_);
                socket_->set_peer_endpoint(target_endpoint_);
            }

            auto self = this->shared_from_this();
            socket_->set_receive_callback(
                [this, self](const std::vector<uint8_t>& data,
                             const asio::ip::udp::endpoint& sender) {
                    invoke_receive_callback(data, sender);
                });

            socket_->set_error_callback([this, self](std::error_code ec) {
                invoke_error_callback(ec);
            });
#endif
        }
        else
        {
            socket_ = std::make_shared<internal::udp_socket>(std::move(raw_socket));

            auto receive_cb = get_receive_callback();
            if (receive_cb)
            {
                socket_->set_receive_callback(std::move(receive_cb));
            }

            auto error_cb = get_error_callback();
            if (error_cb)
            {
                socket_->set_error_callback(std::move(error_cb));
            }

            socket_->start_receive();
        }

        thread_pool_ = network_context::instance().get_thread_pool();
        if (!thread_pool_)
        {
            NETWORK_LOG_WARN("[unified_udp_messaging_client] network_context not initialized, "
                             "creating temporary thread pool");
            thread_pool_ = std::make_shared<integration::basic_thread_pool>(
                std::thread::hardware_concurrency());
        }

        io_context_future_ = thread_pool_->submit([this]() {
            try
            {
                auto work_guard = asio::make_work_guard(*io_context_);
                NETWORK_LOG_DEBUG("[unified_udp_messaging_client] io_context started");
                io_context_->run();
                NETWORK_LOG_DEBUG("[unified_udp_messaging_client] io_context stopped");
            }
            catch (const std::exception& e)
            {
                NETWORK_LOG_ERROR(std::string("Worker thread exception: ") + e.what());
            }
        });

#ifdef BUILD_TLS_SUPPORT
        if constexpr (is_secure)
        {
            auto handshake_result = do_handshake();
            if (!handshake_result.is_ok())
            {
                return handshake_result;
            }

            set_connected(true);
            invoke_connected_callback();
        }
        else
#endif
        {
            set_connected(true);
            invoke_connected_callback();
        }

        NETWORK_LOG_INFO("UDP client started targeting " + std::string(host) + ":" +
                         std::to_string(port));

        return ok();
    }
    catch (const std::system_error& e)
    {
        return error_void(error_codes::common_errors::internal_error,
                          std::string("Failed to create UDP socket: ") + e.what(),
                          "unified_udp_messaging_client::do_start_impl",
                          "Host: " + std::string(host) + ":" + std::to_string(port));
    }
    catch (const std::exception& e)
    {
        return error_void(error_codes::common_errors::internal_error,
                          std::string("Failed to start UDP client: ") + e.what(),
                          "unified_udp_messaging_client::do_start_impl",
                          "Host: " + std::string(host) + ":" + std::to_string(port));
    }
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::do_stop_impl() -> VoidResult
{
    try
    {
        if (socket_)
        {
            socket_->stop_receive();
            socket_.reset();
        }

        if (io_context_)
        {
            io_context_->stop();
        }

        if (io_context_future_.valid())
        {
            try
            {
                io_context_future_.wait();
            }
            catch (const std::exception& e)
            {
                NETWORK_LOG_ERROR("[unified_udp_messaging_client] Exception while waiting for "
                                  "io_context: " +
                                  std::string(e.what()));
            }
        }

#ifdef BUILD_TLS_SUPPORT
        if constexpr (is_secure)
        {
            if (ssl_ctx_)
            {
                SSL_CTX_free(ssl_ctx_);
                ssl_ctx_ = nullptr;
            }
        }
#endif

        io_context_.reset();

        NETWORK_LOG_INFO("UDP client stopped");

        return ok();
    }
    catch (const std::exception& e)
    {
        return error_void(error_codes::common_errors::internal_error,
                          std::string("Failed to stop UDP client: ") + e.what(),
                          "unified_udp_messaging_client::do_stop_impl");
    }
}

#ifdef BUILD_TLS_SUPPORT
template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::init_ssl_context() -> VoidResult
    requires is_secure
{
    ssl_ctx_ = SSL_CTX_new(DTLS_client_method());
    if (!ssl_ctx_)
    {
        return error_void(error_codes::common_errors::internal_error,
                          "Failed to create DTLS context",
                          "unified_udp_messaging_client::init_ssl_context");
    }

    SSL_CTX_set_options(ssl_ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

    if (tls_config_.verify_peer)
    {
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, nullptr);
        if (!tls_config_.ca_path.empty())
        {
            SSL_CTX_load_verify_locations(ssl_ctx_, tls_config_.ca_path.c_str(), nullptr);
        }
        else
        {
            SSL_CTX_set_default_verify_paths(ssl_ctx_);
        }
    }
    else
    {
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, nullptr);
    }

    if (!tls_config_.cert_path.empty())
    {
        SSL_CTX_use_certificate_chain_file(ssl_ctx_, tls_config_.cert_path.c_str());
    }
    if (!tls_config_.key_path.empty())
    {
        SSL_CTX_use_PrivateKey_file(ssl_ctx_, tls_config_.key_path.c_str(), SSL_FILETYPE_PEM);
    }

    return ok();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_client<TlsPolicy>::do_handshake() -> VoidResult
    requires is_secure
{
    std::mutex handshake_mutex;
    std::condition_variable handshake_cv;
    bool handshake_done = false;
    std::error_code handshake_error;

    socket_->async_handshake(
        internal::dtls_socket::handshake_type::client,
        [&](std::error_code ec) {
            {
                std::lock_guard<std::mutex> lock(handshake_mutex);
                handshake_done = true;
                handshake_error = ec;
            }
            handshake_cv.notify_one();
        });

    {
        std::unique_lock<std::mutex> lock(handshake_mutex);
        if (!handshake_cv.wait_for(
                lock, std::chrono::seconds(30), [&] { return handshake_done; }))
        {
            return error_void(error_codes::network_system::connection_timeout,
                              "DTLS handshake timeout",
                              "unified_udp_messaging_client::do_handshake");
        }
    }

    if (handshake_error)
    {
        return error_void(error_codes::network_system::connection_failed,
                          "DTLS handshake failed: " + handshake_error.message(),
                          "unified_udp_messaging_client::do_handshake");
    }

    return ok();
}
#endif

}  // namespace kcenon::network::core
