// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file mock_quic_peer_loop.cpp
 * @brief Implementation of server-side QUIC Initial echo peer
 *        (Phase 2C of Issue #1074)
 */

#include "mock_quic_peer_loop.h"

#include "internal/protocols/quic/crypto.h"
#include "internal/protocols/quic/packet.h"
#include "internal/protocols/quic/varint.h"
#include "kcenon/network/detail/protocols/quic/connection_id.h"

#include <asio/ip/udp.hpp>

#include <array>
#include <cstdint>
#include <thread>
#include <vector>

namespace kcenon::network::tests::support
{

namespace
{

namespace quic = kcenon::network::protocols::quic;

// Minimum size for a parseable QUIC long-header Initial datagram.
constexpr std::size_t k_min_datagram = 32;

// Maximum receive buffer for one datagram.
constexpr std::size_t k_recv_buf = 4096;

// Build a raw QUIC v1 Initial packet ready to send:
//   long-header | payload-length-varint | packet-number | ciphertext | tag
// with header protection applied.
//
// Returns an empty vector on any failure; the caller treats that as io_failed.
auto build_server_initial(
    const quic::connection_id& dcid,    // server DCID = client SCID (RFC 9000 §17.2.5)
    const quic::connection_id& scid,    // server SCID (fresh random ID)
    const quic::quic_keys& server_keys) // server write keys (from initial_keys::derive)
    -> std::vector<uint8_t>
{
    constexpr uint64_t k_pn = 0;

    // Plaintext payload: one PADDING byte (0x00) + crypto_frame.
    // crypto_frame layout (RFC 9000 §19.6):
    //   type (varint) = 0x06
    //   offset (varint) = 0x00
    //   length (varint) = 8
    //   data (8 zero bytes) -- TLS record stub; process_crypto_frame fires
    //                          regardless of TLS parse outcome.
    std::vector<uint8_t> plaintext;
    plaintext.push_back(0x00); // PADDING
    // crypto_frame type = 0x06
    auto type_enc = quic::varint::encode(0x06);
    plaintext.insert(plaintext.end(), type_enc.begin(), type_enc.end());
    // offset = 0
    auto off_enc = quic::varint::encode(0);
    plaintext.insert(plaintext.end(), off_enc.begin(), off_enc.end());
    // length = 8
    auto len_enc = quic::varint::encode(8);
    plaintext.insert(plaintext.end(), len_enc.begin(), len_enc.end());
    // 8 zero bytes of TLS stub
    plaintext.insert(plaintext.end(), 8, 0x00);

    // Ciphertext size = plaintext + AEAD tag.
    const std::size_t cipher_size = plaintext.size() + quic::aead_tag_size;

    // Packet number length for PN=0 with no prior packets is 1 byte.
    const std::size_t pn_len = 1;

    // Build the unprotected long header including PN and payload-length varint.
    // Wire order (RFC 9000 §17.2.2):
    //   first_byte | version(4) | DCID-len(1) | DCID | SCID-len(1) | SCID
    //   | token-len-varint(1) | payload-length-varint | packet-number(pn_len)
    std::vector<uint8_t> header;

    // first_byte: 1 1 00 00 (pn_len-1)
    // Long header = 0x80, fixed = 0x40, type Initial = 0x00<<4, pn_len-1 = 0
    const uint8_t first_byte = 0x80 | 0x40 | (0x00 << 4) | static_cast<uint8_t>(pn_len - 1);
    header.push_back(first_byte);

    // QUIC version 1
    header.push_back(0x00);
    header.push_back(0x00);
    header.push_back(0x00);
    header.push_back(0x01);

    // DCID
    header.push_back(static_cast<uint8_t>(dcid.length()));
    auto dcid_span = dcid.data();
    header.insert(header.end(), dcid_span.begin(), dcid_span.end());

    // SCID
    header.push_back(static_cast<uint8_t>(scid.length()));
    auto scid_span = scid.data();
    header.insert(header.end(), scid_span.begin(), scid_span.end());

    // Token length = 0 (no token for server Initial per RFC 9000 §17.2.2)
    header.push_back(0x00);

    // Payload length = pn_len + cipher_size (varint)
    auto payload_len_enc = quic::varint::encode(pn_len + cipher_size);
    header.insert(header.end(), payload_len_enc.begin(), payload_len_enc.end());

    // Packet number (1 byte, PN=0)
    header.push_back(static_cast<uint8_t>(k_pn));

    // Encrypt: protect(server_write_keys, header_as_AAD, plaintext, pn)
    // protect() returns header || ciphertext || tag.
    auto protect_result = quic::packet_protection::protect(
        server_keys,
        std::span<const uint8_t>(header.data(), header.size()),
        std::span<const uint8_t>(plaintext.data(), plaintext.size()),
        k_pn);
    if (protect_result.is_err())
    {
        return {};
    }

    auto packet = protect_result.value();

    // Apply header protection (RFC 9001 §5.4).
    // sample_offset = pn_offset + 4, where pn_offset = header.size() - pn_len.
    // protect() prepended the original header bytes, so:
    //   pn_offset in the output = header.size() - pn_len
    const std::size_t pn_offset = header.size() - pn_len;
    const std::size_t sample_offset = pn_offset + 4;

    if (sample_offset + quic::hp_sample_size > packet.size())
    {
        return {};
    }

    std::span<const uint8_t> sample(packet.data() + sample_offset, quic::hp_sample_size);
    auto hp_result = quic::packet_protection::protect_header(
        server_keys,
        std::span<uint8_t>(packet.data(), pn_offset + pn_len),
        pn_offset,
        pn_len,
        sample);
    if (hp_result.is_err())
    {
        return {};
    }

    return packet;
}

} // namespace

mock_quic_peer_loop::mock_quic_peer_loop(asio::io_context& io,
                                         injection_spec inject)
    : socket_(io, asio::ip::udp::v4()), injector_(inject)
{
    // Bind to loopback with an ephemeral port.
    socket_.bind(asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), 0));
    endpoint_ = socket_.local_endpoint();
    port_ = endpoint_.port();

    worker_ = std::thread([this]() { this->run(); });
}

