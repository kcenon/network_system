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

#include "kcenon/network/core/messaging_client.h"
#include "kcenon/network/integration/logger_integration.h"
#include "kcenon/network/integration/io_context_thread_manager.h"
#include "kcenon/network/tracing/span.h"
#include "kcenon/network/tracing/trace_context.h"
#include "kcenon/network/tracing/tracing_config.h"
#include <chrono>
#include <span>
#include <string_view>

// Use nested namespace definition (C++17)
namespace kcenon::network::core {

using tcp = asio::ip::tcp;

// Use string_view for better efficiency (C++17)
messaging_client::messaging_client(std::string_view client_id)
    : client_id_(client_id) {
}

messaging_client::~messaging_client() noexcept {
  if (is_running()) {
    (void)stop_client();
  }
}

auto messaging_client::start_client(std::string_view host, unsigned short port)
    -> VoidResult {
  // Create tracing span for client start operation
  auto span = tracing::is_tracing_enabled()
      ? std::make_optional(tracing::trace_context::create_span("tcp.client.start"))
      : std::nullopt;
  if (span) {
    span->set_attribute("net.peer.name", host)
         .set_attribute("net.peer.port", static_cast<int64_t>(port))
         .set_attribute("net.transport", "tcp")
         .set_attribute("client.id", client_id_);
  }

  if (host.empty()) {
    if (span) {
      span->set_error("Host cannot be empty");
    }
    return error_void(error_codes::common_errors::invalid_argument,
                      "Host cannot be empty",
                      "messaging_client::start_client");
  }

  // Reset connection state before start
  is_connected_.store(false, std::memory_order_release);

  // Use base class do_start which handles lifecycle management
  auto result = do_start(host, port);
  if (result.is_err()) {
    if (span) {
      span->set_error(result.error().message);
    }
  } else {
    if (span) {
      span->set_status(tracing::span_status::ok);
    }
  }

  return result;
}

auto messaging_client::stop_client() -> VoidResult {
  // Use base class do_stop which handles lifecycle management and calls on_stopped()
  // Note: do_stop() sets stop_initiated_ first, then calls do_stop_impl() which
  // handles is_connected_ reset and async operation cleanup in the correct order.
  return do_stop();
}

auto messaging_client::on_stopped() -> void {
  // Invoke disconnected callback after stop completes
  callbacks_.invoke<to_index(callback_index::disconnected)>();
}

// Note: wait_for_stop() and is_running() are inherited from startable_base

auto messaging_client::is_connected() const noexcept -> bool {
  return is_connected_.load(std::memory_order_acquire);
}

auto messaging_client::client_id() const -> const std::string& {
  return client_id_;
}

auto messaging_client::send_packet(std::vector<uint8_t>&& data) -> VoidResult {
  // Create tracing span for send operation
  auto span = tracing::is_tracing_enabled()
      ? std::make_optional(tracing::trace_context::create_span("tcp.client.send"))
      : std::nullopt;
  if (span) {
    span->set_attribute("net.transport", "tcp")
         .set_attribute("message.size", static_cast<int64_t>(data.size()))
         .set_attribute("client.id", client_id_);
  }

  if (!is_connected()) {
    if (span) {
      span->set_error("Client is not connected");
    }
    return error_void(error_codes::network_system::connection_closed,
                      "Client is not connected",
                      "messaging_client::send_packet",
                      "Client ID: " + client_id_);
  }

  if (data.empty()) {
    if (span) {
      span->set_error("Data cannot be empty");
    }
    return error_void(error_codes::common_errors::invalid_argument,
                      "Data cannot be empty",
                      "messaging_client::send_packet");
  }

  auto result = do_send_impl(std::move(data));
  if (span) {
    if (result.is_err()) {
      span->set_error(result.error().message);
    } else {
      span->set_status(tracing::span_status::ok);
    }
  }
  return result;
}

auto messaging_client::set_receive_callback(receive_callback_t callback) -> void {
  callbacks_.set<to_index(callback_index::receive)>(std::move(callback));
}

auto messaging_client::set_connected_callback(connected_callback_t callback) -> void {
  callbacks_.set<to_index(callback_index::connected)>(std::move(callback));
}

auto messaging_client::set_disconnected_callback(disconnected_callback_t callback) -> void {
  callbacks_.set<to_index(callback_index::disconnected)>(std::move(callback));
}

auto messaging_client::set_error_callback(error_callback_t callback) -> void {
  callbacks_.set<to_index(callback_index::error)>(std::move(callback));
}

auto messaging_client::set_connected(bool connected) -> void {
  is_connected_.store(connected, std::memory_order_release);
}

auto messaging_client::invoke_receive_callback(const std::vector<uint8_t>& data) -> void {
  callbacks_.invoke<to_index(callback_index::receive)>(data);
}

auto messaging_client::invoke_connected_callback() -> void {
  callbacks_.invoke<to_index(callback_index::connected)>();
}

auto messaging_client::invoke_disconnected_callback() -> void {
  callbacks_.invoke<to_index(callback_index::disconnected)>();
}

auto messaging_client::invoke_error_callback(std::error_code ec) -> void {
  callbacks_.invoke<to_index(callback_index::error)>(ec);
}

// TCP-specific implementation of client start
auto messaging_client::do_start_impl(std::string_view host, unsigned short port)
    -> VoidResult {
  try {
    {
      std::lock_guard<std::mutex> lock(socket_mutex_);
      socket_.reset();
    }

    // Use Intentional Leak pattern for io_context to prevent heap corruption
    // during static destruction phase. The no-op deleter ensures io_context
    // survives until process termination, avoiding issues when pending async
    // operations' handlers reference io_context internals.
    // Memory impact: ~few KB per client (reclaimed by OS on process termination)
    io_context_ = std::shared_ptr<asio::io_context>(
        new asio::io_context(),
        [](asio::io_context*) { /* no-op deleter - intentional leak */ });
    // Create work guard to keep io_context running
    work_guard_ = std::make_unique<
        asio::executor_work_guard<asio::io_context::executor_type>>(
        asio::make_work_guard(*io_context_));

    // Use io_context_thread_manager for unified thread management
    io_context_future_ = integration::io_context_thread_manager::instance()
        .run_io_context(io_context_, "messaging_client:" + client_id());

    do_connect(host, port);

    // NOTE: No logging here to prevent heap corruption during static destruction.
    // When common_system uses async logging, log messages may still be queued
    // when static destruction begins, causing heap corruption when the logging
    // thread accesses destroyed GlobalLoggerRegistry.

    return ok();
  } catch (const std::exception &e) {
    return error_void(error_codes::common_errors::internal_error,
                      "Failed to start client: " + std::string(e.what()),
                      "messaging_client::do_start",
                      "Client ID: " + client_id() +
                          ", Host: " + std::string(host));
  }
}

// TCP-specific implementation of client stop
auto messaging_client::do_stop_impl() -> VoidResult {
  // NOTE: No logging in do_stop to prevent heap corruption during static
  // destruction. This method may be called from destructor when
  // GlobalLoggerRegistry is already destroyed.

  // Mark as disconnected first (stop_initiated_ is already set by base class do_stop())
  is_connected_.store(false, std::memory_order_release);

  try {
    // Swap out socket with mutex protection and close outside lock
    std::shared_ptr<internal::tcp_socket> local_socket;
    {
      std::lock_guard<std::mutex> lock(socket_mutex_);
      local_socket = std::move(socket_);
    }
    // Close socket safely using tcp_socket::close() which posts to the socket's
    // executor for thread-safety with async operations.
    if (local_socket) {
      local_socket->close();
    }

    // Cancel pending connection operations by posting to io_context.
    // This ensures socket operations happen on the same thread as async_connect,
    // preventing data races detected by ThreadSanitizer.
    if (io_context_) {
      std::shared_ptr<asio::ip::tcp::resolver> resolver_to_cancel;
      std::shared_ptr<asio::ip::tcp::socket> socket_to_close;
      {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        resolver_to_cancel = pending_resolver_;
        socket_to_close = pending_socket_;
      }

      if (resolver_to_cancel || socket_to_close) {
        // Use promise/future to wait for the close operation to complete
        auto close_promise = std::make_shared<std::promise<void>>();
        auto close_future = close_promise->get_future();

        asio::post(*io_context_,
                   [resolver_to_cancel, socket_to_close, close_promise]() {
                     if (resolver_to_cancel) {
                       resolver_to_cancel->cancel();
                     }
                     if (socket_to_close) {
                       asio::error_code ec;
                       // Only use close() - do NOT call cancel() on pending_socket_.
                       // When async_connect() is in progress, the socket may be open but
                       // not fully registered with the reactor. Calling cancel() on such
                       // a socket causes SEGV in epoll_reactor::cancel_ops() when accessing
                       // uninitialized op_queue. close() is sufficient as it cancels all
                       // pending async operations and is safe in any socket state.
                       socket_to_close->close(ec);
                     }
                     close_promise->set_value();
                   });

        // Wait with timeout in case io_context is already stopped
        close_future.wait_for(std::chrono::milliseconds(100));
      }
    }

    // Release work guard to allow io_context to finish
    if (work_guard_) {
      work_guard_.reset();
    }

    // Stop io_context BEFORE waiting. This ensures io_context::run() returns
    // promptly without waiting for pending async operations to complete.
    // This is safe because io_context uses intentional leak pattern (no-op deleter),
    // so any handlers that don't run won't cause heap corruption since
    // io_context won't be destroyed.
    if (io_context_) {
      integration::io_context_thread_manager::instance().stop_io_context(
          io_context_);
    }

    // Wait for io_context task to complete - should return immediately after stop()
    if (io_context_future_.valid()) {
      io_context_future_.wait();
    }

    // Clear pending resources after io_context is stopped
    {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      pending_resolver_.reset();
      pending_socket_.reset();
    }
    // Note: io_context uses intentional leak pattern (no-op deleter),
    // so reset() just clears the shared_ptr without destroying io_context
    io_context_.reset();

    return ok();
  } catch (const std::exception &e) {
    return error_void(error_codes::common_errors::internal_error,
                      "Failed to stop client: " + std::string(e.what()),
                      "messaging_client::do_stop",
                      "Client ID: " + client_id());
  }
}

// wait_for_stop() is provided by base class

auto messaging_client::on_connection_failed(std::error_code ec) -> void {
  // NOTE: No logging here to prevent heap corruption during static destruction.
  // When common_system's GlobalLoggerRegistry is destroyed before this callback
  // completes, any logging (even with is_logging_safe() check) causes heap
  // corruption because the check cannot synchronize with common_system's
  // static destruction order.

  // Mark as not connected (but keep is_running_ true so destructor calls stop_client)
  set_connected(false);

  // Invoke error callback using base class method
  invoke_error_callback(ec);

  // Note: Do NOT release work_guard_ or signal stop_promise_ here.
  // The destructor will call stop_client() which handles all cleanup properly.
  // This avoids race conditions between callback cleanup and destructor cleanup.
}

auto messaging_client::do_connect(std::string_view host, unsigned short port)
    -> void {
  // Use resolver to get endpoints
  // Store resolver as member so we can cancel it during stop_client()
  auto resolver = std::make_shared<tcp::resolver>(*io_context_);
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_resolver_ = resolver;
  }
  auto self = shared_from_this();

