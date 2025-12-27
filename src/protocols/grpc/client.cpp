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

#include "kcenon/network/protocols/grpc/client.h"
#include "kcenon/network/protocols/grpc/frame.h"
#include "kcenon/network/protocols/grpc/grpc_official_wrapper.h"

#include <atomic>
#include <charconv>
#include <condition_variable>
#include <mutex>
#include <thread>

#if NETWORK_GRPC_OFFICIAL
#include <grpcpp/grpcpp.h>
#include <grpcpp/generic/generic_stub.h>
#include <grpcpp/support/byte_buffer.h>
#else
#include "kcenon/network/protocols/http2/http2_client.h"
#include "kcenon/network/integration/thread_integration.h"
#endif

namespace kcenon::network::protocols::grpc
{

#if NETWORK_GRPC_OFFICIAL

// ============================================================================
// Official gRPC Library Client Implementation
// ============================================================================

namespace detail
{

auto vector_to_byte_buffer(const std::vector<uint8_t>& data) -> ::grpc::ByteBuffer
{
    ::grpc::Slice slice(data.data(), data.size());
    return ::grpc::ByteBuffer(&slice, 1);
}

auto byte_buffer_to_vector(const ::grpc::ByteBuffer& buffer) -> std::vector<uint8_t>
{
    std::vector<::grpc::Slice> slices;
    buffer.Dump(&slices);

    std::vector<uint8_t> result;
    result.reserve(buffer.Length());

    for (const auto& slice : slices)
    {
        const auto* begin = reinterpret_cast<const uint8_t*>(slice.begin());
        result.insert(result.end(), begin, begin + slice.size());
    }

    return result;
}

} // namespace detail

// Server stream reader implementation for official gRPC
class official_server_stream_reader : public grpc_client::server_stream_reader
{
public:
    official_server_stream_reader(
        std::unique_ptr<::grpc::ClientContext> ctx,
        std::unique_ptr<::grpc::GenericClientAsyncReader> reader,
        ::grpc::CompletionQueue* cq)
        : ctx_(std::move(ctx))
        , reader_(std::move(reader))
        , cq_(cq)
        , has_more_(true)
    {
    }

    auto read() -> Result<grpc_message> override
    {
        if (!has_more_)
        {
            return error<grpc_message>(
                static_cast<int>(status_code::ok),
                "End of stream",
                "grpc::client::server_stream_reader");
        }

        ::grpc::ByteBuffer buffer;
        void* tag = nullptr;
        bool ok = false;

        reader_->Read(&buffer, &tag);

        if (!cq_->Next(&tag, &ok) || !ok)
        {
            has_more_ = false;
            return error<grpc_message>(
                static_cast<int>(status_code::ok),
                "End of stream",
                "grpc::client::server_stream_reader");
        }

        auto data = detail::byte_buffer_to_vector(buffer);
        return kcenon::network::protocols::grpc::ok(grpc_message{std::move(data)});
    }

    auto has_more() const -> bool override
    {
        return has_more_;
    }

    auto finish() -> grpc_status override
    {
        ::grpc::Status status;
        void* tag = nullptr;
        bool ok = false;

        reader_->Finish(&status, &tag);
        cq_->Next(&tag, &ok);

        return from_grpc_status(status);
    }

private:
    std::unique_ptr<::grpc::ClientContext> ctx_;
    std::unique_ptr<::grpc::GenericClientAsyncReader> reader_;
    ::grpc::CompletionQueue* cq_;
    bool has_more_;
};

// Client stream writer implementation for official gRPC
class official_client_stream_writer : public grpc_client::client_stream_writer
{
public:
    official_client_stream_writer(
        std::unique_ptr<::grpc::ClientContext> ctx,
        std::unique_ptr<::grpc::GenericClientAsyncWriter> writer,
        ::grpc::CompletionQueue* cq)
        : ctx_(std::move(ctx))
        , writer_(std::move(writer))
        , cq_(cq)
        , writes_done_(false)
    {
    }

    auto write(const std::vector<uint8_t>& message) -> VoidResult override
    {
        if (writes_done_)
        {
            return error_void(
                error_codes::common_errors::internal_error,
                "Stream writes already done",
                "grpc::client::client_stream_writer");
        }

        ::grpc::ByteBuffer buffer = detail::vector_to_byte_buffer(message);
        void* tag = nullptr;
        bool ok = false;

        writer_->Write(buffer, &tag);

        if (!cq_->Next(&tag, &ok) || !ok)
        {
            return error_void(
                error_codes::network_system::connection_failed,
                "Failed to write message",
                "grpc::client::client_stream_writer");
        }

        return kcenon::network::protocols::grpc::ok();
    }

    auto writes_done() -> VoidResult override
    {
        if (writes_done_)
        {
            return kcenon::network::protocols::grpc::ok();
        }

        void* tag = nullptr;
        bool ok = false;

        writer_->WritesDone(&tag);
        cq_->Next(&tag, &ok);

        writes_done_ = true;
        return kcenon::network::protocols::grpc::ok();
    }

