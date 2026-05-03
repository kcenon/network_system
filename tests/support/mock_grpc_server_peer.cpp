// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file mock_grpc_server_peer.cpp
 * @brief Implementation of server-side gRPC framing peer
 *        (Phase 2B of Issue #1074)
 */

#include "mock_grpc_server_peer.h"

#include "internal/protocols/http2/frame.h"

#include <asio/buffer.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <span>
#include <string_view>
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

// HPACK-encoded response headers for the unary echo path. Each entry uses
// the literal-with-incremental-indexing form (0x40 prefix) followed by a
// length-prefixed name and length-prefixed value. Static-table indexed
// fields are used where they exist:
//
//   0x88            -> :status: 200            (static index 8)
//
// content-type and grpc-status are sent as literal headers because the
// h2 client (http2_client) does not maintain a populated dynamic table
// strict enough to require indexed encoding here, and the static table
// has no exact match for "application/grpc" or numeric grpc-status.
//
// Encoding helpers below build the payload at runtime to keep the byte
// layout legible without committing to a hand-rolled blob.

// Build a literal-without-indexing HPACK header field (0x00 prefix per
// RFC 7541 Section 6.2.2). Both name and value are sent as plain (non-
// Huffman) length-prefixed strings. Length is encoded with the simple
// 7-bit form because all values used here fit in <=127 bytes.
auto encode_literal_header(std::string_view name, std::string_view value)
    -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> out;
    out.reserve(2 + name.size() + value.size());
    out.push_back(0x00); // literal header field without indexing, new name
    out.push_back(static_cast<std::uint8_t>(name.size()));
    out.insert(out.end(), name.begin(), name.end());
    out.push_back(static_cast<std::uint8_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
    return out;
}

// Build the HPACK block for the response HEADERS frame:
//   :status: 200             (indexed, static table 8)
//   content-type: application/grpc
auto build_response_header_block() -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> block;
    block.push_back(0x88); // indexed header field, static index 8 = :status: 200
    auto ct = encode_literal_header("content-type", "application/grpc");
    block.insert(block.end(), ct.begin(), ct.end());
    return block;
}

// Build the HPACK block for the trailing HEADERS frame:
//   grpc-status: 0
auto build_trailer_header_block() -> std::vector<std::uint8_t>
{
    return encode_literal_header("grpc-status", "0");
}

// Build a length-prefixed gRPC message body
// (1 byte compressed flag = 0, 4 bytes big-endian length, payload).
auto build_grpc_framed_body(std::span<const std::uint8_t> payload)
    -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> framed;
    framed.reserve(5 + payload.size());
    framed.push_back(0x00); // not compressed
    const auto len = static_cast<std::uint32_t>(payload.size());
    framed.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
    framed.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
    framed.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
    framed.push_back(static_cast<std::uint8_t>(len & 0xFF));
    framed.insert(framed.end(), payload.begin(), payload.end());
    return framed;
}

} // namespace

mock_grpc_server_peer::mock_grpc_server_peer(asio::io_context& io,
                                             grpc_reply_mode mode)
    : listener_(io), mode_(mode)
{
    worker_ = std::thread([this]() { this->run(); });
}

mock_grpc_server_peer::~mock_grpc_server_peer()
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

auto mock_grpc_server_peer::request_body() const -> std::vector<std::uint8_t>
{
    std::lock_guard<std::mutex> lock(request_body_mutex_);
    return request_body_;
}

void mock_grpc_server_peer::run()
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

    // Drain the client SETTINGS payload. Any negotiated values are
    // accepted as-is — gRPC unary success path coverage does not depend
    // on enforcing them.
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

    // grpc_reply_mode::echo_unary: read one client request stream and
    // reply with HEADERS (status 200) + DATA (length-prefixed body) +
    // trailing HEADERS (grpc-status: 0, END_STREAM). The drain loop
    // afterwards absorbs PING/GOAWAY frames emitted during disconnect().
    if (mode_ == grpc_reply_mode::echo_unary)
    {
        std::uint32_t request_stream_id = 0;
        bool headers_received = false;
        bool stream_complete = false;
        std::vector<std::uint8_t> accumulated_body;

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

            std::vector<std::uint8_t> req_payload;
            if (req_h.length > 0)
            {
                req_payload.resize(req_h.length);
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
                accumulated_body.insert(accumulated_body.end(),
                                        req_payload.begin(),
                                        req_payload.end());
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

        {
            std::lock_guard<std::mutex> lock(request_body_mutex_);
            request_body_ = std::move(accumulated_body);
        }
        request_received_.store(true);

        // Send response HEADERS frame: ":status: 200" + content-type.
        // END_HEADERS set, END_STREAM unset (DATA + trailers follow).
        {
            const auto header_block = build_response_header_block();
            http2::headers_frame response_headers(
                request_stream_id, header_block,
                /*end_stream=*/false, /*end_headers=*/true);
            const auto bytes = response_headers.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec)
            {
                io_failed_.store(true);
                return;
            }
        }

        // Send response DATA frame with a length-prefixed gRPC message.
        // The body content is intentionally short ("ok") so tests can
        // match it exactly.
        {
            constexpr std::array<std::uint8_t, 2> payload{'o', 'k'};
            const auto framed = build_grpc_framed_body(
                std::span<const std::uint8_t>(payload.data(), payload.size()));
            http2::data_frame response_data(
                request_stream_id, framed,
                /*end_stream=*/false, /*padded=*/false);
            const auto bytes = response_data.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec)
            {
                io_failed_.store(true);
                return;
            }
        }

        // Send trailing HEADERS frame: "grpc-status: 0", END_STREAM set.
        // gRPC carries terminal status as HTTP/2 trailers in the success
        // path; the client extracts this from response.headers (the
        // http2_client merges trailers into the headers vector before
        // delivering the response).
        {
            const auto trailer_block = build_trailer_header_block();
            http2::headers_frame trailers(
                request_stream_id, trailer_block,
                /*end_stream=*/true, /*end_headers=*/true);
            const auto bytes = trailers.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec)
            {
                io_failed_.store(true);
                return;
            }
        }

        response_sent_.store(true);
    }

    // Drain any subsequent frames (e.g. the GOAWAY emitted by client
    // disconnect(), or further PING frames) until EOF or stop_ is set.
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
