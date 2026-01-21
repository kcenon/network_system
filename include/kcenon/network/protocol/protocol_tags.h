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

#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>

namespace kcenon::network::protocol {

/*!
 * \struct tcp_protocol
 * \brief Protocol tag for TCP transport.
 *
 * Represents the Transmission Control Protocol (TCP), a connection-oriented
 * protocol that provides reliable, ordered delivery of data streams.
 *
 * ### Characteristics
 * - Connection-oriented
 * - Reliable delivery with acknowledgments
 * - Ordered packet delivery
 * - Flow control and congestion control
 */
struct tcp_protocol {
    static constexpr std::string_view name{"tcp"};
    static constexpr bool is_connection_oriented = true;
    static constexpr bool is_reliable = true;
};

/*!
 * \struct udp_protocol
 * \brief Protocol tag for UDP transport.
 *
 * Represents the User Datagram Protocol (UDP), a connectionless protocol
 * that provides fast, unreliable delivery of individual datagrams.
 *
 * ### Characteristics
 * - Connectionless
 * - Unreliable delivery (best-effort)
 * - No ordering guarantees
 * - Low overhead
 */
struct udp_protocol {
    static constexpr std::string_view name{"udp"};
    static constexpr bool is_connection_oriented = false;
    static constexpr bool is_reliable = false;
};

/*!
 * \struct websocket_protocol
 * \brief Protocol tag for WebSocket transport.
 *
 * Represents the WebSocket protocol, providing full-duplex communication
 * channels over a single TCP connection with HTTP upgrade handshake.
 *
 * ### Characteristics
 * - Connection-oriented (over TCP)
 * - Full-duplex communication
 * - Frame-based messaging
 * - HTTP upgrade handshake
 */
struct websocket_protocol {
    static constexpr std::string_view name{"websocket"};
    static constexpr bool is_connection_oriented = true;
    static constexpr bool is_reliable = true;
};

/*!
 * \struct quic_protocol
 * \brief Protocol tag for QUIC transport.
 *
 * Represents the QUIC protocol, a modern transport protocol built on UDP
 * that provides multiplexed connections with built-in TLS 1.3 encryption.
 *
 * ### Characteristics
 * - Built on UDP with reliability layer
 * - Multiplexed streams
 * - Built-in encryption (TLS 1.3)
 * - Reduced connection establishment latency (0-RTT)
 */
struct quic_protocol {
    static constexpr std::string_view name{"quic"};
    static constexpr bool is_connection_oriented = true;
    static constexpr bool is_reliable = true;
};

/*!
 * \concept Protocol
 * \brief Concept that constrains types to be valid protocol tags.
 *
 * A valid protocol tag must have:
 * - A static constexpr `name` member convertible to `std::string_view`
 * - A static constexpr bool `is_connection_oriented` member
 * - A static constexpr bool `is_reliable` member
 *
 * ### Satisfied Types
 * - `tcp_protocol`
 * - `udp_protocol`
 * - `websocket_protocol`
 * - `quic_protocol`
 *
 * ### Usage
 * \code
 * template<Protocol P, TlsPolicy T = no_tls>
 * class messaging_client { ... };
 * \endcode
 */
template <typename T>
concept Protocol = requires {
    { T::name } -> std::convertible_to<std::string_view>;
    { T::is_connection_oriented } -> std::convertible_to<bool>;
    { T::is_reliable } -> std::convertible_to<bool>;
};

/*!
 * \brief Helper variable template to check if protocol is connection-oriented.
 */
template <Protocol P>
inline constexpr bool is_connection_oriented_v = P::is_connection_oriented;

/*!
 * \brief Helper variable template to check if protocol is reliable.
 */
template <Protocol P>
inline constexpr bool is_reliable_v = P::is_reliable;

/*!
 * \brief Helper variable template to get protocol name.
 */
template <Protocol P>
inline constexpr std::string_view protocol_name_v = P::name;

/*!
 * \brief Type trait to check if a protocol is connection-oriented.
 */
template <Protocol P>
struct is_connection_oriented : std::bool_constant<P::is_connection_oriented> {};

/*!
 * \brief Type trait to check if a protocol is reliable.
 */
template <Protocol P>
struct is_reliable : std::bool_constant<P::is_reliable> {};

}  // namespace kcenon::network::protocol
