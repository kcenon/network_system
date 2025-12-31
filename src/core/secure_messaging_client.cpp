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

#include "kcenon/network/core/secure_messaging_client.h"

#include "kcenon/network/core/network_context.h"
#include "kcenon/network/integration/logger_integration.h"

namespace kcenon::network::core {

using tcp = asio::ip::tcp;

secure_messaging_client::secure_messaging_client(const std::string &client_id,
                                                 bool verify_cert)
    : client_id_(client_id), verify_cert_(verify_cert) {
  // Initialize SSL context with TLS 1.3 support (TICKET-009: TLS 1.3 only by default)
  ssl_context_ =
      std::make_unique<asio::ssl::context>(asio::ssl::context::tls_client);

  // Set SSL context options - disable all legacy protocols
  ssl_context_->set_options(
      asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
      asio::ssl::context::no_sslv3 | asio::ssl::context::no_tlsv1 |
      asio::ssl::context::no_tlsv1_1);

  // Enforce TLS 1.3 minimum version using native OpenSSL handle
  // This prevents protocol downgrade attacks (CVSS 7.5)
  SSL_CTX *native_ctx = ssl_context_->native_handle();
  if (native_ctx) {
    // Set minimum protocol version to TLS 1.3 (security hardening)
    SSL_CTX_set_min_proto_version(native_ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(native_ctx, TLS1_3_VERSION);

    // Set strong cipher suites for TLS 1.3
    SSL_CTX_set_ciphersuites(native_ctx,
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256:"
        "TLS_AES_128_GCM_SHA256");
  }

  if (verify_cert_) {
    // Set verification mode to verify peer certificate
    ssl_context_->set_verify_mode(asio::ssl::verify_peer);

    // Load default certificate paths for verification
    ssl_context_->set_default_verify_paths();

    NETWORK_LOG_INFO("[secure_messaging_client] SSL context initialized with "
                     "TLS 1.3 only and certificate verification");
  } else {
    // No certificate verification
    ssl_context_->set_verify_mode(asio::ssl::verify_none);

    NETWORK_LOG_WARN("[secure_messaging_client] SSL context initialized with "
                     "TLS 1.3 only WITHOUT certificate verification");
  }
}

secure_messaging_client::~secure_messaging_client() noexcept {
  try {
    // Ignore the return value in destructor to avoid throwing
    (void)stop_client();
  } catch (...) {
    // Destructor must not throw - swallow all exceptions
  }
}

auto secure_messaging_client::start_client(const std::string &host,
                                           unsigned short port) -> VoidResult {
  if (is_connected_.load()) {
    return error_void(error_codes::network_system::connection_failed,
                      "Secure client is already connected",
                      "secure_messaging_client::start_client",
                      "Client ID: " + client_id_);
  }

  try {
    // Create io_context
    io_context_ = std::make_unique<asio::io_context>();

    // Create work guard to keep io_context running
    work_guard_ = std::make_unique<
        asio::executor_work_guard<asio::io_context::executor_type>>(
        asio::make_work_guard(*io_context_));

    // Resolve the server address
    tcp::resolver resolver(*io_context_);
    auto endpoints = resolver.resolve(host, std::to_string(port));

    // Create TCP socket first (not SSL socket yet)
    tcp::socket tcp_sock(*io_context_);

    // Connect to server (plain TCP)
    std::error_code connect_ec;
    asio::connect(tcp_sock, endpoints, connect_ec);

    if (connect_ec) {
      return error_void(error_codes::network_system::connection_failed,
                        "Failed to connect to server: " + connect_ec.message(),
                        "secure_messaging_client::start_client",
                        "Host: " + host + ", Port: " + std::to_string(port));
    }

    // Now create secure socket with connected TCP socket
    socket_ = std::make_shared<internal::secure_tcp_socket>(std::move(tcp_sock),
                                                            *ssl_context_);

    // Set up callbacks
    auto self = shared_from_this();
    socket_->set_receive_callback(
        [this, self](const std::vector<uint8_t> &data) { on_receive(data); });
    socket_->set_error_callback(
        [this, self](std::error_code ec) { on_error(ec); });

    // Perform SSL handshake (synchronously for now)
    std::promise<std::error_code> handshake_promise;
    auto handshake_future = handshake_promise.get_future();

    socket_->async_handshake(asio::ssl::stream_base::client,
                             [&handshake_promise](std::error_code ec) {
                               handshake_promise.set_value(ec);
                             });

    // Get thread pool from network context
    thread_pool_ = network_context::instance().get_thread_pool();
    if (!thread_pool_) {
      // Fallback: create a temporary thread pool if network_context is not
      // initialized
      NETWORK_LOG_WARN(
          "[secure_messaging_client::" + client_id_ +
          "] network_context not initialized, creating temporary thread pool");
      thread_pool_ = std::make_shared<integration::basic_thread_pool>(
          std::thread::hardware_concurrency());
    }

    // Start io_context on thread pool for handshake to complete
    io_context_future_ = thread_pool_->submit([this]() {
      try {
        NETWORK_LOG_DEBUG("[secure_messaging_client::" + client_id_ +
                          "] io_context started");
        io_context_->run();
        NETWORK_LOG_DEBUG("[secure_messaging_client::" + client_id_ +
                          "] io_context stopped");
      } catch (const std::exception &e) {
        NETWORK_LOG_ERROR(
            "[secure_messaging_client::" + client_id_ +
            "] Exception in io_context: " + std::string(e.what()));
      }
    });

    // Wait for handshake to complete (with timeout)
    auto status = handshake_future.wait_for(std::chrono::seconds(10));

    if (status == std::future_status::timeout) {
      stop_client();
      return error_void(error_codes::network_system::connection_timeout,
                        "SSL handshake timeout",
                        "secure_messaging_client::start_client",
                        "Host: " + host + ", Port: " + std::to_string(port));
    }

    auto handshake_ec = handshake_future.get();
    if (handshake_ec) {
      stop_client();
      return error_void(error_codes::network_system::connection_failed,
                        "SSL handshake failed: " + handshake_ec.message(),
                        "secure_messaging_client::start_client",
                        "Host: " + host + ", Port: " + std::to_string(port));
    }

    // Begin reading after successful handshake
    socket_->start_read();

    is_connected_.store(true);

    NETWORK_LOG_INFO("[secure_messaging_client] Connected to " + host + ":" +
                     std::to_string(port) + " (TLS/SSL secured)");

    // Invoke connected callback if set
    {
      std::lock_guard<std::mutex> lock(callback_mutex_);
      if (connected_callback_) {
        try {
          connected_callback_();
        } catch (const std::exception &e) {
          NETWORK_LOG_ERROR(
              "[secure_messaging_client] Exception in connected callback: " +
              std::string(e.what()));
        }
      }
    }

    return ok();
  } catch (const std::system_error &e) {
    return error_void(error_codes::network_system::connection_failed,
                      "Failed to connect: " + std::string(e.what()),
                      "secure_messaging_client::start_client",
                      "Host: " + host + ", Port: " + std::to_string(port));
  } catch (const std::exception &e) {
    return error_void(error_codes::common_errors::internal_error,
                      "Failed to connect: " + std::string(e.what()),
                      "secure_messaging_client::start_client",
                      "Host: " + host + ", Port: " + std::to_string(port));
  }
}

auto secure_messaging_client::stop_client() -> VoidResult {
  if (!is_connected_.exchange(false)) {
    return ok(); // Already disconnected
  }

  try {
    // Close socket safely using atomic close() method
    // This prevents data races between close and async read operations
    if (socket_) {
      socket_->close();
    }

    // Release work guard
    if (work_guard_) {
      work_guard_.reset();
    }

    // Stop io_context
    if (io_context_) {
      io_context_->stop();
    }

    // Wait for io_context task to complete
    if (io_context_future_.valid()) {
      try {
        io_context_future_.wait();
      } catch (const std::exception &e) {
        NETWORK_LOG_ERROR("[secure_messaging_client::" + client_id_ +
                          "] Exception while waiting for io_context: " +
                          std::string(e.what()));
      }
    }

    // Release resources
    socket_.reset();
    thread_pool_.reset();
    io_context_.reset();

    NETWORK_LOG_INFO("[secure_messaging_client] Disconnected.");
    return ok();
  } catch (const std::exception &e) {
    return error_void(error_codes::common_errors::internal_error,
                      "Failed to stop client: " + std::string(e.what()),
                      "secure_messaging_client::stop_client",
                      "Client ID: " + client_id_);
  }
}

auto secure_messaging_client::send_packet(std::vector<uint8_t> &&data)
    -> VoidResult {
  if (!is_connected_.load()) {
    return error_void(
        error_codes::network_system::send_failed, "Client is not connected",
        "secure_messaging_client::send_packet", "Client ID: " + client_id_);
  }

  if (!socket_) {
    return error_void(
        error_codes::network_system::send_failed, "Socket is not available",
        "secure_messaging_client::send_packet", "Client ID: " + client_id_);
  }

  // Send data directly over the secure connection
  socket_->async_send(
      std::move(data), [](std::error_code ec, std::size_t bytes_transferred) {
        if (ec) {
          NETWORK_LOG_ERROR("[secure_messaging_client] Send error: " +
                            ec.message());
        } else {
          NETWORK_LOG_DEBUG("[secure_messaging_client] Sent " +
                            std::to_string(bytes_transferred) + " bytes");
        }
      });

  return ok();
}

auto secure_messaging_client::on_receive(const std::vector<uint8_t> &data)
    -> void {
  if (!is_connected_.load()) {
    return;
  }

  NETWORK_LOG_DEBUG("[secure_messaging_client] Received " +
                    std::to_string(data.size()) + " bytes.");

  // Invoke receive callback if set
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (receive_callback_) {
      try {
        receive_callback_(data);
      } catch (const std::exception &e) {
        NETWORK_LOG_ERROR(
            "[secure_messaging_client] Exception in receive callback: " +
            std::string(e.what()));
      }
    }
  }

  // Handle received data here
  // In a real implementation, you might dispatch to a callback or queue
}

auto secure_messaging_client::on_error(std::error_code ec) -> void {
  NETWORK_LOG_ERROR("[secure_messaging_client] Socket error: " + ec.message());

  // Invoke error callback if set
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
      try {
        error_callback_(ec);
      } catch (const std::exception &e) {
        NETWORK_LOG_ERROR(
            "[secure_messaging_client] Exception in error callback: " +
            std::string(e.what()));
      }
    }
  }

  // Invoke disconnected callback if was connected
  if (is_connected_.load()) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (disconnected_callback_) {
      try {
        disconnected_callback_();
      } catch (const std::exception &e) {
        NETWORK_LOG_ERROR(
            "[secure_messaging_client] Exception in disconnected callback: " +
            std::string(e.what()));
      }
    }
  }

  is_connected_.store(false);
}

auto secure_messaging_client::set_receive_callback(
    std::function<void(const std::vector<uint8_t> &)> callback) -> void {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  receive_callback_ = std::move(callback);
}

auto secure_messaging_client::set_connected_callback(
    std::function<void()> callback) -> void {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  connected_callback_ = std::move(callback);
}

auto secure_messaging_client::set_disconnected_callback(
    std::function<void()> callback) -> void {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  disconnected_callback_ = std::move(callback);
}

auto secure_messaging_client::set_error_callback(
    std::function<void(std::error_code)> callback) -> void {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  error_callback_ = std::move(callback);
}

} // namespace kcenon::network::core