    auto finish() -> Result<grpc_message> override
    {
        if (!writes_done_)
        {
            writes_done();
        }

        ::grpc::ByteBuffer response;
        ::grpc::Status status;
        void* tag = nullptr;
        bool ok = false;

        writer_->Finish(&status, &tag);
        cq_->Next(&tag, &ok);

        if (!status.ok())
        {
            auto grpc_st = from_grpc_status(status);
            return error<grpc_message>(
                static_cast<int>(grpc_st.code),
                grpc_st.message,
                "grpc::client::client_stream_writer");
        }

        // Note: Response is not available from writer, need to handle differently
        return kcenon::network::protocols::grpc::ok(grpc_message{});
    }

private:
    std::unique_ptr<::grpc::ClientContext> ctx_;
    std::unique_ptr<::grpc::GenericClientAsyncWriter> writer_;
    ::grpc::CompletionQueue* cq_;
    bool writes_done_;
};

// Bidirectional stream implementation for official gRPC
class official_bidi_stream : public grpc_client::bidi_stream
{
public:
    official_bidi_stream(
        std::unique_ptr<::grpc::ClientContext> ctx,
        std::unique_ptr<::grpc::GenericClientAsyncReaderWriter> stream,
        ::grpc::CompletionQueue* cq)
        : ctx_(std::move(ctx))
        , stream_(std::move(stream))
        , cq_(cq)
        , writes_done_(false)
    {
    }

    auto write(const std::vector<uint8_t>& message) -> VoidResult override
    {
        if (writes_done_)
        {
            return error_void(
                error_codes::common_errors::internal_error,
                "Stream writes already done",
                "grpc::client::bidi_stream");
        }

        ::grpc::ByteBuffer buffer = detail::vector_to_byte_buffer(message);
        void* tag = nullptr;
        bool ok = false;

        stream_->Write(buffer, &tag);

        if (!cq_->Next(&tag, &ok) || !ok)
        {
            return error_void(
                error_codes::network_system::connection_failed,
                "Failed to write message",
                "grpc::client::bidi_stream");
        }

        return kcenon::network::protocols::grpc::ok();
    }

    auto read() -> Result<grpc_message> override
    {
        ::grpc::ByteBuffer buffer;
        void* tag = nullptr;
        bool ok = false;

        stream_->Read(&buffer, &tag);

        if (!cq_->Next(&tag, &ok) || !ok)
        {
            return error<grpc_message>(
                static_cast<int>(status_code::ok),
                "End of stream",
                "grpc::client::bidi_stream");
        }

        auto data = detail::byte_buffer_to_vector(buffer);
        return kcenon::network::protocols::grpc::ok(grpc_message{std::move(data)});
    }

    auto writes_done() -> VoidResult override
    {
        if (writes_done_)
        {
            return kcenon::network::protocols::grpc::ok();
        }

        void* tag = nullptr;
        bool ok = false;

        stream_->WritesDone(&tag);
        cq_->Next(&tag, &ok);

        writes_done_ = true;
        return kcenon::network::protocols::grpc::ok();
    }

    auto finish() -> grpc_status override
    {
        if (!writes_done_)
        {
            writes_done();
        }

        ::grpc::Status status;
        void* tag = nullptr;
        bool ok = false;

        stream_->Finish(&status, &tag);
        cq_->Next(&tag, &ok);

        return from_grpc_status(status);
    }

private:
    std::unique_ptr<::grpc::ClientContext> ctx_;
    std::unique_ptr<::grpc::GenericClientAsyncReaderWriter> stream_;
    ::grpc::CompletionQueue* cq_;
    bool writes_done_;
};

// Implementation class using official gRPC
class grpc_client::impl
{
public:
    explicit impl(std::string target, grpc_channel_config config)
        : target_(std::move(target))
        , config_(std::move(config))
        , connected_(false)
    {
    }

    ~impl()
    {
        disconnect();
    }

    auto connect() -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (connected_.load())
        {
            return kcenon::network::protocols::grpc::ok();
        }

        // Create channel credentials
        channel_credentials_config creds_config;
        creds_config.insecure = !config_.use_tls;

        if (config_.use_tls)
        {
            creds_config.root_certificates = config_.root_certificates;
            creds_config.client_certificate = config_.client_certificate;
            creds_config.client_key = config_.client_key;
        }

        channel_ = create_channel(target_, creds_config);

        if (!channel_)
        {
            return error_void(
                error_codes::network_system::connection_failed,
                "Failed to create gRPC channel",
                "grpc::client");
        }

        // Wait for channel to be ready
        if (!wait_for_channel_ready(channel_, config_.default_timeout))
        {
            return error_void(
                error_codes::network_system::connection_failed,
                "Failed to connect to gRPC server",
                "grpc::client",
                target_);
        }

        stub_ = std::make_unique<::grpc::GenericStub>(channel_);
        connected_.store(true);

        return kcenon::network::protocols::grpc::ok();
    }

