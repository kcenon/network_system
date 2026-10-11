// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file mock_h2_server_peer.cpp
 * @brief Implementation of server-side HTTP/2 framing peer
 *        (Phase 2A + Phase 2A.2 of Issue #1074)
 */

#include "mock_h2_server_peer.h"

#include "internal/protocols/http2/frame.h"

#include <asio/buffer.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

namespace kcenon::network::tests::support
{

namespace
{

namespace http2 = kcenon::network::protocols::http2;

// Polling bounds stop latency without touching a shared SSL stream from the
// destructor thread. Each complete read/write also has a fixed deadline.
constexpr auto kStopPoll = std::chrono::milliseconds(2);
#ifdef NETWORK_COVERAGE_TIMEOUT_MULTIPLIER
constexpr auto kIoTimeout = std::chrono::seconds(10) * NETWORK_COVERAGE_TIMEOUT_MULTIPLIER;
#else
constexpr auto kIoTimeout = std::chrono::seconds(10);
#endif

class stoppable_tls_io
{
public:
    stoppable_tls_io(asio::io_context& io,
                     asio::ssl::stream<asio::ip::tcp::socket>& stream,
                     const std::atomic<bool>& stop)
        : io_(io), stream_(stream), stop_(stop) {}

    void read(asio::mutable_buffer buffer, std::error_code& ec)
    {
        perform([&](auto done) { asio::async_read(stream_, buffer, std::move(done)); }, ec);
    }

    void write(asio::const_buffer buffer, std::error_code& ec)
    {
        perform([&](auto done) { asio::async_write(stream_, buffer, std::move(done)); }, ec);
    }

    void write_injected(const frame_injector& injector,
                        std::span<const std::uint8_t> bytes, std::error_code& ec)
    {
        ec.clear();
        const auto wire = injector.transform(bytes);
        if (!wire) return; // Deliberately dropped frame.
        if (injector.mode() != injection_mode::slow_write)
        {
            write(asio::buffer(*wire), ec);
            return;
        }
        for (std::size_t i = 0; i < wire->size(); ++i)
        {
            write(asio::buffer(wire->data() + i, 1), ec);
            if (ec) return;
            if (i + 1 == wire->size()) break;
            const auto until = std::chrono::steady_clock::now() + injector.spec().slow_step;
            while (std::chrono::steady_clock::now() < until)
            {
                if (stop_.load()) { ec = asio::error::operation_aborted; return; }
                std::this_thread::sleep_for(std::min(
                    std::chrono::steady_clock::duration(kStopPoll),
                    until - std::chrono::steady_clock::now()));
            }
        }
    }

private:
    template <typename Start>
    void perform(Start start, std::error_code& ec)
    {
        ec.clear();
        if (stop_.load()) { ec = asio::error::operation_aborted; return; }
        bool done = false;
        start([&](std::error_code result, std::size_t) { ec = result; done = true; });
        const auto deadline = std::chrono::steady_clock::now() + kIoTimeout;
        std::error_code cancelled;
        while (!done)
        {
            if (!cancelled && (stop_.load() || std::chrono::steady_clock::now() >= deadline))
            {
                cancelled = stop_.load() ? asio::error::operation_aborted : asio::error::timed_out;
                std::error_code ignored;
                // All SSL operations, socket closure, and completions execute
                // on this worker. Never destroy buffer/handler state early.
                stream_.lowest_layer().close(ignored);
            }
            if (io_.stopped()) io_.restart();
            io_.run_one_for(kStopPoll);
        }
        if (cancelled) ec = cancelled;
    }

    asio::io_context& io_;
    asio::ssl::stream<asio::ip::tcp::socket>& stream_;
    const std::atomic<bool>& stop_;
};

constexpr std::size_t kPrefaceSize = 24;
constexpr std::size_t kFrameHeaderSize = 9;

// HTTP/2 connection preface bytes (RFC 7540 Section 3.5):
// "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
constexpr std::uint8_t kPrefaceBytes[kPrefaceSize] = {
    'P',  'R',  'I',  ' ',  '*',  ' ',  'H',  'T',
    'T',  'P',  '/',  '2',  '.',  '0',  '\r', '\n',
    '\r', '\n', 'S',  'M',  '\r', '\n', '\r', '\n'
};

} // namespace

mock_h2_server_peer::mock_h2_server_peer(
    asio::io_context& io, reply_mode mode, injection_spec inject,
    std::vector<std::vector<std::uint8_t>> post_handshake_frames)
    : listener_(worker_io_, /*trusted=*/true),
      mode_(mode),
      injector_(inject),
      post_handshake_frames_(std::move(post_handshake_frames))
{
    (void)io; // Source-compatible constructor; no dependency on the caller executor.
    worker_ = std::thread([this]() { this->run(); });
}

mock_h2_server_peer::~mock_h2_server_peer()
{
    stop_.store(true);
    // The worker checks stop every kStopPoll, cancels its own asynchronous
    // operation, and drains completion handlers before releasing their state.
    if (worker_.joinable())
    {
        worker_.join();
    }
}

void mock_h2_server_peer::run()
{
    run_protocol();
    listener_.stop();
    // Complete cancellation of an unfinished accept/TLS handshake on our own
    // executor even if the caller's executor is stopped or was never run.
    worker_io_.restart();
    worker_io_.run();
    stage_.store(stage::stopped);
}

void mock_h2_server_peer::run_protocol()
{
    const auto deadline = std::chrono::steady_clock::now() + kIoTimeout;
    while (!stop_.load() && !listener_.handshake_done() &&
           std::chrono::steady_clock::now() < deadline)
    {
        if (worker_io_.stopped()) worker_io_.restart();
        // run_for could dispatch an unlimited series of ready handlers;
        // run_one_for returns control for a stop/deadline check each time.
        worker_io_.run_one_for(kStopPoll);
        if (listener_.accepted()) stage_.store(stage::handshaking);
        // No remaining work before a successful handshake means it failed.
        if (worker_io_.stopped() && !listener_.handshake_done()) break;
    }
    if (stop_.load()) return;
    auto stream = listener_.handshake_done()
        ? listener_.accepted_socket(std::chrono::milliseconds(0)) : nullptr;
    if (!stream)
    {
        io_failed_.store(true);
        return;
    }

    std::error_code ec;
    stoppable_tls_io transport(worker_io_, *stream, stop_);

    // Step 1: Read the 24-byte client connection preface.
    stage_.store(stage::preface);
    std::array<std::uint8_t, kPrefaceSize> preface_buf{};
    transport.read(asio::buffer(preface_buf), ec);
    if (ec ||
        std::memcmp(preface_buf.data(), kPrefaceBytes, kPrefaceSize) != 0)
    {
        io_failed_.store(true);
        return;
    }

    // Step 2: Send an empty server SETTINGS frame
    // (length=0, type=0x4, flags=0, stream_id=0).
    {
        http2::settings_frame initial({}, /*ack=*/false);
        const auto bytes = initial.serialize();
        transport.write_injected(
            injector_,
            std::span<const std::uint8_t>(bytes.data(), bytes.size()), ec);
        if (ec)
        {
            io_failed_.store(true);
            return;
        }
    }

    // Step 3: Read the client's SETTINGS frame header (9 bytes).
    stage_.store(stage::settings_header);
    std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
    transport.read(asio::buffer(hdr_buf), ec);
    if (ec)
    {
        io_failed_.store(true);
        return;
    }
    auto parsed = http2::frame_header::parse(
        std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
    if (parsed.is_err())
    {
        io_failed_.store(true);
        return;
    }
    const auto hdr = parsed.value();
    if (hdr.type != http2::frame_type::settings ||
        (hdr.flags & http2::frame_flags::ack) != 0)
    {
        io_failed_.store(true);
        return;
    }

    // Drain the client SETTINGS payload. Phase 2A intentionally accepts any
    // values — the post-connect path coverage we want does not depend on
    // negotiated settings.
    if (hdr.length > 0)
    {
        stage_.store(stage::settings_payload);
        std::vector<std::uint8_t> payload(hdr.length);
        transport.read(asio::buffer(payload), ec);
        if (ec)
        {
            io_failed_.store(true);
            return;
        }
    }

    // Step 4: Send SETTINGS-ACK
    // (length=0, type=0x4, flags=0x1, stream_id=0).
    {
        http2::settings_frame ack_frame({}, /*ack=*/true);
        const auto bytes = ack_frame.serialize();
        transport.write_injected(
            injector_,
            std::span<const std::uint8_t>(bytes.data(), bytes.size()), ec);
        if (ec)
        {
            io_failed_.store(true);
            return;
        }
    }

    // Require the client's acknowledgment before advertising a completed
    // exchange. Fault-injected server SETTINGS must not look successful.
    stage_.store(stage::settings_ack);
    std::array<std::uint8_t, kFrameHeaderSize> client_ack_buf{};
    transport.read(asio::buffer(client_ack_buf), ec);
    if (ec) { io_failed_.store(true); return; }
    auto client_ack = http2::frame_header::parse(client_ack_buf);
    if (client_ack.is_err() ||
        client_ack.value().type != http2::frame_type::settings ||
        (client_ack.value().flags & http2::frame_flags::ack) == 0 ||
        client_ack.value().length != 0 || client_ack.value().stream_id != 0)
    { io_failed_.store(true); return; }

    settings_exchanged_.store(true);

    // Phase 2E.R3: emit any caller-supplied server-originated frames
    // (e.g. PING, GOAWAY, WINDOW_UPDATE, RST_STREAM, unknown-type) so the
    // client's process_frame dispatcher reaches handler branches that the
    // request path cannot drive. These bytes bypass the injector — callers
    // construct them with exact wire formats so frame::parse will accept
    // them and route them to the intended handler.
    for (const auto& frame_bytes : post_handshake_frames_)
    {
        if (frame_bytes.empty())
        {
            continue;
        }
        stage_.store(stage::post_handshake_write);
        transport.write(asio::buffer(frame_bytes), ec);
        if (ec)
        {
            io_failed_.store(true);
            return;
        }
    }

    // Phase 2A.2: optionally read one client request stream and reply with
    // a server HEADERS+DATA pair before falling through to the drain loop.
    // The drain loop still runs afterwards so client GOAWAY/PING frames
    // emitted during disconnect() are absorbed.
    if (mode_ == reply_mode::echo_one)
    {
        // Read frames until we observe the client's full request stream:
        // a HEADERS frame opens the stream, then any number of DATA frames
        // follow until END_STREAM is seen on either HEADERS or a DATA
        // frame. Frames on other streams or non-stream frames (PING, etc.)
        // are silently drained.
        std::uint32_t request_stream_id = 0;
        bool headers_received = false;
        bool stream_complete = false;
        while (!stop_.load() && !stream_complete)
        {
            stage_.store(stage::request_header);
            std::array<std::uint8_t, kFrameHeaderSize> req_hdr_buf{};
            transport.read(asio::buffer(req_hdr_buf), ec);
            if (ec)
            {
                io_failed_.store(true);
                return;
            }
            auto req_parsed = http2::frame_header::parse(
                std::span<const std::uint8_t>(
                    req_hdr_buf.data(), req_hdr_buf.size()));
            if (req_parsed.is_err())
            {
                io_failed_.store(true);
                return;
            }
            const auto req_h = req_parsed.value();
            if (req_h.length > 0)
            {
                stage_.store(stage::request_payload);
                std::vector<std::uint8_t> req_payload(req_h.length);
                transport.read(asio::buffer(req_payload), ec);
                if (ec)
                {
                    io_failed_.store(true);
                    return;
                }
            }

            const bool end_stream =
                (req_h.flags & http2::frame_flags::end_stream) != 0;

            if (req_h.type == http2::frame_type::headers && !headers_received)
            {
                request_stream_id = req_h.stream_id;
                headers_received = true;
                last_request_stream_id_.store(request_stream_id);
                if (end_stream)
                {
                    stream_complete = true;
                }
            }
            else if (req_h.type == http2::frame_type::data &&
                     headers_received &&
                     req_h.stream_id == request_stream_id)
            {
                if (end_stream)
                {
                    stream_complete = true;
                }
            }
            // Other frames (PING, additional SETTINGS, frames on other
            // streams) are deliberately drained without affecting state.
        }

        if (!stream_complete)
        {
            // Worker was asked to stop or the socket closed before we saw
            // END_STREAM; nothing else to do.
            return;
        }

        request_received_.store(true);

        // Send response HEADERS frame: ":status: 200" encoded as the HPACK
        // indexed header field for static-table index 8 (RFC 7541 Appendix A).
        // A single byte 0x88 is sufficient and avoids pulling in a full
        // HPACK encoder for this minimal reply path.
        stage_.store(stage::response_write);
        const std::vector<std::uint8_t> hpack_status_200{0x88};
        http2::headers_frame response_headers(
            request_stream_id, hpack_status_200,
            /*end_stream=*/false, /*end_headers=*/true);
        const auto resp_hdr_bytes = response_headers.serialize();
        transport.write_injected(
            injector_,
            std::span<const std::uint8_t>(resp_hdr_bytes.data(),
                                          resp_hdr_bytes.size()),
            ec);
        if (ec)
        {
            io_failed_.store(true);
            return;
        }

        // Send response DATA frame with a small body and END_STREAM set.
        // The body content is intentionally short so tests can match it
        // exactly; "ok" is the convention used elsewhere in this fixture.
        const std::vector<std::uint8_t> body{'o', 'k'};
        http2::data_frame response_data(
            request_stream_id, body,
            /*end_stream=*/true, /*padded=*/false);
        const auto resp_data_bytes = response_data.serialize();
        transport.write_injected(
            injector_,
            std::span<const std::uint8_t>(resp_data_bytes.data(),
                                          resp_data_bytes.size()),
            ec);
        if (ec)
        {
            io_failed_.store(true);
            return;
        }

        response_sent_.store(true);
    }

    // Step 5: Drain any subsequent frames (e.g. the GOAWAY emitted by
    // client disconnect(), or further requests on additional streams)
    // until EOF or stop_ is set. After Phase 2A.2 the same loop also
    // absorbs frames the client may emit after consuming the response.
    while (!stop_.load())
    {
        stage_.store(stage::drain_header);
        std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
        transport.read(asio::buffer(drain_hdr), ec);
        if (ec)
        {
            break;
        }
        auto drain_parsed = http2::frame_header::parse(
            std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
        if (drain_parsed.is_err())
        {
            break;
        }
        const auto drain_h = drain_parsed.value();
        if (drain_h.length > 0)
        {
            stage_.store(stage::drain_payload);
            std::vector<std::uint8_t> payload(drain_h.length);
            transport.read(asio::buffer(payload), ec);
            if (ec)
            {
                break;
            }
        }
    }
}

} // namespace kcenon::network::tests::support
