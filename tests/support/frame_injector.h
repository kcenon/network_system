// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file frame_injector.h
 * @brief Composable byte-level fault hooks for hermetic protocol peers
 *        (Phase 2E of Issue #1074).
 *
 * The earlier phases of #1074 (2A-2D) shipped server-side framing peers and
 * friend-test probes that drive the *happy path* of every protocol family
 * (HTTP/2, gRPC, QUIC, WebSocket). Several uncovered branches in
 * @c http2_client.cpp, @c grpc/client.cpp, @c quic_socket.cpp, and
 * @c websocket_server.cpp are reachable only when the peer emits malformed,
 * partial, or delayed bytes. @ref frame_injector is the substrate for
 * driving those branches: a small, reusable transform applied to any
 * byte buffer immediately before it is written to a sync ASIO stream.
 *
 * Four modes are supported (RFC-style fault classes):
 *
 * - @ref injection_mode::none — pass-through. Default; preserves the existing
 *   peer behavior so opting into a @ref frame_injector is a no-op for tests
 *   that do not need fault injection.
 * - @ref injection_mode::drop — skip the write entirely. The caller is told
 *   the write happened so it does not retry, but no bytes leave the peer.
 *   Useful for "what does the client do when an expected reply never arrives"
 *   tests without resorting to socket-close races.
 * - @ref injection_mode::truncate — write only the first
 *   @ref injection_spec::truncate_at bytes of the buffer. Drives length-prefix
 *   parsers and frame-header readers down their short-read branches.
 * - @ref injection_mode::malform — XOR the byte at
 *   @ref injection_spec::malform_offset with @ref injection_spec::malform_xor
 *   before writing. The simplest single-byte corruption that exercises
 *   integrity-check / parse-validation branches without inventing semantics.
 * - @ref injection_mode::slow_write — write the buffer one byte at a time,
 *   sleeping @ref injection_spec::slow_step between bytes. Drives partial-read
 *   branches in protocols that buffer incrementally.
 *
 * All modes are stateless across writes (the spec is captured once at
 * construction). Concurrent writes from a single peer worker thread are
 * fine; sharing one @ref frame_injector across threads is not.
 *
 * Hermetic discipline: this header pulls in only `<asio/write.hpp>` and the
 * standard library; it does not require `<openssl/...>` or any production
 * protocol header, so compile cost stays in line with the rest of the
 * `network_test_support` library.
 */

#include <asio/buffer.hpp>
#include <asio/write.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <system_error>
#include <thread>
#include <vector>

namespace kcenon::network::tests::support
{

/**
 * @brief Selects the byte-level fault to apply to a single write.
 */
enum class injection_mode
{
    /// Pass-through. The peer write proceeds unchanged.
    none,
    /// Skip the write entirely; no bytes are emitted on the wire.
    drop,
    /// Write only the first @ref injection_spec::truncate_at bytes.
    truncate,
    /// Flip the bits of one byte at @ref injection_spec::malform_offset
    /// (XOR with @ref injection_spec::malform_xor) before writing the
    /// otherwise-unmodified buffer.
    malform,
    /// Write byte-by-byte with @ref injection_spec::slow_step between bytes.
    slow_write
};

/**
 * @brief Parameters for a single @ref injection_mode.
 *
 * Fields are read only by the modes that name them; the rest are ignored.
 * Default values are safe for any mode (no truncation, no XOR, no sleep).
 */
struct injection_spec
{
    /// Selected fault. Defaults to @ref injection_mode::none.
    injection_mode mode{injection_mode::none};

    /// For @ref injection_mode::truncate. Number of leading bytes to keep.
    /// If zero or larger than the buffer, the entire buffer is dropped or
    /// passed unchanged respectively.
    std::size_t truncate_at{0};

    /// For @ref injection_mode::malform. Index into the buffer of the byte
    /// to corrupt. Out-of-range offsets are silently clamped to a no-op.
    std::size_t malform_offset{0};

    /// For @ref injection_mode::malform. Value XOR'd with the byte at
    /// @ref malform_offset. Default flips every bit.
    std::uint8_t malform_xor{0xFF};

    /// For @ref injection_mode::slow_write. Pause between consecutive
    /// 1-byte writes. Defaults to 1 ms — small enough to keep the test
    /// fast, large enough to coax a partial-read in most async parsers.
    std::chrono::microseconds slow_step{std::chrono::milliseconds{1}};
};

/**
 * @brief Apply a captured @ref injection_spec to byte buffers about to be
 *        written via a sync ASIO stream.
 *
 * Typical usage from inside a hermetic peer worker (see
 * @ref mock_h2_server_peer for the unmodified baseline pattern):
 * @code
 * frame_injector inject{spec};
 * std::error_code ec;
 *
 * // Was: asio::write(*stream, asio::buffer(bytes), ec);
 * inject.write(*stream, std::span(bytes), ec);
 * @endcode
 *
 * For tests that build raw client→server byte streams (e.g. WebSocket
 * upgrade requests fed directly into @ref ws_server_probe), use the pure
 * transform variant:
 * @code
 * frame_injector inject{{.mode = injection_mode::truncate, .truncate_at = 8}};
 * auto maybe_bytes = inject.transform(std::span(client_bytes));
 * if (maybe_bytes) {
 *     // Write *maybe_bytes to the destination socket.
 * }
 * @endcode
 */
class frame_injector
{
public:
    /// Construct a pass-through injector (@ref injection_mode::none).
    frame_injector() noexcept = default;

    /// Construct an injector that applies @p spec on every write.
    explicit frame_injector(injection_spec spec) noexcept : spec_(spec) {}

    /// Spec the injector was constructed with.
    [[nodiscard]] auto spec() const noexcept -> const injection_spec&
    {
        return spec_;
    }

    /// Convenience accessor for the selected mode.
    [[nodiscard]] auto mode() const noexcept -> injection_mode
    {
        return spec_.mode;
    }

    /// True if this injector would change the wire bytes for any non-empty
    /// buffer (i.e. mode is anything other than @ref injection_mode::none).
    [[nodiscard]] auto active() const noexcept -> bool
    {
        return spec_.mode != injection_mode::none;
    }

    /**
     * @brief Pure transform: produce the bytes that should actually be
     *        emitted given @p input and the captured spec.
     *
     * @param input Buffer the caller intended to write.
     * @return The bytes to write, or @c std::nullopt for
     *         @ref injection_mode::drop (caller skips the write entirely).
     *
     * For @ref injection_mode::slow_write this returns the unchanged buffer
     * because the per-byte pacing must happen at write time; pure-transform
     * callers that need pacing should use @ref write instead.
     */
    [[nodiscard]] auto transform(std::span<const std::uint8_t> input) const
        -> std::optional<std::vector<std::uint8_t>>;

    /**
     * @brief Apply the injection spec and write the result to @p stream.
     *
     * Returns @c true if a write was performed (or successfully skipped via
     * @ref injection_mode::drop) and @p stream remains usable. Returns
     * @c false only when the underlying I/O failed; @p ec carries the error
     * in that case. For @ref injection_mode::drop, @p ec is cleared.
     *
     * @tparam SyncStream Anything matching ASIO's sync write stream concept
     *         (e.g. @c asio::ssl::stream<asio::ip::tcp::socket>,
     *         @c asio::ip::tcp::socket).
     */
    template <typename SyncStream>
    auto write(SyncStream& stream, std::span<const std::uint8_t> input,
               std::error_code& ec) const -> bool;

private:
    injection_spec spec_{};
};

template <typename SyncStream>
auto frame_injector::write(SyncStream& stream,
                           std::span<const std::uint8_t> input,
                           std::error_code& ec) const -> bool
{
    ec.clear();

    switch (spec_.mode)
    {
    case injection_mode::none:
        asio::write(stream, asio::buffer(input.data(), input.size()), ec);
        return !ec;

    case injection_mode::drop:
        // Pretend the write happened. No bytes leave the peer.
        return true;

    case injection_mode::truncate:
    {
        const auto keep = std::min(spec_.truncate_at, input.size());
        if (keep == 0)
        {
            return true;
        }
        asio::write(stream, asio::buffer(input.data(), keep), ec);
        return !ec;
    }

    case injection_mode::malform:
    {
        if (input.empty())
        {
            return true;
        }
        std::vector<std::uint8_t> buf(input.begin(), input.end());
        if (spec_.malform_offset < buf.size())
        {
            buf[spec_.malform_offset] =
                static_cast<std::uint8_t>(buf[spec_.malform_offset]
                                          ^ spec_.malform_xor);
        }
        asio::write(stream, asio::buffer(buf), ec);
        return !ec;
    }

    case injection_mode::slow_write:
    {
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            asio::write(stream, asio::buffer(input.data() + i, 1), ec);
            if (ec)
            {
                return false;
            }
            if (i + 1 < input.size() && spec_.slow_step.count() > 0)
            {
                std::this_thread::sleep_for(spec_.slow_step);
            }
        }
        return true;
    }
    }

    // Unreachable: the enum is exhaustive. Treat any future mode as a
    // pass-through so adding a new entry never silently breaks existing
    // callers that compile against this header.
    asio::write(stream, asio::buffer(input.data(), input.size()), ec);
    return !ec;
}

} // namespace kcenon::network::tests::support