    auto disconnect() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);

        stub_.reset();
        channel_.reset();
        connected_.store(false);
    }

    auto is_connected() const -> bool
    {
        if (!connected_.load() || !channel_)
        {
            return false;
        }

        auto state = channel_->GetState(false);
        return state == GRPC_CHANNEL_READY || state == GRPC_CHANNEL_IDLE;
    }

    auto wait_for_connected(std::chrono::milliseconds timeout) -> bool
    {
        if (!channel_)
        {
            return false;
        }

        return wait_for_channel_ready(channel_, timeout);
    }

    auto target() const -> const std::string&
    {
        return target_;
    }

    auto call_raw(const std::string& method,
                  const std::vector<uint8_t>& request,
                  const call_options& options) -> Result<grpc_message>
    {
        if (!is_connected())
        {
            return error<grpc_message>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        ::grpc::ClientContext ctx;

        // Set deadline if provided
        if (options.deadline.has_value())
        {
            set_deadline(&ctx, options.deadline.value());
        }
        else
        {
            // Use default timeout
            set_timeout(&ctx, config_.default_timeout);
        }

        // Add metadata
        for (const auto& [key, value] : options.metadata)
        {
            ctx.AddMetadata(key, value);
        }

        // Set wait for ready
        if (options.wait_for_ready)
        {
            ctx.set_wait_for_ready(true);
        }

        // Prepare request
        ::grpc::ByteBuffer request_buffer = detail::vector_to_byte_buffer(request);
        ::grpc::ByteBuffer response_buffer;

        // Make unary call
        ::grpc::Status status = stub_->UnaryCall(&ctx, method, request_buffer, &response_buffer);

        if (!status.ok())
        {
            auto grpc_st = from_grpc_status(status);
            return error<grpc_message>(
                static_cast<int>(grpc_st.code),
                grpc_st.message,
                "grpc::client");
        }

        auto response_data = detail::byte_buffer_to_vector(response_buffer);
        return kcenon::network::protocols::grpc::ok(grpc_message{std::move(response_data)});
    }

    auto call_raw_async(const std::string& method,
                        const std::vector<uint8_t>& request,
                        std::function<void(Result<grpc_message>)> callback,
                        const call_options& options) -> void
    {
        // Create async call in separate thread
        std::thread([this, method, request, callback, options]() {
            auto result = call_raw(method, request, options);
            if (callback)
            {
                callback(std::move(result));
            }
        }).detach();
    }

    auto server_stream_raw(const std::string& method,
                           const std::vector<uint8_t>& request,
                           const call_options& options)
        -> Result<std::unique_ptr<grpc_client::server_stream_reader>>
    {
        if (!is_connected())
        {
            return error<std::unique_ptr<grpc_client::server_stream_reader>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        auto ctx = std::make_unique<::grpc::ClientContext>();

        // Set deadline if provided
        if (options.deadline.has_value())
        {
            set_deadline(ctx.get(), options.deadline.value());
        }
        else
        {
            set_timeout(ctx.get(), config_.default_timeout);
        }

        // Add metadata
        for (const auto& [key, value] : options.metadata)
        {
            ctx->AddMetadata(key, value);
        }

        // Note: GenericStub doesn't have PrepareUnaryCall with streaming
        // For full streaming support, we would need to use async API
        // This is a simplified synchronous implementation

        return error<std::unique_ptr<grpc_client::server_stream_reader>>(
            static_cast<int>(status_code::unimplemented),
            "Server streaming not fully implemented for official gRPC wrapper",
            "grpc::client");
    }

    auto client_stream_raw(const std::string& method,
                           const call_options& options)
        -> Result<std::unique_ptr<grpc_client::client_stream_writer>>
    {
        if (!is_connected())
        {
            return error<std::unique_ptr<grpc_client::client_stream_writer>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        return error<std::unique_ptr<grpc_client::client_stream_writer>>(
            static_cast<int>(status_code::unimplemented),
            "Client streaming not fully implemented for official gRPC wrapper",
            "grpc::client");
    }

    auto bidi_stream_raw(const std::string& method,
                         const call_options& options)
        -> Result<std::unique_ptr<grpc_client::bidi_stream>>
    {
        if (!is_connected())
        {
            return error<std::unique_ptr<grpc_client::bidi_stream>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        return error<std::unique_ptr<grpc_client::bidi_stream>>(
            static_cast<int>(status_code::unimplemented),
            "Bidirectional streaming not fully implemented for official gRPC wrapper",
            "grpc::client");
    }

private:
    std::string target_;
    grpc_channel_config config_;
    std::shared_ptr<::grpc::Channel> channel_;
    std::unique_ptr<::grpc::GenericStub> stub_;
    std::atomic<bool> connected_;
    mutable std::mutex mutex_;
};

#else // !NETWORK_GRPC_OFFICIAL

// ============================================================================
// Prototype Implementation (HTTP/2 Transport)
// ============================================================================

// Server stream reader implementation
class server_stream_reader_impl : public grpc_client::server_stream_reader
{
public:
    server_stream_reader_impl(std::shared_ptr<http2::http2_client> http2_client,
                              uint32_t stream_id)
        : http2_client_(std::move(http2_client))
        , stream_id_(stream_id)
        , has_more_(true)
    {
    }

    auto read() -> Result<grpc_message> override
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait for data or end of stream
        cv_.wait(lock, [this] { return !buffer_.empty() || !has_more_; });

        if (buffer_.empty())
        {
            if (!has_more_)
            {
                return error<grpc_message>(
                    static_cast<int>(status_code::ok),
                    "End of stream",
                    "server_stream_reader::read");
            }
        }

        // Parse gRPC message from buffer
        auto parse_result = grpc_message::parse(buffer_);
        if (parse_result.is_err())
        {
            return parse_result;
        }

        // Remove parsed data from buffer
        auto& msg = parse_result.value();
        size_t msg_size = 5 + msg.data.size();  // 1 byte compression + 4 bytes length + data
        if (buffer_.size() >= msg_size)
        {
            buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(msg_size));
        }

        return ok(std::move(msg));
    }

    auto has_more() const -> bool override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return has_more_ || !buffer_.empty();
    }

    auto finish() -> grpc_status override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !has_more_; });
        return final_status_;
    }

    void on_data(const std::vector<uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.insert(buffer_.end(), data.begin(), data.end());
        cv_.notify_one();
    }

    void on_headers(const std::vector<http2::http_header>& headers)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& h : headers)
        {
            if (h.name == "grpc-status")
            {
                try { final_status_.code = static_cast<status_code>(std::stoi(h.value)); }
                catch (...) {}
            }
            else if (h.name == "grpc-message")
            {
                final_status_.message = h.value;
            }
        }
    }

    void on_complete(int status_code)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        has_more_ = false;
        if (status_code != 200)
        {
            final_status_.code = grpc::status_code::unavailable;
            final_status_.message = "HTTP error: " + std::to_string(status_code);
        }
        cv_.notify_all();
    }