  // NOTE: No logging in async handlers to prevent heap corruption during
  // static destruction. The is_logging_safe() check cannot synchronize with
  // common_system's static destruction order.

  resolver->async_resolve(
      std::string(host), std::to_string(port),
      [this, self, resolver](std::error_code ec,
                             tcp::resolver::results_type results) {
        // Check if stop was initiated before proceeding with socket operations.
        // This prevents race conditions when do_stop() is called during connection.
        if (is_stop_initiated()) {
          return;
        }

        // Clear pending resolver only if it's still the same resolver
        // (prevents race when rapid connect/disconnect overwrites pending_resolver_)
        {
          std::lock_guard<std::mutex> lock(pending_mutex_);
          if (pending_resolver_.get() == resolver.get()) {
            pending_resolver_.reset();
          }
        }
        if (ec) {
          on_connection_failed(ec);
          return;
        }

        // Check again after clearing resolver
        if (is_stop_initiated()) {
          return;
        }

        // Attempt to connect to one of the resolved endpoints
        // Store socket as member so we can cancel it during stop_client()
        auto raw_socket = std::make_shared<tcp::socket>(*io_context_);
        {
          std::lock_guard<std::mutex> lock(pending_mutex_);
          pending_socket_ = raw_socket;
        }
        asio::async_connect(
            *raw_socket, results,
            [this, self,
             raw_socket](std::error_code connect_ec,
                         [[maybe_unused]] const tcp::endpoint &endpoint) {
              // Check if stop was initiated before proceeding with socket operations.
              // This prevents race conditions when do_stop() is called during connection.
              if (is_stop_initiated()) {
                return;
              }

              // Clear pending socket only if it's still the same socket
              // (prevents race when rapid connect/disconnect overwrites pending_socket_)
              {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                if (pending_socket_.get() == raw_socket.get()) {
                  pending_socket_.reset();
                }
              }
              if (connect_ec) {
                on_connection_failed(connect_ec);
                return;
              }

              // Check again after clearing socket
              if (is_stop_initiated()) {
                return;
              }

              // On success, wrap it in our tcp_socket with mutex protection
              {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                socket_ = std::make_shared<internal::tcp_socket>(
                    std::move(*raw_socket));
              }
              on_connect(connect_ec);
            });
      });
}

