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

#include "kcenon/network/detail/session/secure_session.h"

#include "internal/integration/logger_integration.h"

namespace kcenon::network::session {

secure_session::secure_session(asio::ip::tcp::socket socket,
                               asio::ssl::context &ssl_context,
                               std::string_view server_id)
    : server_id_(server_id) {
  // Create the secure_tcp_socket wrapper
  socket_ = std::make_shared<internal::secure_tcp_socket>(std::move(socket),
                                                          ssl_context);
}

secure_session::~secure_session() noexcept {
  try {
    // Call stop_session to clean up resources
    stop_session();
  } catch (...) {
    // Destructor must not throw - swallow all exceptions
  }
}

auto secure_session::start_session() -> void {
  if (is_stopped_.load()) {
    return;
  }

  // Perform SSL handshake first
  auto self = shared_from_this();
  socket_->async_handshake(
      asio::ssl::stream_base::server, [this, self](std::error_code ec) {
        if (ec) {
          NETWORK_LOG_ERROR("[secure_session] SSL handshake failed: " +
                            ec.message());
          stop_session();
          return;
        }

        NETWORK_LOG_INFO("[secure_session] SSL handshake completed");

        // Set up callbacks after successful handshake
        socket_->set_receive_callback(
            [this, self](const std::vector<uint8_t> &data) {
              on_receive(data);
            });
        socket_->set_error_callback(
            [this, self](std::error_code ec) { on_error(ec); });

        // Begin reading
        socket_->start_read();

        NETWORK_LOG_INFO("[secure_session] Started secure session on server: " +
                         server_id_);
      });
}

auto secure_session::stop_session() -> void {
  if (is_stopped_.exchange(true)) {
    return;
  }
  // Close socket safely using atomic close() method
  // This prevents data races between close and async read operations
  if (socket_) {
    socket_->close();
  }

  // Invoke disconnection callback if set
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (disconnection_callback_) {
      try {
        disconnection_callback_(server_id_);
      } catch (const std::exception &e) {
        NETWORK_LOG_ERROR(
            "[secure_session] Exception in disconnection callback: " +
            std::string(e.what()));
      }
    }
  }

  NETWORK_LOG_INFO("[secure_session] Stopped.");
}

auto secure_session::send_packet(std::vector<uint8_t> &&data) -> void {
  if (is_stopped_.load()) {
    return;
  }

  // Send data directly over the secure connection
  socket_->async_send(
      std::move(data), [](std::error_code ec, std::size_t bytes_transferred) {
        if (ec) {
          NETWORK_LOG_ERROR("[secure_session] Send error: " + ec.message());
        } else {
          NETWORK_LOG_DEBUG("[secure_session] Sent " +
                            std::to_string(bytes_transferred) + " bytes");
        }
      });
}

auto secure_session::on_receive(const std::vector<uint8_t> &data) -> void {
  if (is_stopped_.load()) {
    return;
  }

  NETWORK_LOG_DEBUG("[secure_session] Received " + std::to_string(data.size()) +
                    " bytes.");

  // Check queue size before adding
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    size_t queue_size = pending_messages_.size();

    // Apply backpressure if queue is getting full
    if (queue_size >= max_pending_messages_) {
      NETWORK_LOG_WARN("[secure_session] Queue size (" +
                       std::to_string(queue_size) + ") reached limit (" +
                       std::to_string(max_pending_messages_) +
                       "). Applying backpressure.");

      // If queue is severely overflowing, disconnect the session
      if (queue_size >= max_pending_messages_ * 2) {
        NETWORK_LOG_ERROR("[secure_session] Queue overflow (" +
                          std::to_string(queue_size) +
                          "). Disconnecting abusive client.");
        stop_session();
        return;
      }
    }

    // Add message to queue
    pending_messages_.push_back(data);
  }

  // Process messages from queue
  process_next_message();
}

auto secure_session::on_error(std::error_code ec) -> void {
  NETWORK_LOG_ERROR("[secure_session] Socket error: " + ec.message());

  // Invoke error callback if set
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
      try {
        error_callback_(ec);
      } catch (const std::exception &e) {
        NETWORK_LOG_ERROR("[secure_session] Exception in error callback: " +
                          std::string(e.what()));
      }
    }
  }

  stop_session();
}

auto secure_session::process_next_message() -> void {
  std::vector<uint8_t> message;

  // Dequeue one message
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (pending_messages_.empty()) {
      return;
    }

    message = std::move(pending_messages_.front());
    pending_messages_.pop_front();
  }

  // Process the message
  NETWORK_LOG_DEBUG(
      "[secure_session] Processing message of " +
      std::to_string(message.size()) +
      " bytes. Queue remaining: " + std::to_string(pending_messages_.size()));

  // Invoke receive callback if set
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (receive_callback_) {
      try {
        receive_callback_(message);
      } catch (const std::exception &e) {
        NETWORK_LOG_ERROR("[secure_session] Exception in receive callback: " +
                          std::string(e.what()));
      }
    }
  }

  // If there are more messages, process them asynchronously
  // to avoid blocking the receive thread
  std::lock_guard<std::mutex> lock(queue_mutex_);
  if (!pending_messages_.empty()) {
    // In a production system, you might want to post this to a thread pool
    // or use a work queue to process messages asynchronously
    // For now, we just process one message per call
  }
}

auto secure_session::set_receive_callback(
    std::function<void(const std::vector<uint8_t> &)> callback) -> void {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  receive_callback_ = std::move(callback);
}

auto secure_session::set_disconnection_callback(
    std::function<void(const std::string &)> callback) -> void {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  disconnection_callback_ = std::move(callback);
}

auto secure_session::set_error_callback(
    std::function<void(std::error_code)> callback) -> void {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  error_callback_ = std::move(callback);
}

} // namespace kcenon::network::session