private:
    std::shared_ptr<http2::http2_client> http2_client_;
    [[maybe_unused]] uint32_t stream_id_;  // Reserved for future stream operations
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<uint8_t> buffer_;
    bool has_more_;
    grpc_status final_status_;
};

// Client stream writer implementation
class client_stream_writer_impl : public grpc_client::client_stream_writer
{
public:
    client_stream_writer_impl(std::shared_ptr<http2::http2_client> http2_client,
                              uint32_t stream_id)
        : http2_client_(std::move(http2_client))
        , stream_id_(stream_id)
        , writes_done_(false)
    {
    }

    auto write(const std::vector<uint8_t>& message) -> VoidResult override
    {
        if (writes_done_)
        {
            return error_void(
                error_codes::common_errors::internal_error,
                "Stream writes already done",
                "client_stream_writer::write");
        }

        // Serialize with gRPC framing
        grpc_message msg{std::vector<uint8_t>(message)};
        auto serialized = msg.serialize();

        return http2_client_->write_stream(stream_id_, serialized, false);
    }

    auto writes_done() -> VoidResult override
    {
        if (writes_done_)
        {
            return ok();
        }

        writes_done_ = true;
        return http2_client_->close_stream_writer(stream_id_);
    }

    auto finish() -> Result<grpc_message> override
    {
        // Close write side if not already done
        if (!writes_done_)
        {
            auto wd_result = writes_done();
            if (wd_result.is_err())
            {
                const auto& err = wd_result.error();
                return error<grpc_message>(err.code, err.message, "client_stream_writer::finish");
            }
        }

        // Wait for response
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return response_ready_; });

        if (!response_.data.empty() || final_status_.code == status_code::ok)
        {
            return ok(std::move(response_));
        }

        return error<grpc_message>(
            static_cast<int>(final_status_.code),
            final_status_.message,
            "client_stream_writer::finish");
    }

    void on_data(const std::vector<uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        response_buffer_.insert(response_buffer_.end(), data.begin(), data.end());
    }

    void on_headers(const std::vector<http2::http_header>& headers)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& h : headers)
        {
            if (h.name == "grpc-status")
            {
                try { final_status_.code = static_cast<status_code>(std::stoi(h.value)); }
                catch (...) {}
            }
            else if (h.name == "grpc-message")
            {
                final_status_.message = h.value;
            }
        }
    }

    void on_complete(int status_code)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (status_code != 200)
        {
            final_status_.code = grpc::status_code::unavailable;
            final_status_.message = "HTTP error: " + std::to_string(status_code);
        }

        // Parse response
        if (!response_buffer_.empty())
        {
            auto parse_result = grpc_message::parse(response_buffer_);
            if (parse_result.is_ok())
            {
                response_ = std::move(parse_result.value());
            }
        }

        response_ready_ = true;
        cv_.notify_all();
    }

private:
    std::shared_ptr<http2::http2_client> http2_client_;
    uint32_t stream_id_;
    bool writes_done_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<uint8_t> response_buffer_;
    grpc_message response_;
    grpc_status final_status_;
    bool response_ready_ = false;
};