auto messaging_client::on_connect(std::error_code ec) -> void {
  // NOTE: No logging in async handlers to prevent heap corruption during
  // static destruction. The is_logging_safe() check cannot synchronize with
  // common_system's static destruction order.
  if (ec) {
    return;
  }
  set_connected(true);

  // Invoke connected callback using base class method
  invoke_connected_callback();

  // set callbacks and start read loop with mutex protection
  // Use span-based callback for zero-copy receive path (no per-read allocation)
  auto self = shared_from_this();
  auto local_socket = get_socket();

  if (local_socket) {
    local_socket->set_receive_callback_view(
        [this, self](std::span<const uint8_t> chunk) { on_receive(chunk); });
    local_socket->set_error_callback(
        [this, self](std::error_code err) { on_error(err); });
    local_socket->start_read();
  }
}

// TCP-specific implementation of data send
auto messaging_client::do_send_impl(std::vector<uint8_t> &&data) -> VoidResult {
  // Get a local copy of socket with mutex protection
  auto local_socket = get_socket();

  if (!local_socket) {
    return error_void(error_codes::network_system::connection_closed,
                      "Socket not available",
                      "messaging_client::do_send",
                      "Client ID: " + client_id());
  }

  // Send data directly without pipeline transformation
  // NOTE: No logging in send callbacks to prevent heap corruption during
  // static destruction.
  local_socket->async_send(std::move(data),
      [](std::error_code, std::size_t) {
        // Silently ignore errors - no logging during potential static destruction
      });

  return ok();
}

auto messaging_client::on_receive(std::span<const uint8_t> data) -> void {
  // NOTE: No logging in async handlers to prevent heap corruption during
  // static destruction.
  if (!is_connected()) {
    return;
  }

  // Invoke receive callback using base class method
  // Copy span to vector only at API boundary to maintain external callback compatibility
  std::vector<uint8_t> data_copy(data.begin(), data.end());
  invoke_receive_callback(data_copy);
}

auto messaging_client::on_error(std::error_code ec) -> void {
  // NOTE: No logging in async handlers to prevent heap corruption during
  // static destruction.

  // Invoke error callback using base class method
  invoke_error_callback(ec);

  // Invoke disconnected callback if was connected
  if (is_connected()) {
    invoke_disconnected_callback();
  }

  // Mark connection as lost using base class method
  // Note: We don't modify is_running_ here - the base class stop_client() handles that
  set_connected(false);
}

auto messaging_client::get_socket() const
    -> std::shared_ptr<internal::tcp_socket> {
  std::lock_guard<std::mutex> lock(socket_mutex_);
  return socket_;
}

} // namespace kcenon::network::core
