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

#include "network_system/internal/send_coroutine.h"
#include "network_system/integration/logger_integration.h"
#include "network_system/integration/thread_integration.h"

#include <asio/associated_executor.hpp>
#include <asio/experimental/as_tuple.hpp>

#include <functional>
#include <future>
#include <system_error>
#include <type_traits>

namespace network_system::internal {
namespace {

std::vector<uint8_t> apply_pipeline(std::vector<uint8_t> data,
                                    const pipeline& pl,
                                    bool use_compress,
                                    bool use_encrypt) {
    if (use_compress && pl.compress) {
        data = pl.compress(data);
    }

    if (use_encrypt && pl.encrypt) {
        data = pl.encrypt(data);
    }

    return data;
}

template<typename CompletionToken>
auto async_prepare_pipeline(std::vector<uint8_t> data,
                            pipeline pl,
                            bool use_compress,
                            bool use_encrypt,
                            CompletionToken&& token) {
    return asio::async_initiate<CompletionToken, void(std::error_code, std::vector<uint8_t>)>(
        [data = std::move(data), pl = std::move(pl), use_compress, use_encrypt](auto&& handler) mutable {
            using Handler = std::decay_t<decltype(handler)>;
            auto executor = asio::get_associated_executor(handler);
            Handler wrapped_handler(std::forward<decltype(handler)>(handler));

            // Use thread pool instead of detached thread
            integration::thread_integration_manager::instance().submit_task(
                [executor,
                 handler = std::move(wrapped_handler),
                 data = std::move(data),
                 pl = std::move(pl),
                 use_compress,
                 use_encrypt]() mutable {
                    std::error_code ec;
                    std::vector<uint8_t> processed;
                    try {
                        processed = apply_pipeline(std::move(data), pl, use_compress, use_encrypt);
                    } catch (...) {
                        ec = std::make_error_code(std::errc::io_error);
                    }

                    asio::post(executor, [handler = std::move(handler), ec, processed = std::move(processed)]() mutable {
                        handler(ec, std::move(processed));
                    });
                });
        },
        std::forward<CompletionToken>(token));
}

} // namespace

auto prepare_data_async(std::vector<uint8_t> input_data,
                        pipeline pl,
                        bool use_compress,
                        bool use_encrypt)
    -> std::future<std::vector<uint8_t>>
{
    return std::async(std::launch::async,
        [data = std::move(input_data), pl = std::move(pl), use_compress, use_encrypt]() mutable {
            return apply_pipeline(std::move(data), pl, use_compress, use_encrypt);
        });
}

#ifdef USE_STD_COROUTINE

auto async_send_with_pipeline_co(std::shared_ptr<tcp_socket> sock,
                                 std::vector<uint8_t> data,
                                 pipeline pl,
                                 bool use_compress,
                                 bool use_encrypt)
    -> asio::awaitable<std::error_code>
{
    auto [prep_ec, processed_data] = co_await async_prepare_pipeline(
        std::move(data), std::move(pl), use_compress, use_encrypt,
        asio::experimental::as_tuple(asio::use_awaitable));

    if (prep_ec) {
        NETWORK_LOG_ERROR("[send_coroutine] Error preparing data: " + prep_ec.message());
        co_return prep_ec;
    }

    auto [ec, bytes_transferred] = co_await asio::async_write(
        sock->socket(),
        asio::buffer(processed_data),
        asio::experimental::as_tuple(asio::use_awaitable));

    if (ec) {
        NETWORK_LOG_ERROR("[send_coroutine] Error sending data: " + ec.message());
    } else {
        (void)bytes_transferred;
    }

    co_return ec;
}

#else // fallback

auto async_send_with_pipeline_no_co(std::shared_ptr<tcp_socket> sock,
                                    std::vector<uint8_t> data,
                                    pipeline pl,
                                    bool use_compress,
                                    bool use_encrypt)
    -> std::future<std::error_code>
{
    auto promise = std::make_shared<std::promise<std::error_code>>();
    auto future_result = promise->get_future();

    // Use thread pool instead of detached thread
    integration::thread_integration_manager::instance().submit_task(
        [sock = std::move(sock), promise, data = std::move(data), pl = std::move(pl),
         use_compress, use_encrypt]() mutable {
            try {
                auto processed_future = prepare_data_async(std::move(data), std::move(pl), use_compress, use_encrypt);
                auto processed_data = processed_future.get();

                sock->async_send(std::move(processed_data),
                    [promise](std::error_code ec, std::size_t /*bytes_transferred*/) {
                        promise->set_value(ec);
                    });
            } catch (const std::exception& e) {
                NETWORK_LOG_ERROR("[send_coroutine] Exception processing data: " + std::string(e.what()));
                promise->set_value(std::make_error_code(std::errc::io_error));
            }
        });

    return future_result;
}

#endif

} // namespace network_system::internal