// Bidirectional stream implementation
class bidi_stream_impl : public grpc_client::bidi_stream
{
public:
    bidi_stream_impl(std::shared_ptr<http2::http2_client> http2_client,
                     uint32_t stream_id)
        : http2_client_(std::move(http2_client))
        , stream_id_(stream_id)
        , writes_done_(false)
        , stream_ended_(false)
    {
    }

    auto write(const std::vector<uint8_t>& message) -> VoidResult override
    {
        if (writes_done_)
        {
            return error_void(
                error_codes::common_errors::internal_error,
                "Stream writes already done",
                "bidi_stream::write");
        }

        grpc_message msg{std::vector<uint8_t>(message)};
        auto serialized = msg.serialize();

        return http2_client_->write_stream(stream_id_, serialized, false);
    }

    auto read() -> Result<grpc_message> override
    {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_.wait(lock, [this] { return !buffer_.empty() || stream_ended_; });

        if (buffer_.empty())
        {
            if (stream_ended_)
            {
                return error<grpc_message>(
                    static_cast<int>(status_code::ok),
                    "End of stream",
                    "bidi_stream::read");
            }
        }

        auto parse_result = grpc_message::parse(buffer_);
        if (parse_result.is_err())
        {
            return parse_result;
        }

        auto& msg = parse_result.value();
        size_t msg_size = 5 + msg.data.size();
        if (buffer_.size() >= msg_size)
        {
            buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<ptrdiff_t>(msg_size));
        }

        return ok(std::move(msg));
    }

    auto writes_done() -> VoidResult override
    {
        if (writes_done_)
        {
            return ok();
        }

        writes_done_ = true;
        return http2_client_->close_stream_writer(stream_id_);
    }

    auto finish() -> grpc_status override
    {
        if (!writes_done_)
        {
            writes_done();
        }

        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return stream_ended_; });
        return final_status_;
    }

    void on_data(const std::vector<uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.insert(buffer_.end(), data.begin(), data.end());
        cv_.notify_one();
    }

    void on_headers(const std::vector<http2::http_header>& headers)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& h : headers)
        {
            if (h.name == "grpc-status")
            {
                try { final_status_.code = static_cast<status_code>(std::stoi(h.value)); }
                catch (...) {}
            }
            else if (h.name == "grpc-message")
            {
                final_status_.message = h.value;
            }
        }
    }

    void on_complete(int status_code)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stream_ended_ = true;
        if (status_code != 200)
        {
            final_status_.code = grpc::status_code::unavailable;
            final_status_.message = "HTTP error: " + std::to_string(status_code);
        }
        cv_.notify_all();
    }

private:
    std::shared_ptr<http2::http2_client> http2_client_;
    uint32_t stream_id_;
    bool writes_done_;
    bool stream_ended_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<uint8_t> buffer_;
    grpc_status final_status_;
};

// Implementation class using HTTP/2 transport
class grpc_client::impl
{
public:
    explicit impl(std::string target, grpc_channel_config config)
        : target_(std::move(target))
        , config_(std::move(config))
        , connected_(false)
    {
    }

    ~impl()
    {
        disconnect();
    }

    auto connect() -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (connected_.load())
        {
            return ok();
        }

