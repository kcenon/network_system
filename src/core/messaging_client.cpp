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
#include "kcenon/network/internal/send_coroutine.h"
#include <span>
#include <string_view>
#include <type_traits>

// Use nested namespace definition (C++17)
namespace kcenon::network::core {

using tcp = asio::ip::tcp;

// Use string_view for better efficiency (C++17)
messaging_client::messaging_client(std::string_view client_id)
    : messaging_client_base<messaging_client>(client_id) {
  // Optionally configure pipeline or modes here:
  pipeline_ = internal::make_default_pipeline();
  compress_mode_ = false; // set true if you want to compress
  encrypt_mode_ = false;  // set true if you want to encrypt
}

// Destructor is defaulted in header - base class handles stop_client() call

// TCP-specific implementation of client start
// Called by base class start_client() after common validation
auto messaging_client::do_start(std::string_view host, unsigned short port)
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
// Called by base class stop_client() after common cleanup
auto messaging_client::do_stop() -> VoidResult {
  // NOTE: No logging in do_stop to prevent heap corruption during static
  // destruction. This method may be called from destructor when
  // GlobalLoggerRegistry is already destroyed.

  try {
    // Swap out socket with mutex protection and close outside lock
    std::shared_ptr<internal::tcp_socket> local_socket;
    {
      std::lock_guard<std::mutex> lock(socket_mutex_);
      local_socket = std::move(socket_);
    }
    if (local_socket) {
      // Stop reading first to prevent new async operations
      local_socket->stop_read();
      asio::error_code ec;
      local_socket->socket().close(ec);
    }

    // Cancel pending connection operations FIRST to trigger handler callbacks
    // with operation_aborted error. This ensures handlers run and clean up
    // their captured resources (resolver/socket) properly.
    {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      if (pending_resolver_) {
        pending_resolver_->cancel();
      }
      if (pending_socket_) {
        asio::error_code ec;
        // Only use close() - do NOT call cancel() on pending_socket_.
        // When async_connect() is in progress, the socket may be open but
        // not fully registered with the reactor. Calling cancel() on such
        // a socket causes SEGV in epoll_reactor::cancel_ops() when accessing
        // uninitialized op_queue. close() is sufficient as it cancels all
        // pending async operations and is safe in any socket state.
        pending_socket_->close(ec);
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
// Called by base class send_packet() after common validation
auto messaging_client::do_send(std::vector<uint8_t> &&data) -> VoidResult {
  // Get a local copy of socket with mutex protection
  auto local_socket = get_socket();

  if (!local_socket) {
    return error_void(error_codes::network_system::connection_closed,
                      "Socket not available",
                      "messaging_client::do_send",
                      "Client ID: " + client_id());
  }

  // Using if constexpr for compile-time branching (C++17)
  // NOTE: No logging in send callbacks to prevent heap corruption during
  // static destruction.
  if constexpr (std::is_same_v<decltype(local_socket->socket().get_executor()),
                               asio::io_context::executor_type>) {
#ifdef USE_STD_COROUTINE
    // Coroutine approach
    asio::co_spawn(
        local_socket->socket().get_executor(),
        async_send_with_pipeline_co(local_socket, std::move(data), pipeline_,
                                    compress_mode_, encrypt_mode_),
        [](std::error_code) {
          // Silently ignore errors - no logging during potential static destruction
        });
#else
    // Fallback approach
    auto fut =
        async_send_with_pipeline_no_co(local_socket, std::move(data), pipeline_,
                                       compress_mode_, encrypt_mode_);
    try {
      (void)fut.get();  // Ignore result - no logging during potential static destruction
    } catch (...) {
      // Silently ignore exceptions - no logging during potential static destruction
    }
#endif
  }
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

  // Decompress/Decrypt if needed?
  // For demonstration, ignoring. In real usage:
  //   auto uncompressed = pipeline_.decompress(...);
  //   auto decrypted = pipeline_.decrypt(...);
  //   parse or handle...
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

// Callback setters are provided by base class

} // namespace kcenon::network::core
