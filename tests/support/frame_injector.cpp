// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file frame_injector.cpp
 * @brief Implementation of the pure byte-buffer transform half of
 *        @ref kcenon::network::tests::support::frame_injector
 *        (Phase 2E of Issue #1074).
 *
 * The sync-stream @c write() entry point is template-defined in the header
 * so that callers can pass any ASIO sync stream type (TLS, plain TCP, UDP)
 * without an explicit instantiation list. This translation unit only owns
 * @c transform(), which is the variant used by tests that build raw byte
 * streams off-stream (e.g. malformed WebSocket upgrade requests fed
 * directly into @c ws_server_probe::invoke_handle_new_connection).
 */

#include "frame_injector.h"

#include <algorithm>

namespace kcenon::network::tests::support
{

auto frame_injector::transform(std::span<const std::uint8_t> input) const
    -> std::optional<std::vector<std::uint8_t>>
{
    switch (spec_.mode)
    {
    case injection_mode::none:
    case injection_mode::slow_write:
        // slow_write has no meaningful byte-level transform; pacing is
        // applied only at write() time. Treat it as a pass-through here so
        // pure-transform callers receive a usable buffer.
        return std::vector<std::uint8_t>(input.begin(), input.end());

    case injection_mode::drop:
        return std::nullopt;

    case injection_mode::truncate:
    {
        const auto keep = std::min(spec_.truncate_at, input.size());
        return std::vector<std::uint8_t>(input.begin(),
                                         input.begin()
                                             + static_cast<std::ptrdiff_t>(keep));
    }

    case injection_mode::malform:
    {
        std::vector<std::uint8_t> out(input.begin(), input.end());
        if (spec_.malform_offset < out.size())
        {
            out[spec_.malform_offset] =
                static_cast<std::uint8_t>(out[spec_.malform_offset]
                                          ^ spec_.malform_xor);
        }
        return out;
    }
    }

    // Defensive fallback: unknown future mode → pass-through so we never
    // silently drop bytes a caller still expected to see on the wire.
    return std::vector<std::uint8_t>(input.begin(), input.end());
}

} // namespace kcenon::network::tests::support