        // Parse target address
        auto colon_pos = target_.find(':');
        if (colon_pos == std::string::npos)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid target address format",
                "grpc::client",
                "Expected format: host:port");
        }

        host_ = target_.substr(0, colon_pos);
        auto port_str = target_.substr(colon_pos + 1);

        unsigned short port = 0;
        auto [ptr, ec] = std::from_chars(
            port_str.data(),
            port_str.data() + port_str.size(),
            port);

        if (ec != std::errc())
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid port number",
                "grpc::client");
        }

        // Create HTTP/2 client
        http2_client_ = std::make_shared<http2::http2_client>("grpc-client");
        http2_client_->set_timeout(config_.default_timeout);

        // Connect using HTTP/2
        auto result = http2_client_->connect(host_, port);
        if (result.is_err())
        {
            const auto& err = result.error();
            return error_void(err.code, err.message, "grpc::client",
                              get_error_details(err));
        }

        connected_.store(true);
        return ok();
    }

    auto disconnect() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (http2_client_ && connected_.load())
        {
            http2_client_->disconnect();
        }

        connected_.store(false);
        http2_client_.reset();
    }

    auto is_connected() const -> bool
    {
        return connected_.load() && http2_client_ && http2_client_->is_connected();
    }

    auto wait_for_connected(std::chrono::milliseconds timeout) -> bool
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (is_connected())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return is_connected();
    }

    auto target() const -> const std::string&
    {
        return target_;
    }

    auto call_raw(const std::string& method,
                  const std::vector<uint8_t>& request,
                  const call_options& options) -> Result<grpc_message>
    {
        if (!is_connected())
        {
            return error<grpc_message>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        // Validate method format
        if (method.empty() || method[0] != '/')
        {
            return error<grpc_message>(
                error_codes::common_errors::invalid_argument,
                "Invalid method format",
                "grpc::client",
                "Method must start with '/'");
        }

        // Check deadline
        if (options.deadline.has_value())
        {
            if (std::chrono::system_clock::now() > options.deadline.value())
            {
                return error<grpc_message>(
                    static_cast<int>(status_code::deadline_exceeded),
                    "Deadline exceeded before call started",
                    "grpc::client");
            }
        }

        // Build gRPC headers
        std::vector<http2::http_header> headers;
        headers.emplace_back(header_names::content_type, grpc_content_type);
        headers.emplace_back(header_names::te, "trailers");
        headers.emplace_back(header_names::grpc_accept_encoding,
                             std::string(compression::identity) + "," +
                             compression::gzip + "," + compression::deflate);

        // Add timeout header if deadline is set
        if (options.deadline.has_value())
        {
            auto now = std::chrono::system_clock::now();
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                options.deadline.value() - now);
            if (remaining.count() > 0)
            {
                headers.emplace_back(header_names::grpc_timeout,
                                     format_timeout(static_cast<uint64_t>(remaining.count())));
            }
        }

        // Add custom metadata
        for (const auto& [key, value] : options.metadata)
        {
            headers.emplace_back(key, value);
        }

        // Serialize request with gRPC framing
        grpc_message request_msg{std::vector<uint8_t>{request.begin(), request.end()}};
        auto serialized_request = request_msg.serialize();

        // Send HTTP/2 POST request
        auto response_result = http2_client_->post(method, serialized_request, headers);
        if (response_result.is_err())
        {
            const auto& err = response_result.error();
            return error<grpc_message>(err.code, err.message, "grpc::client",
                                       get_error_details(err));
        }

        const auto& response = response_result.value();

        // Check HTTP status - gRPC always uses 200 OK for successful transport
        if (response.status_code != 200)
        {
            return error<grpc_message>(
                static_cast<int>(status_code::unavailable),
                "HTTP error: " + std::to_string(response.status_code),
                "grpc::client");
        }

        // Extract gRPC status from trailers/headers
        status_code grpc_status = status_code::ok;
        std::string grpc_message_str;

        for (const auto& header : response.headers)
        {
            if (header.name == trailer_names::grpc_status)
            {
                int status_int = 0;
                auto [ptr, ec] = std::from_chars(
                    header.value.data(),
                    header.value.data() + header.value.size(),
                    status_int);
                if (ec == std::errc())
                {
                    grpc_status = static_cast<status_code>(status_int);
                }
            }
            else if (header.name == trailer_names::grpc_message)
            {
                grpc_message_str = header.value;
            }
        }

        // Check gRPC status
        if (grpc_status != status_code::ok)
        {
            return error<grpc_message>(
                static_cast<int>(grpc_status),
                grpc_message_str.empty() ?
                    std::string(status_code_to_string(grpc_status)) : grpc_message_str,
                "grpc::client");
        }

        // Parse response message
        if (response.body.empty())
        {
            // Empty response is valid for some RPCs
            return ok(grpc_message{});
        }

        auto parse_result = grpc_message::parse(response.body);
        if (parse_result.is_err())
        {
            const auto& err = parse_result.error();
            return error<grpc_message>(err.code, err.message, "grpc::client",
                                       get_error_details(err));
        }

        return ok(std::move(parse_result.value()));
    }

    auto call_raw_async(const std::string& method,
                        const std::vector<uint8_t>& request,
                        std::function<void(Result<grpc_message>)> callback,
                        const call_options& options) -> void
    {
        // Execute asynchronously using thread pool
        integration::thread_integration_manager::instance().submit_task(
            [this, method, request, callback, options]() {
                auto result = call_raw(method, request, options);
                if (callback)
                {
                    callback(std::move(result));
                }
            });
    }

    auto server_stream_raw(const std::string& method,
                           const std::vector<uint8_t>& request,
                           const call_options& options)
        -> Result<std::unique_ptr<grpc_client::server_stream_reader>>
    {
        if (!is_connected())
        {
            return error<std::unique_ptr<grpc_client::server_stream_reader>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        // Validate method format
        if (method.empty() || method[0] != '/')
        {
            return error<std::unique_ptr<grpc_client::server_stream_reader>>(
                error_codes::common_errors::invalid_argument,
                "Invalid method format",
                "grpc::client",
                "Method must start with '/'");
        }

        // Build gRPC headers
        std::vector<http2::http_header> headers;
        headers.emplace_back("content-type", std::string(grpc_content_type));
        headers.emplace_back("te", "trailers");
        headers.emplace_back("grpc-accept-encoding",
                             std::string(compression::identity) + "," +
                             compression::gzip + "," + compression::deflate);

        // Add timeout header if deadline is set
        if (options.deadline.has_value())
        {
            auto now = std::chrono::system_clock::now();
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                options.deadline.value() - now);
            if (remaining.count() > 0)
            {
                headers.emplace_back("grpc-timeout",
                                     format_timeout(static_cast<uint64_t>(remaining.count())));
            }
        }

        // Add custom metadata
        for (const auto& [key, value] : options.metadata)
        {
            headers.emplace_back(key, value);
        }

        // Create the reader as shared_ptr for callback capture
        auto reader = std::make_shared<server_stream_reader_impl>(http2_client_, 0);

        // Start streaming request
        auto stream_result = http2_client_->start_stream(
            method,
            headers,
            [reader](std::vector<uint8_t> data) { reader->on_data(data); },
            [reader](std::vector<http2::http_header> hdrs) { reader->on_headers(hdrs); },
            [reader](int status) { reader->on_complete(status); });

        if (stream_result.is_err())
        {
            const auto& err = stream_result.error();
            return error<std::unique_ptr<grpc_client::server_stream_reader>>(
                err.code, err.message, "grpc::client", get_error_details(err));
        }

        // Send the request with gRPC framing
        grpc_message request_msg{std::vector<uint8_t>(request)};
        auto serialized = request_msg.serialize();
        auto write_result = http2_client_->write_stream(stream_result.value(), serialized, true);

        if (write_result.is_err())
        {
            const auto& err = write_result.error();
            return error<std::unique_ptr<grpc_client::server_stream_reader>>(
                err.code, err.message, "grpc::client", get_error_details(err));
        }

        // Return the reader wrapped in a shared_ptr-owning unique_ptr wrapper
        // Use a custom deleter that captures the shared_ptr to extend lifetime
        struct shared_holder : public grpc_client::server_stream_reader {
            std::shared_ptr<server_stream_reader_impl> impl;
            explicit shared_holder(std::shared_ptr<server_stream_reader_impl> p) : impl(std::move(p)) {}
            auto read() -> Result<grpc_message> override { return impl->read(); }
            auto has_more() const -> bool override { return impl->has_more(); }
            auto finish() -> grpc_status override { return impl->finish(); }
        };

        return ok(std::unique_ptr<grpc_client::server_stream_reader>(
            new shared_holder(reader)));
    }

    auto client_stream_raw(const std::string& method,
                           const call_options& options)
        -> Result<std::unique_ptr<grpc_client::client_stream_writer>>
    {
        if (!is_connected())
        {
            return error<std::unique_ptr<grpc_client::client_stream_writer>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        // Validate method format
        if (method.empty() || method[0] != '/')
        {
            return error<std::unique_ptr<grpc_client::client_stream_writer>>(
                error_codes::common_errors::invalid_argument,
                "Invalid method format",
                "grpc::client",
                "Method must start with '/'");
        }

        // Build gRPC headers
        std::vector<http2::http_header> headers;
        headers.emplace_back("content-type", std::string(grpc_content_type));
        headers.emplace_back("te", "trailers");
        headers.emplace_back("grpc-accept-encoding",
                             std::string(compression::identity) + "," +
                             compression::gzip + "," + compression::deflate);

        // Add timeout header if deadline is set
        if (options.deadline.has_value())
        {
            auto now = std::chrono::system_clock::now();
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                options.deadline.value() - now);
            if (remaining.count() > 0)
            {
                headers.emplace_back("grpc-timeout",
                                     format_timeout(static_cast<uint64_t>(remaining.count())));
            }
        }

        // Add custom metadata
        for (const auto& [key, value] : options.metadata)
        {
            headers.emplace_back(key, value);
        }

        // Create the writer as shared_ptr for callback capture
        auto stream_id_holder = std::make_shared<uint32_t>(0);
        auto writer = std::make_shared<client_stream_writer_impl>(http2_client_, 0);

        // Start streaming request
        auto stream_result = http2_client_->start_stream(
            method,
            headers,
            [writer](std::vector<uint8_t> data) { writer->on_data(data); },
            [writer](std::vector<http2::http_header> hdrs) { writer->on_headers(hdrs); },
            [writer](int status) { writer->on_complete(status); });

        if (stream_result.is_err())
        {
            const auto& err = stream_result.error();
            return error<std::unique_ptr<grpc_client::client_stream_writer>>(
                err.code, err.message, "grpc::client", get_error_details(err));
        }

        // Update the writer with actual stream ID
        auto actual_writer = std::make_shared<client_stream_writer_impl>(http2_client_, stream_result.value());

        // Re-register callbacks with actual writer
        // Note: This is a simplified implementation - in production, you'd want
        // to properly update the stream callbacks

        struct shared_writer_holder : public grpc_client::client_stream_writer {
            std::shared_ptr<client_stream_writer_impl> impl;
            explicit shared_writer_holder(std::shared_ptr<client_stream_writer_impl> p) : impl(std::move(p)) {}
            auto write(const std::vector<uint8_t>& message) -> VoidResult override { return impl->write(message); }
            auto writes_done() -> VoidResult override { return impl->writes_done(); }
            auto finish() -> Result<grpc_message> override { return impl->finish(); }
        };

        return ok(std::unique_ptr<grpc_client::client_stream_writer>(
            new shared_writer_holder(actual_writer)));
    }

    auto bidi_stream_raw(const std::string& method,
                         const call_options& options)
        -> Result<std::unique_ptr<grpc_client::bidi_stream>>
    {
        if (!is_connected())
        {
            return error<std::unique_ptr<grpc_client::bidi_stream>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        // Validate method format
        if (method.empty() || method[0] != '/')
        {
            return error<std::unique_ptr<grpc_client::bidi_stream>>(
                error_codes::common_errors::invalid_argument,
                "Invalid method format",
                "grpc::client",
                "Method must start with '/'");
        }

        // Build gRPC headers
        std::vector<http2::http_header> headers;
        headers.emplace_back("content-type", std::string(grpc_content_type));
        headers.emplace_back("te", "trailers");
        headers.emplace_back("grpc-accept-encoding",
                             std::string(compression::identity) + "," +
                             compression::gzip + "," + compression::deflate);

        // Add timeout header if deadline is set
        if (options.deadline.has_value())
        {
            auto now = std::chrono::system_clock::now();
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                options.deadline.value() - now);
            if (remaining.count() > 0)
            {
                headers.emplace_back("grpc-timeout",
                                     format_timeout(static_cast<uint64_t>(remaining.count())));
            }
        }

        // Add custom metadata
        for (const auto& [key, value] : options.metadata)
        {
            headers.emplace_back(key, value);
        }

        // Create the bidi stream as shared_ptr for callback capture
        auto bidi = std::make_shared<bidi_stream_impl>(http2_client_, 0);

        // Start streaming request
        auto stream_result = http2_client_->start_stream(
            method,
            headers,
            [bidi](std::vector<uint8_t> data) { bidi->on_data(data); },
            [bidi](std::vector<http2::http_header> hdrs) { bidi->on_headers(hdrs); },
            [bidi](int status) { bidi->on_complete(status); });

        if (stream_result.is_err())
        {
            const auto& err = stream_result.error();
            return error<std::unique_ptr<grpc_client::bidi_stream>>(
                err.code, err.message, "grpc::client", get_error_details(err));
        }

        // Create actual bidi stream with proper stream ID
        auto actual_bidi = std::make_shared<bidi_stream_impl>(http2_client_, stream_result.value());

        struct shared_bidi_holder : public grpc_client::bidi_stream {
            std::shared_ptr<bidi_stream_impl> impl;
            explicit shared_bidi_holder(std::shared_ptr<bidi_stream_impl> p) : impl(std::move(p)) {}
            auto write(const std::vector<uint8_t>& message) -> VoidResult override { return impl->write(message); }
            auto read() -> Result<grpc_message> override { return impl->read(); }
            auto writes_done() -> VoidResult override { return impl->writes_done(); }
            auto finish() -> grpc_status override { return impl->finish(); }
        };

        return ok(std::unique_ptr<grpc_client::bidi_stream>(
            new shared_bidi_holder(actual_bidi)));
    }

