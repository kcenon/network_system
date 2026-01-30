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

#include "kcenon/network/core/network_context.h"
#include "network_udp/internal/udp_socket.h"
#include "kcenon/network/integration/logger_integration.h"

#ifdef BUILD_TLS_SUPPORT
#include "kcenon/network/internal/dtls_socket.h"
#endif

namespace kcenon::network::core
{

using udp = asio::ip::udp;

// =====================================================================
// Constructor / Destructor
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
unified_udp_messaging_server<TlsPolicy>::unified_udp_messaging_server(std::string_view server_id)
    requires(!policy::is_tls_enabled_v<TlsPolicy>)
    : server_id_(server_id)
{
}

#ifdef BUILD_TLS_SUPPORT
template <policy::TlsPolicy TlsPolicy>
unified_udp_messaging_server<TlsPolicy>::unified_udp_messaging_server(
    std::string_view server_id,
    const TlsPolicy& tls_config)
    requires policy::is_tls_enabled_v<TlsPolicy>
    : server_id_(server_id)
    , tls_config_(tls_config)
{
}
#endif

template <policy::TlsPolicy TlsPolicy>
unified_udp_messaging_server<TlsPolicy>::~unified_udp_messaging_server() noexcept
{
    if (lifecycle_.is_running())
    {
        [[maybe_unused]] auto _ = stop_server();
    }
}

// =====================================================================
// Lifecycle Management
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::start_server(uint16_t port) -> VoidResult
{
    if (lifecycle_.is_running())
    {
        return error_void(error_codes::network_system::server_already_running,
                          "UDP server is already running",
                          "unified_udp_messaging_server::start_server");
    }

#ifdef BUILD_TLS_SUPPORT
    if constexpr (is_secure)
    {
        if (tls_config_.cert_path.empty() || tls_config_.key_path.empty())
        {
            return error_void(error_codes::common_errors::invalid_argument,
                              "Certificate and private key files must be set for DTLS server",
                              "unified_udp_messaging_server::start_server");
        }
    }
#endif

    lifecycle_.set_running();

    auto result = do_start_impl(port);
    if (result.is_err())
    {
        lifecycle_.mark_stopped();
    }

    return result;
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::stop_server() -> VoidResult
{
    if (!lifecycle_.prepare_stop())
    {
        return ok();  // Not running or already stopping
    }

    auto result = do_stop_impl();

    lifecycle_.mark_stopped();

    return result;
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::server_id() const -> const std::string&
{
    return server_id_;
}

// =====================================================================
// i_network_component interface implementation
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::is_running() const -> bool
{
    return lifecycle_.is_running();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::wait_for_stop() -> void
{
    lifecycle_.wait_for_stop();
}

// =====================================================================
// i_udp_server interface implementation
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::start(uint16_t port) -> VoidResult
{
    return start_server(port);
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::stop() -> VoidResult
{
    return stop_server();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::send_to(
    const interfaces::i_udp_server::endpoint_info& endpoint,
    std::vector<uint8_t>&& data,
    interfaces::i_udp_server::send_callback_t handler) -> VoidResult
{
    if (!lifecycle_.is_running())
    {
        return error_void(error_codes::network_system::server_not_started,
                          "UDP server is not running",
                          "unified_udp_messaging_server::send_to");
    }

    try
    {
        asio::ip::udp::endpoint asio_endpoint(asio::ip::make_address(endpoint.address),
                                              endpoint.port);
        async_send_to(std::move(data), asio_endpoint, std::move(handler));
        return ok();
    }
    catch (const std::exception& e)
    {
        return error_void(error_codes::common_errors::internal_error,
                          std::string("Failed to send datagram: ") + e.what(),
                          "unified_udp_messaging_server::send_to",
                          "Target: " + endpoint.address + ":" + std::to_string(endpoint.port));
    }
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::set_receive_callback(
    interfaces::i_udp_server::receive_callback_t callback) -> void
{
    if (!callback)
    {
        callbacks_.template set<to_index(callback_index::receive)>(nullptr);
        return;
    }

    callbacks_.template set<to_index(callback_index::receive)>(
        [callback = std::move(callback)](const std::vector<uint8_t>& data,
                                         const asio::ip::udp::endpoint& endpoint) {
            interfaces::i_udp_server::endpoint_info info;
            info.address = endpoint.address().to_string();
            info.port = endpoint.port();
            callback(data, info);
        });
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::set_receive_callback(receive_callback_t callback)
    -> void
{
    callbacks_.template set<to_index(callback_index::receive)>(std::move(callback));
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::set_error_callback(error_callback_t callback) -> void
{
    callbacks_.template set<to_index(callback_index::error)>(std::move(callback));
}

// =====================================================================
// Extended API
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::set_client_connected_callback(
    client_connected_callback_t callback) -> void
{
    callbacks_.template set<to_index(callback_index::client_connected)>(std::move(callback));
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::set_client_disconnected_callback(
    client_disconnected_callback_t callback) -> void
{
    callbacks_.template set<to_index(callback_index::client_disconnected)>(std::move(callback));
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::async_send_to(
    std::vector<uint8_t>&& data,
    const asio::ip::udp::endpoint& endpoint,
    std::function<void(std::error_code, std::size_t)> handler) -> void
{
    if constexpr (is_secure)
    {
#ifdef BUILD_TLS_SUPPORT
        std::shared_ptr<dtls_session> session;
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            auto it = sessions_.find(endpoint);
            if (it != sessions_.end())
            {
                session = it->second;
            }
        }

        if (!session || !session->socket || !session->handshake_complete)
        {
            if (handler)
            {
                handler(std::make_error_code(std::errc::not_connected), 0);
            }
            return;
        }

        session->socket->async_send_to(std::move(data), endpoint, std::move(handler));
#endif
    }
    else
    {
        if (!socket_)
        {
            if (handler)
            {
                handler(std::make_error_code(std::errc::not_connected), 0);
            }
            return;
        }

        socket_->async_send_to(std::move(data), endpoint, std::move(handler));
    }
}

// =====================================================================
// Internal Callback Helpers
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::invoke_receive_callback(
    const std::vector<uint8_t>& data,
    const asio::ip::udp::endpoint& endpoint) -> void
{
    callbacks_.template invoke<to_index(callback_index::receive)>(data, endpoint);
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::invoke_error_callback(std::error_code ec) -> void
{
    callbacks_.template invoke<to_index(callback_index::error)>(ec);
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::get_receive_callback() const -> receive_callback_t
{
    return callbacks_.template get<to_index(callback_index::receive)>();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::get_error_callback() const -> error_callback_t
{
    return callbacks_.template get<to_index(callback_index::error)>();
}

// =====================================================================
// Internal Implementation Methods
// =====================================================================

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::do_start_impl(uint16_t port) -> VoidResult
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

        if constexpr (is_secure)
        {
#ifdef BUILD_TLS_SUPPORT
            dtls_socket_ = std::make_unique<asio::ip::udp::socket>(
                *io_context_, udp::endpoint(udp::v4(), port));
#endif
        }
        else
        {
            asio::ip::udp::socket raw_socket(*io_context_, udp::endpoint(udp::v4(), port));
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
            NETWORK_LOG_WARN("[unified_udp_messaging_server] network_context not initialized, "
                             "creating temporary thread pool");
            thread_pool_ = std::make_shared<integration::basic_thread_pool>(
                std::thread::hardware_concurrency());
        }

        io_context_future_ = thread_pool_->submit([this]() {
            try
            {
                auto work_guard = asio::make_work_guard(*io_context_);
                NETWORK_LOG_DEBUG("[unified_udp_messaging_server] io_context started");
                io_context_->run();
                NETWORK_LOG_DEBUG("[unified_udp_messaging_server] io_context stopped");
            }
            catch (const std::exception& e)
            {
                NETWORK_LOG_ERROR(std::string("Worker thread exception: ") + e.what());
            }
        });

#ifdef BUILD_TLS_SUPPORT
        if constexpr (is_secure)
        {
            do_receive();
        }
#endif

        NETWORK_LOG_INFO("UDP server started on port " + std::to_string(port));

        return ok();
    }
    catch (const std::system_error& e)
    {
        return error_void(error_codes::network_system::bind_failed,
                          std::string("Failed to bind UDP socket: ") + e.what(),
                          "unified_udp_messaging_server::do_start_impl",
                          "Port: " + std::to_string(port));
    }
    catch (const std::exception& e)
    {
        return error_void(error_codes::common_errors::internal_error,
                          std::string("Failed to start UDP server: ") + e.what(),
                          "unified_udp_messaging_server::do_start_impl",
                          "Port: " + std::to_string(port));
    }
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::do_stop_impl() -> VoidResult
{
    try
    {
        if constexpr (is_secure)
        {
#ifdef BUILD_TLS_SUPPORT
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                for (auto& [endpoint, session] : sessions_)
                {
                    if (session && session->socket)
                    {
                        session->socket->stop_receive();
                    }
                }
                sessions_.clear();
            }

            if (dtls_socket_)
            {
                std::error_code ec;
                dtls_socket_->close(ec);
                dtls_socket_.reset();
            }
#endif
        }
        else
        {
            if (socket_)
            {
                socket_->stop_receive();
                socket_.reset();
            }
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
                NETWORK_LOG_ERROR("[unified_udp_messaging_server] Exception while waiting for "
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

        NETWORK_LOG_INFO("UDP server stopped");

        return ok();
    }
    catch (const std::exception& e)
    {
        return error_void(error_codes::common_errors::internal_error,
                          std::string("Failed to stop UDP server: ") + e.what(),
                          "unified_udp_messaging_server::do_stop_impl");
    }
}

#ifdef BUILD_TLS_SUPPORT
template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::init_ssl_context() -> VoidResult
    requires is_secure
{
    ssl_ctx_ = SSL_CTX_new(DTLS_server_method());
    if (!ssl_ctx_)
    {
        return error_void(error_codes::common_errors::internal_error,
                          "Failed to create DTLS context",
                          "unified_udp_messaging_server::init_ssl_context");
    }

    SSL_CTX_set_options(ssl_ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

    if (!tls_config_.cert_path.empty())
    {
        if (SSL_CTX_use_certificate_chain_file(ssl_ctx_, tls_config_.cert_path.c_str()) != 1)
        {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = nullptr;
            return error_void(error_codes::common_errors::internal_error,
                              "Failed to load certificate: " + tls_config_.cert_path,
                              "unified_udp_messaging_server::init_ssl_context");
        }
    }

    if (!tls_config_.key_path.empty())
    {
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, tls_config_.key_path.c_str(), SSL_FILETYPE_PEM) !=
            1)
        {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = nullptr;
            return error_void(error_codes::common_errors::internal_error,
                              "Failed to load private key: " + tls_config_.key_path,
                              "unified_udp_messaging_server::init_ssl_context");
        }

        if (SSL_CTX_check_private_key(ssl_ctx_) != 1)
        {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = nullptr;
            return error_void(error_codes::common_errors::internal_error,
                              "Private key does not match certificate",
                              "unified_udp_messaging_server::init_ssl_context");
        }
    }

    if (tls_config_.verify_peer)
    {
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, nullptr);
        if (!tls_config_.ca_path.empty())
        {
            SSL_CTX_load_verify_locations(ssl_ctx_, tls_config_.ca_path.c_str(), nullptr);
        }
    }
    else
    {
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, nullptr);
    }

    return ok();
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::do_receive() -> void
    requires is_secure
{
    if (!lifecycle_.is_running() || !dtls_socket_)
    {
        return;
    }

    auto self = this->shared_from_this();
    dtls_socket_->async_receive_from(
        asio::buffer(read_buffer_),
        sender_endpoint_,
        [this, self](std::error_code ec, std::size_t length) {
            if (!lifecycle_.is_running())
            {
                return;
            }

            if (ec)
            {
                if (ec != asio::error::operation_aborted)
                {
                    invoke_error_callback(ec);
                }
                return;
            }

            if (length > 0)
            {
                std::vector<uint8_t> data(read_buffer_.begin(), read_buffer_.begin() + length);
                process_session_data(data, sender_endpoint_);
            }

            if (lifecycle_.is_running())
            {
                do_receive();
            }
        });
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::process_session_data(
    [[maybe_unused]] const std::vector<uint8_t>& data,
    const asio::ip::udp::endpoint& sender) -> void
    requires is_secure
{
    std::shared_ptr<dtls_session> session;

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(sender);
        if (it != sessions_.end())
        {
            session = it->second;
        }
    }

    if (!session)
    {
        session = create_session(sender);
        if (!session)
        {
            return;
        }
    }
}

template <policy::TlsPolicy TlsPolicy>
auto unified_udp_messaging_server<TlsPolicy>::create_session(
    const asio::ip::udp::endpoint& client_endpoint) -> std::shared_ptr<dtls_session>
    requires is_secure
{
    try
    {
        asio::ip::udp::socket client_socket(*io_context_, asio::ip::udp::v4());

        auto session = std::make_shared<dtls_session>();
        session->socket =
            std::make_shared<internal::dtls_socket>(std::move(client_socket), ssl_ctx_);
        session->socket->set_peer_endpoint(client_endpoint);

        auto self = this->shared_from_this();
        session->socket->set_receive_callback(
            [this, self, client_endpoint](const std::vector<uint8_t>& data,
                                          const asio::ip::udp::endpoint& /*sender*/) {
                invoke_receive_callback(data, client_endpoint);
            });

        session->socket->async_handshake(
            internal::dtls_socket::handshake_type::server,
            [this, self, session, client_endpoint](std::error_code ec) {
                if (ec)
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_.erase(client_endpoint);
                    return;
                }

                session->handshake_complete = true;

                auto callback =
                    callbacks_.template get<to_index(callback_index::client_connected)>();
                if (callback)
                {
                    callback(client_endpoint);
                }
            });

        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_[client_endpoint] = session;
        }

        return session;
    }
    catch (const std::exception& /*e*/)
    {
        return nullptr;
    }
}
#endif

}  // namespace kcenon::network::core