mock_quic_peer_loop::~mock_quic_peer_loop()
{
    stop_.store(true);
    // Close the socket to unblock any pending recv_from.
    std::error_code ec;
    socket_.close(ec);
    if (worker_.joinable())
    {
        worker_.join();
    }
}

void mock_quic_peer_loop::run()
{
    try
    {
        std::array<uint8_t, k_recv_buf> buf{};
        asio::ip::udp::endpoint sender;
        std::error_code ec;

        // Step 1: receive the client's first Initial datagram.
        const std::size_t n = socket_.receive_from(
            asio::buffer(buf.data(), buf.size()), sender, 0, ec);
        if (ec || n < k_min_datagram)
        {
            io_failed_.store(true);
            return;
        }

        std::span<const uint8_t> datagram(buf.data(), n);

        // Step 2: parse the long header to get client DCID (for key derivation)
        // and client SCID (becomes server DCID per RFC 9000 §17.2.5).
        auto header_result = quic::packet_parser::parse_long_header(datagram);
        if (header_result.is_err())
        {
            io_failed_.store(true);
            return;
        }
        const auto& [client_header, client_header_len] = header_result.value();
        (void)client_header_len;

        // Derive initial secrets from the client's original DCID (RFC 9001 §5.2).
        // initial_keys::derive returns from the client's perspective:
        //   result.read  = server keys  (server writes with these for us)
        //   result.write = client keys
        auto keys_result = quic::initial_keys::derive(client_header.dest_conn_id);
        if (keys_result.is_err())
        {
            io_failed_.store(true);
            return;
        }

        // Server writes with "read" keys (from client's view = server write).
        const auto& server_write_keys = keys_result.value().read;

        // Step 3-6: build, encrypt, and send the server Initial reply.
        // Server DCID = client SCID (RFC 9000 §17.2.5).
        const auto server_dcid = client_header.src_conn_id;
        // Server SCID: a fresh random ID so the client can route replies.
        const auto server_scid = quic::connection_id::generate(8);

        const auto packet = build_server_initial(server_dcid, server_scid, server_write_keys);
        if (packet.empty())
        {
            io_failed_.store(true);
            return;
        }

        // Step 6: send reply back to the sender (the client's address).
        // Phase 2E: optionally apply byte-level fault injection. UDP is a
        // datagram protocol, so the injector's pure transform variant is
        // used here — the per-byte pacing slow_write mode is meaningless
        // for a single send_to and is treated as a pass-through by
        // transform().
        auto wire = injector_.transform(
            std::span<const std::uint8_t>(packet.data(), packet.size()));
        if (wire.has_value())
        {
            if (!wire->empty())
            {
                socket_.send_to(asio::buffer(wire->data(), wire->size()),
                                sender, 0, ec);
                if (ec)
                {
                    io_failed_.store(true);
                    return;
                }
            }
            // Step 7: signal that the Initial was sent. Truncate-to-zero
            // is treated the same as a successful send (the test asked
            // for an empty datagram).
            initial_sent_.store(true);
        }
        // injection_mode::drop: deliberately skip send_to. The client
        // never sees a server Initial, which drives idle-timeout /
        // retransmit branches in quic_socket. initial_sent_ stays false
        // so tests can distinguish drop from a normal reply.

        // Step 8: drain remaining datagrams until stop_ is set.
        while (!stop_.load())
        {
            std::array<uint8_t, k_recv_buf> drain_buf{};
            asio::ip::udp::endpoint drain_sender;
            socket_.receive_from(
                asio::buffer(drain_buf.data(), drain_buf.size()),
                drain_sender, 0, ec);
            if (ec)
            {
                break;
            }
        }
    }
    catch (...)
    {
        io_failed_.store(true);
    }
}

} // namespace kcenon::network::tests::support