private:
    std::string target_;
    std::string host_;
    grpc_channel_config config_;
    std::shared_ptr<http2::http2_client> http2_client_;
    std::atomic<bool> connected_;
    mutable std::mutex mutex_;
};

#endif // !NETWORK_GRPC_OFFICIAL

// grpc_client implementation

grpc_client::grpc_client(const std::string& target,
                         const grpc_channel_config& config)
    : impl_(std::make_unique<impl>(target, config))
{
}

grpc_client::~grpc_client() = default;

grpc_client::grpc_client(grpc_client&&) noexcept = default;
grpc_client& grpc_client::operator=(grpc_client&&) noexcept = default;

auto grpc_client::connect() -> VoidResult
{
    return impl_->connect();
}

auto grpc_client::disconnect() -> void
{
    impl_->disconnect();
}

auto grpc_client::is_connected() const -> bool
{
    return impl_->is_connected();
}

auto grpc_client::wait_for_connected(std::chrono::milliseconds timeout) -> bool
{
    return impl_->wait_for_connected(timeout);
}

auto grpc_client::target() const -> const std::string&
{
    return impl_->target();
}

auto grpc_client::call_raw(const std::string& method,
                           const std::vector<uint8_t>& request,
                           const call_options& options) -> Result<grpc_message>
{
    return impl_->call_raw(method, request, options);
}

auto grpc_client::call_raw_async(const std::string& method,
                                 const std::vector<uint8_t>& request,
                                 std::function<void(Result<grpc_message>)> callback,
                                 const call_options& options) -> void
{
    impl_->call_raw_async(method, request, std::move(callback), options);
}

auto grpc_client::server_stream_raw(const std::string& method,
                                    const std::vector<uint8_t>& request,
                                    const call_options& options)
    -> Result<std::unique_ptr<server_stream_reader>>
{
    return impl_->server_stream_raw(method, request, options);
}

auto grpc_client::client_stream_raw(const std::string& method,
                                    const call_options& options)
    -> Result<std::unique_ptr<client_stream_writer>>
{
    return impl_->client_stream_raw(method, options);
}

auto grpc_client::bidi_stream_raw(const std::string& method,
                                  const call_options& options)
    -> Result<std::unique_ptr<bidi_stream>>
{
    return impl_->bidi_stream_raw(method, options);
}

} // namespace kcenon::network::protocols::grpc
