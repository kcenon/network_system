// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file mock_h2_server_peer.cpp
 * @brief Implementation of server-side HTTP/2 framing peer
 *        (Phase 2A of Issue #1074)
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

mock_h2_server_peer::mock_h2_server_peer(asio::io_context& io)
    : listener_(io)
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

    // Step 5: Drain any subsequent frames (e.g. the GOAWAY emitted by
    // client disconnect()) until EOF or stop_ is set. Phase 2A does not
    // reply with HEADERS+DATA — that is deferred to Phase 2A.2.
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
