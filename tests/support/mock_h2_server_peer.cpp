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

mock_h2_server_peer::mock_h2_server_peer(asio::io_context& io, reply_mode mode)
    : listener_(io), mode_(mode)
{
    worker_ = std::thread([this]() { this->run(); });
}

mock_h2_server_peer::~mock_h2_server_peer()
{
    stop_.store(true);
    // The expected lifecycle is that the client issues disconnect() before
    // this destructor runs. disconnect() emits a GOAWAY frame and closes
    // the socket, which causes the worker's blocking read to return EOF
    // and the worker to exit promptly.
    if (worker_.joinable())
    {
        worker_.join();
    }
}

void mock_h2_server_peer::run()
{
    auto stream = listener_.accepted_socket(std::chrono::seconds(5));
    if (!stream)
    {
        io_failed_.store(true);
        return;
    }

    std::error_code ec;

    // Step 1: Read the 24-byte client connection preface.
    std::array<std::uint8_t, kPrefaceSize> preface_buf{};
    asio::read(*stream, asio::buffer(preface_buf), ec);
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
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec)
        {
            io_failed_.store(true);
            return;
        }
    }

    // Step 3: Read the client's SETTINGS frame header (9 bytes).
    std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
    asio::read(*stream, asio::buffer(hdr_buf), ec);
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
        std::vector<std::uint8_t> payload(hdr.length);
        asio::read(*stream, asio::buffer(payload), ec);
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
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec)
        {
            io_failed_.store(true);
            return;
        }
    }

    settings_exchanged_.store(true);

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
            std::array<std::uint8_t, kFrameHeaderSize> req_hdr_buf{};
            asio::read(*stream, asio::buffer(req_hdr_buf), ec);
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
                std::vector<std::uint8_t> req_payload(req_h.length);
                asio::read(*stream, asio::buffer(req_payload), ec);
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
        const std::vector<std::uint8_t> hpack_status_200{0x88};
        http2::headers_frame response_headers(
            request_stream_id, hpack_status_200,
            /*end_stream=*/false, /*end_headers=*/true);
        const auto resp_hdr_bytes = response_headers.serialize();
        asio::write(*stream, asio::buffer(resp_hdr_bytes), ec);
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
        asio::write(*stream, asio::buffer(resp_data_bytes), ec);
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
        std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
        asio::read(*stream, asio::buffer(drain_hdr), ec);
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
            std::vector<std::uint8_t> payload(drain_h.length);
            asio::read(*stream, asio::buffer(payload), ec);
            if (ec)
            {
                break;
            }
        }
    }
}

} // namespace kcenon::network::tests::support
