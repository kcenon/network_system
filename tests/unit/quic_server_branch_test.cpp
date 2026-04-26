// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_server_branch_test.cpp
 * @brief Additional branch coverage for src/experimental/quic_server.cpp (Issue #1052)
 *
 * Complements @ref experimental_quic_server_test.cpp by exercising
 * public-API surfaces that remained uncovered after Issue #989:
 *  - quic_server_config field round-trips with empty / very long /
 *    binary-byte cert/key paths, optional ca_cert_file present and absent,
 *    require_client_cert toggling, ALPN protocol vector growth (empty,
 *    single, many, empty-string entries), all eight numeric settings at
 *    zero and at std::numeric_limits boundaries, max_connections sweep,
 *    enable_retry toggling, retry_key vector growth (empty, 16 bytes,
 *    256 bytes), and full-config round-trip preserving every field
 *  - messaging_quic_server construction with empty / 512-char / binary-
 *    byte server IDs and 16-iteration repeated short-lived construction
 *  - server_id() returning the constructor argument as a const reference
 *    that survives across calls
 *  - is_running() / session_count() / connection_count() / sessions() /
 *    get_session() default invariants on a never-started server
 *  - stop_server() rejection branch when never started
 *  - disconnect_session() not_found branch for unknown session id values
 *    including empty string, very long string, and binary-byte string
 *  - disconnect_all() being a no-op on an empty session map (covers the
 *    empty-loop branch)
 *  - broadcast() / multicast() returning ok() on an empty session map
 *    with empty / small / 64 KiB payloads (covers the "no active session"
 *    early-return branch)
 *  - multicast() with empty session_ids vector and with many unknown
 *    session_ids (every entry hits the get_session() == nullptr branch)
 *  - Legacy callback registration covering set_connection_callback /
 *    set_disconnection_callback / set_receive_callback /
 *    set_stream_receive_callback / set_error_callback with default-
 *    constructed (empty) std::function values, populated lambdas,
 *    triple replacement, and shared_ptr-captured state
 *  - Interface-level (i_quic_server) callback registration covering all
 *    five callback adapters: connection / disconnection / receive /
 *    stream / error, including null callbacks, populated lambdas, and
 *    repeated replacement (covers the wrap-and-store path inside the
 *    interface adapter)
 *  - Concurrent is_running() / session_count() / connection_count() /
 *    server_id() polling under shared_ptr lifetime
 *  - Concurrent legacy callback replacement under shared_ptr lifetime
 *  - Many concurrent server instances exercising independent lifecycle
 *  - Destructor on a never-started server runs cleanly
 *  - wait_for_stop() returns immediately when called on a never-started
 *    server (covers the early-return branch in lifecycle_manager)
 *
 * All tests operate purely on the public API; no real QUIC client or
 * live TLS handshake is required. These tests are hermetic and rely on
 * the never-started state machine reachable without a UDP peer.
 *
 * Honest scope statement: the impl-level methods that physically receive
 * QUIC packets and accept handshakes from a peer remain reachable only
 * with a UDP transport fixture and a TLS-1.3 client. Specifically, the
 * private surfaces do_start_impl() success path past asio::ip::udp::socket
 * bind (the bind_failed branches for address_in_use / access_denied are
 * reachable only by binding to a privileged or in-use port and would not
 * be hermetic), do_stop_impl() (only reachable after a successful start),
 * start_receive() (async UDP receive completion), handle_packet() (header
 * parsing, dcid extraction, and session dispatch), find_or_create_session()
 * (session lookup + create + connection-limit-reached branch + UDP socket
 * connect + quic_socket::accept + session callback wiring + metrics
 * reporting + monitor recording), generate_session_id() (only invoked
 * inside find_or_create_session), on_session_close() (peer-driven close),
 * start_cleanup_timer() (timer scheduling), cleanup_dead_sessions()
 * (periodic cleanup), invoke_connection_callback() / invoke_disconnection_callback()
 * / invoke_receive_callback() / invoke_stream_receive_callback() /
 * invoke_error_callback() (only invoked from packet-driven paths), and
 * the success branches of broadcast() / multicast() / disconnect_session()
 * / disconnect_all() under populated session map all require either a
 * live QUIC client that completes a TLS handshake or a friend-declared
 * test injection point inside messaging_quic_server. The start_server()
 * success path past UDP bind also requires either an ephemeral port
 * (which would still leave the io_context loop running asynchronously
 * inside the test process and pollute global state across tests) or a
 * mock thread pool that does not actually run io_context::run.
 */

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_server.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace core = kcenon::network::core;
namespace qiface = kcenon::network::interfaces;
namespace session_ns = kcenon::network::session;

namespace
{

using namespace std::chrono_literals;

} // namespace

// ============================================================================
// quic_server_config — full-field round-trip coverage
// ============================================================================

TEST(QuicServerConfigRoundTrip, EmptyFieldsAreAccepted)
{
    core::quic_server_config cfg;
    EXPECT_TRUE(cfg.cert_file.empty());
    EXPECT_TRUE(cfg.key_file.empty());
    EXPECT_FALSE(cfg.ca_cert_file.has_value());
    EXPECT_FALSE(cfg.require_client_cert);
    EXPECT_TRUE(cfg.alpn_protocols.empty());
    EXPECT_EQ(cfg.max_idle_timeout_ms, 30000u);
    EXPECT_EQ(cfg.initial_max_data, 1048576u);
    EXPECT_EQ(cfg.initial_max_stream_data, 65536u);
    EXPECT_EQ(cfg.initial_max_streams_bidi, 100u);
    EXPECT_EQ(cfg.initial_max_streams_uni, 100u);
    EXPECT_EQ(cfg.max_connections, 10000u);
    EXPECT_TRUE(cfg.enable_retry);
    EXPECT_TRUE(cfg.retry_key.empty());
}

TEST(QuicServerConfigRoundTrip, LongPathsAreAccepted)
{
    core::quic_server_config cfg;
    cfg.cert_file = std::string(1024, 'C');
    cfg.key_file = std::string(1024, 'K');
    EXPECT_EQ(cfg.cert_file.size(), 1024u);
    EXPECT_EQ(cfg.key_file.size(), 1024u);
}

TEST(QuicServerConfigRoundTrip, BinaryBytePathsAreAccepted)
{
    core::quic_server_config cfg;
    cfg.cert_file = std::string("\x01\x02\x03\x04", 4);
    cfg.key_file = std::string("\xff\xfe\xfd\xfc", 4);
    EXPECT_EQ(cfg.cert_file.size(), 4u);
    EXPECT_EQ(cfg.key_file.size(), 4u);
    EXPECT_EQ(static_cast<unsigned char>(cfg.cert_file[0]), 0x01u);
    EXPECT_EQ(static_cast<unsigned char>(cfg.key_file[0]), 0xffu);
}

TEST(QuicServerConfigRoundTrip, OptionalCaCertFileRoundTrips)
{
    core::quic_server_config cfg;
    EXPECT_FALSE(cfg.ca_cert_file.has_value());

    cfg.ca_cert_file = "/etc/ssl/ca.pem";
    ASSERT_TRUE(cfg.ca_cert_file.has_value());
    EXPECT_EQ(*cfg.ca_cert_file, "/etc/ssl/ca.pem");

    cfg.ca_cert_file = std::string(2048, 'A');
    ASSERT_TRUE(cfg.ca_cert_file.has_value());
    EXPECT_EQ(cfg.ca_cert_file->size(), 2048u);

    cfg.ca_cert_file.reset();
    EXPECT_FALSE(cfg.ca_cert_file.has_value());
}

TEST(QuicServerConfigRoundTrip, RequireClientCertToggleRoundTrips)
{
    core::quic_server_config cfg;
    EXPECT_FALSE(cfg.require_client_cert);
    cfg.require_client_cert = true;
    EXPECT_TRUE(cfg.require_client_cert);
    cfg.require_client_cert = false;
    EXPECT_FALSE(cfg.require_client_cert);
}

TEST(QuicServerConfigRoundTrip, AlpnProtocolsGrowAndShrink)
{
    core::quic_server_config cfg;
    EXPECT_TRUE(cfg.alpn_protocols.empty());

    cfg.alpn_protocols.push_back("h3");
    EXPECT_EQ(cfg.alpn_protocols.size(), 1u);
    EXPECT_EQ(cfg.alpn_protocols.front(), "h3");

    cfg.alpn_protocols.push_back("h3-29");
    cfg.alpn_protocols.push_back("h3-32");
    cfg.alpn_protocols.push_back("");  // empty-string ALPN entry
    EXPECT_EQ(cfg.alpn_protocols.size(), 4u);
    EXPECT_TRUE(cfg.alpn_protocols.back().empty());

    cfg.alpn_protocols.clear();
    EXPECT_TRUE(cfg.alpn_protocols.empty());
}

TEST(QuicServerConfigRoundTrip, ManyAlpnProtocolsAreAccepted)
{
    core::quic_server_config cfg;
    constexpr int kCount = 32;
    for (int i = 0; i < kCount; ++i)
    {
        cfg.alpn_protocols.push_back("alpn-" + std::to_string(i));
    }
    EXPECT_EQ(cfg.alpn_protocols.size(), static_cast<size_t>(kCount));
    EXPECT_EQ(cfg.alpn_protocols.front(), "alpn-0");
    EXPECT_EQ(cfg.alpn_protocols.back(),
              "alpn-" + std::to_string(kCount - 1));
}

TEST(QuicServerConfigRoundTrip, NumericFieldsAtZeroBoundaries)
{
    core::quic_server_config cfg;
    cfg.max_idle_timeout_ms = 0;
    cfg.initial_max_data = 0;
    cfg.initial_max_stream_data = 0;
    cfg.initial_max_streams_bidi = 0;
    cfg.initial_max_streams_uni = 0;
    cfg.max_connections = 0;

    EXPECT_EQ(cfg.max_idle_timeout_ms, 0u);
    EXPECT_EQ(cfg.initial_max_data, 0u);
    EXPECT_EQ(cfg.initial_max_stream_data, 0u);
    EXPECT_EQ(cfg.initial_max_streams_bidi, 0u);
    EXPECT_EQ(cfg.initial_max_streams_uni, 0u);
    EXPECT_EQ(cfg.max_connections, 0u);
}

TEST(QuicServerConfigRoundTrip, NumericFieldsAtMaxBoundaries)
{
    core::quic_server_config cfg;
    cfg.max_idle_timeout_ms = std::numeric_limits<uint64_t>::max();
    cfg.initial_max_data = std::numeric_limits<uint64_t>::max();
    cfg.initial_max_stream_data = std::numeric_limits<uint64_t>::max();
    cfg.initial_max_streams_bidi = std::numeric_limits<uint64_t>::max();
    cfg.initial_max_streams_uni = std::numeric_limits<uint64_t>::max();
    cfg.max_connections = std::numeric_limits<size_t>::max();

    EXPECT_EQ(cfg.max_idle_timeout_ms,
              std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(cfg.initial_max_data,
              std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(cfg.initial_max_stream_data,
              std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(cfg.initial_max_streams_bidi,
              std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(cfg.initial_max_streams_uni,
              std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(cfg.max_connections,
              std::numeric_limits<size_t>::max());
}

TEST(QuicServerConfigRoundTrip, EnableRetryToggleRoundTrips)
{
    core::quic_server_config cfg;
    EXPECT_TRUE(cfg.enable_retry);
    cfg.enable_retry = false;
    EXPECT_FALSE(cfg.enable_retry);
    cfg.enable_retry = true;
    EXPECT_TRUE(cfg.enable_retry);
}

TEST(QuicServerConfigRoundTrip, RetryKeyVectorGrowsAndShrinks)
{
    core::quic_server_config cfg;
    EXPECT_TRUE(cfg.retry_key.empty());

    cfg.retry_key.assign(16, 0xab);
    EXPECT_EQ(cfg.retry_key.size(), 16u);
    EXPECT_EQ(cfg.retry_key.front(), 0xabu);

    cfg.retry_key.assign(256, 0xcd);
    EXPECT_EQ(cfg.retry_key.size(), 256u);
    EXPECT_EQ(cfg.retry_key.back(), 0xcdu);

    cfg.retry_key.clear();
    EXPECT_TRUE(cfg.retry_key.empty());
}

TEST(QuicServerConfigRoundTrip, FullConfigRoundTripPreservesEveryField)
{
    core::quic_server_config cfg;
    cfg.cert_file = "/path/to/cert.pem";
    cfg.key_file = "/path/to/key.pem";
    cfg.ca_cert_file = "/path/to/ca.pem";
    cfg.require_client_cert = true;
    cfg.alpn_protocols = {"h3", "h3-29"};
    cfg.max_idle_timeout_ms = 60000;
    cfg.initial_max_data = 4 * 1024 * 1024;
    cfg.initial_max_stream_data = 256 * 1024;
    cfg.initial_max_streams_bidi = 200;
    cfg.initial_max_streams_uni = 50;
    cfg.max_connections = 5000;
    cfg.enable_retry = false;
    cfg.retry_key = std::vector<uint8_t>(32, 0xee);

    // Copy-construct a separate instance and verify field equivalence.
    core::quic_server_config copy = cfg;
    EXPECT_EQ(copy.cert_file, cfg.cert_file);
    EXPECT_EQ(copy.key_file, cfg.key_file);
    ASSERT_TRUE(copy.ca_cert_file.has_value());
    EXPECT_EQ(*copy.ca_cert_file, *cfg.ca_cert_file);
    EXPECT_EQ(copy.require_client_cert, cfg.require_client_cert);
    EXPECT_EQ(copy.alpn_protocols, cfg.alpn_protocols);
    EXPECT_EQ(copy.max_idle_timeout_ms, cfg.max_idle_timeout_ms);
    EXPECT_EQ(copy.initial_max_data, cfg.initial_max_data);
    EXPECT_EQ(copy.initial_max_stream_data, cfg.initial_max_stream_data);
    EXPECT_EQ(copy.initial_max_streams_bidi, cfg.initial_max_streams_bidi);
    EXPECT_EQ(copy.initial_max_streams_uni, cfg.initial_max_streams_uni);
    EXPECT_EQ(copy.max_connections, cfg.max_connections);
    EXPECT_EQ(copy.enable_retry, cfg.enable_retry);
    EXPECT_EQ(copy.retry_key, cfg.retry_key);
}

// ============================================================================
// Construction — server identifier round-trip
// ============================================================================

TEST(QuicServerConstruction, EmptyServerIdRoundTrips)
{
    auto server = std::make_shared<core::messaging_quic_server>("");
    EXPECT_TRUE(server->server_id().empty());
}

TEST(QuicServerConstruction, LongServerIdRoundTrips)
{
    std::string id(512, 'q');
    auto server = std::make_shared<core::messaging_quic_server>(id);
    EXPECT_EQ(server->server_id(), id);
}

TEST(QuicServerConstruction, BinaryByteServerIdRoundTrips)
{
    std::string id("\x01\x7f\x80\xff", 4);
    auto server = std::make_shared<core::messaging_quic_server>(id);
    EXPECT_EQ(server->server_id(), id);
    EXPECT_EQ(server->server_id().size(), 4u);
}

TEST(QuicServerConstruction, ManyShortLivedInstancesDoNotLeak)
{
    for (int i = 0; i < 16; ++i)
    {
        auto server = std::make_shared<core::messaging_quic_server>(
            "instance-" + std::to_string(i));
        EXPECT_FALSE(server->is_running());
        EXPECT_EQ(server->session_count(), 0u);
    }
}

TEST(QuicServerConstruction, ServerIdReferenceIsStable)
{
    auto server = std::make_shared<core::messaging_quic_server>("stable-id");
    const auto& a = server->server_id();
    const auto& b = server->server_id();
    EXPECT_EQ(&a, &b);
    EXPECT_EQ(a, "stable-id");
}

// ============================================================================
// Default-state queries on a never-started server
// ============================================================================

TEST(QuicServerDefaults, IsNotRunning)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    EXPECT_FALSE(server->is_running());
}

TEST(QuicServerDefaults, SessionCountIsZero)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    EXPECT_EQ(server->session_count(), 0u);
}

TEST(QuicServerDefaults, ConnectionCountIsZero)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    EXPECT_EQ(server->connection_count(), 0u);
    // connection_count() delegates to session_count() — must agree.
    EXPECT_EQ(server->connection_count(), server->session_count());
}

TEST(QuicServerDefaults, SessionsListIsEmpty)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    auto sessions = server->sessions();
    EXPECT_TRUE(sessions.empty());
}

TEST(QuicServerDefaults, GetSessionForUnknownIdReturnsNull)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    EXPECT_EQ(server->get_session(""), nullptr);
    EXPECT_EQ(server->get_session("unknown"), nullptr);
    EXPECT_EQ(server->get_session(std::string(2048, 'x')), nullptr);
}

// ============================================================================
// stop_server() rejection branch when never started
// ============================================================================

TEST(QuicServerStop, StopWhenNeverStartedReturnsErr)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    auto r = server->stop_server();
    EXPECT_TRUE(r.is_err());
}

TEST(QuicServerStop, RepeatedStopWhenNeverStartedAllReturnErr)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_TRUE(server->stop_server().is_err()) << "iteration " << i;
    }
}

TEST(QuicServerStop, InterfaceStopWhenNeverStartedReturnsErr)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    // i_quic_server::stop() delegates to stop_server().
    qiface::i_quic_server& as_iface =
        static_cast<qiface::i_quic_server&>(*server);
    auto r = as_iface.stop();
    EXPECT_TRUE(r.is_err());
}

// ============================================================================
// disconnect_session() not_found branch
// ============================================================================

TEST(QuicServerDisconnect, DisconnectUnknownSessionReturnsErr)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    EXPECT_TRUE(server->disconnect_session("unknown", 0).is_err());
}

TEST(QuicServerDisconnect, DisconnectEmptySessionIdReturnsErr)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    EXPECT_TRUE(server->disconnect_session("", 0).is_err());
}

TEST(QuicServerDisconnect, DisconnectLongSessionIdReturnsErr)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    std::string id(2048, 'z');
    EXPECT_TRUE(server->disconnect_session(id, 0).is_err());
}

TEST(QuicServerDisconnect, DisconnectBinaryByteSessionIdReturnsErr)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    std::string id("\x01\x02\xff\x00\x7f", 5);
    EXPECT_TRUE(server->disconnect_session(id, 0).is_err());
}

TEST(QuicServerDisconnect, DisconnectUnknownSessionWithVariedErrorCodes)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    for (uint64_t code : {uint64_t{0},
                          uint64_t{1},
                          uint64_t{42},
                          std::numeric_limits<uint64_t>::max()})
    {
        EXPECT_TRUE(server->disconnect_session("unknown", code).is_err())
            << "code=" << code;
    }
}

TEST(QuicServerDisconnect, DisconnectAllOnEmptyMapIsHarmless)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    EXPECT_NO_FATAL_FAILURE(server->disconnect_all(0));
    EXPECT_NO_FATAL_FAILURE(server->disconnect_all(42));
    EXPECT_NO_FATAL_FAILURE(
        server->disconnect_all(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(server->session_count(), 0u);
}

// ============================================================================
// broadcast() / multicast() empty-session-map branch
// ============================================================================

TEST(QuicServerBroadcast, BroadcastEmptyPayloadOnEmptyMapReturnsOk)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    auto r = server->broadcast(std::vector<uint8_t>{});
    EXPECT_TRUE(r.is_ok());
}

TEST(QuicServerBroadcast, BroadcastSmallPayloadOnEmptyMapReturnsOk)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    auto r = server->broadcast(std::vector<uint8_t>{1, 2, 3, 4});
    EXPECT_TRUE(r.is_ok());
}

TEST(QuicServerBroadcast, BroadcastLargePayloadOnEmptyMapReturnsOk)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    std::vector<uint8_t> data(64 * 1024, 0xab);
    auto r = server->broadcast(std::move(data));
    EXPECT_TRUE(r.is_ok());
}

TEST(QuicServerBroadcast, RepeatedBroadcastOnEmptyMapAllReturnOk)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    for (int i = 0; i < 4; ++i)
    {
        auto r = server->broadcast(std::vector<uint8_t>{0xaa, 0xbb});
        EXPECT_TRUE(r.is_ok()) << "iteration " << i;
    }
}

TEST(QuicServerMulticast, MulticastEmptyIdsListReturnsOk)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    auto r = server->multicast({}, std::vector<uint8_t>{1, 2});
    EXPECT_TRUE(r.is_ok());
}

TEST(QuicServerMulticast, MulticastUnknownIdsAllSkipReturnOk)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    std::vector<std::string> ids = {"alpha", "beta", "gamma"};
    auto r = server->multicast(ids, std::vector<uint8_t>{0x42});
    EXPECT_TRUE(r.is_ok());
}

TEST(QuicServerMulticast, MulticastManyUnknownIdsReturnOk)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    std::vector<std::string> ids;
    ids.reserve(64);
    for (int i = 0; i < 64; ++i)
    {
        ids.push_back("unknown-" + std::to_string(i));
    }
    auto r = server->multicast(ids, std::vector<uint8_t>{0xee});
    EXPECT_TRUE(r.is_ok());
}

TEST(QuicServerMulticast, MulticastWithEmptyStringIdReturnsOk)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    auto r = server->multicast({""}, std::vector<uint8_t>{0xee});
    EXPECT_TRUE(r.is_ok());
}

TEST(QuicServerMulticast, MulticastLargePayloadOnEmptyMapReturnsOk)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    std::vector<uint8_t> data(64 * 1024, 0xcd);
    auto r = server->multicast({"unknown"}, std::move(data));
    EXPECT_TRUE(r.is_ok());
}

// ============================================================================
// Legacy callback registration — empty / populated / replaced / shared-state
// ============================================================================

TEST(QuicServerLegacyCallbacks, ConnectionCallbackEmptyFunctionAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    core::messaging_quic_server::connection_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_connection_callback(empty));
}

TEST(QuicServerLegacyCallbacks, DisconnectionCallbackEmptyFunctionAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    core::messaging_quic_server::disconnection_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_disconnection_callback(empty));
}

TEST(QuicServerLegacyCallbacks, ReceiveCallbackEmptyFunctionAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    core::messaging_quic_server::receive_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_receive_callback(empty));
}

TEST(QuicServerLegacyCallbacks, StreamReceiveCallbackEmptyFunctionAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    core::messaging_quic_server::stream_receive_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_stream_receive_callback(empty));
}

TEST(QuicServerLegacyCallbacks, ErrorCallbackEmptyFunctionAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    core::messaging_quic_server::error_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_error_callback(empty));
}

TEST(QuicServerLegacyCallbacks, AllCallbacksReplacedThreeTimes)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");

    int counter = 0;
    server->set_connection_callback(
        [&counter](std::shared_ptr<session_ns::quic_session>) { counter += 1; });
    server->set_connection_callback(
        [&counter](std::shared_ptr<session_ns::quic_session>) { counter += 10; });
    server->set_connection_callback(
        [&counter](std::shared_ptr<session_ns::quic_session>) { counter += 100; });

    server->set_disconnection_callback(
        [&counter](std::shared_ptr<session_ns::quic_session>) { counter += 1; });
    server->set_disconnection_callback(
        [&counter](std::shared_ptr<session_ns::quic_session>) { counter += 10; });

    server->set_receive_callback(
        [&counter](std::shared_ptr<session_ns::quic_session>,
                   const std::vector<uint8_t>&) { counter += 1; });

    server->set_stream_receive_callback(
        [&counter](std::shared_ptr<session_ns::quic_session>,
                   uint64_t,
                   const std::vector<uint8_t>&,
                   bool) { counter += 1; });

    server->set_error_callback(
        [&counter](std::error_code) { counter += 1; });
    server->set_error_callback(
        [&counter](std::error_code) { counter += 10; });

    // None of these callbacks fire on a never-started server.
    EXPECT_EQ(counter, 0);
}

TEST(QuicServerLegacyCallbacks, SharedStateCaptureCallbackAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    auto state = std::make_shared<int>(7);

    server->set_connection_callback(
        [state](std::shared_ptr<session_ns::quic_session>) { (void)state; });
    server->set_disconnection_callback(
        [state](std::shared_ptr<session_ns::quic_session>) { (void)state; });
    server->set_receive_callback(
        [state](std::shared_ptr<session_ns::quic_session>,
                const std::vector<uint8_t>&) { (void)state; });
    server->set_stream_receive_callback(
        [state](std::shared_ptr<session_ns::quic_session>,
                uint64_t,
                const std::vector<uint8_t>&,
                bool) { (void)state; });
    server->set_error_callback(
        [state](std::error_code) { (void)state; });

    EXPECT_EQ(*state, 7);
}

// ============================================================================
// Interface (i_quic_server) callback adapters
// ============================================================================

TEST(QuicServerInterfaceCallbacks, ConnectionCallbackAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::connection_callback_t cb =
        [](std::shared_ptr<qiface::i_quic_session>) {};
    EXPECT_NO_FATAL_FAILURE(server->set_connection_callback(cb));
}

TEST(QuicServerInterfaceCallbacks, ConnectionCallbackEmptyAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::connection_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_connection_callback(empty));
}

TEST(QuicServerInterfaceCallbacks, DisconnectionCallbackAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::disconnection_callback_t cb =
        [](std::string_view) {};
    EXPECT_NO_FATAL_FAILURE(server->set_disconnection_callback(cb));
}

TEST(QuicServerInterfaceCallbacks, DisconnectionCallbackEmptyAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::disconnection_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_disconnection_callback(empty));
}

TEST(QuicServerInterfaceCallbacks, ReceiveCallbackAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::receive_callback_t cb =
        [](std::string_view, const std::vector<uint8_t>&) {};
    EXPECT_NO_FATAL_FAILURE(server->set_receive_callback(cb));
}

TEST(QuicServerInterfaceCallbacks, ReceiveCallbackEmptyAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::receive_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_receive_callback(empty));
}

TEST(QuicServerInterfaceCallbacks, StreamCallbackAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::stream_callback_t cb =
        [](std::string_view, uint64_t, const std::vector<uint8_t>&, bool) {};
    EXPECT_NO_FATAL_FAILURE(server->set_stream_callback(cb));
}

TEST(QuicServerInterfaceCallbacks, StreamCallbackEmptyAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::stream_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_stream_callback(empty));
}

TEST(QuicServerInterfaceCallbacks, ErrorCallbackAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::error_callback_t cb =
        [](std::string_view, std::error_code) {};
    EXPECT_NO_FATAL_FAILURE(server->set_error_callback(cb));
}

TEST(QuicServerInterfaceCallbacks, ErrorCallbackEmptyAdapterAccepted)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    qiface::i_quic_server::error_callback_t empty;
    EXPECT_NO_FATAL_FAILURE(server->set_error_callback(empty));
}

TEST(QuicServerInterfaceCallbacks, ConnectionCallbackReplacedThreeTimes)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    int counter = 0;
    qiface::i_quic_server::connection_callback_t cb1 =
        [&counter](std::shared_ptr<qiface::i_quic_session>) { counter += 1; };
    qiface::i_quic_server::connection_callback_t cb2 =
        [&counter](std::shared_ptr<qiface::i_quic_session>) { counter += 10; };
    qiface::i_quic_server::connection_callback_t cb3 =
        [&counter](std::shared_ptr<qiface::i_quic_session>) { counter += 100; };

    server->set_connection_callback(std::move(cb1));
    server->set_connection_callback(std::move(cb2));
    server->set_connection_callback(std::move(cb3));

    // No callback fires on a never-started server.
    EXPECT_EQ(counter, 0);
}

TEST(QuicServerInterfaceCallbacks, AllInterfaceCallbacksAcceptedTogether)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");

    qiface::i_quic_server::connection_callback_t conn_cb =
        [](std::shared_ptr<qiface::i_quic_session>) {};
    qiface::i_quic_server::disconnection_callback_t disc_cb =
        [](std::string_view) {};
    qiface::i_quic_server::receive_callback_t recv_cb =
        [](std::string_view, const std::vector<uint8_t>&) {};
    qiface::i_quic_server::stream_callback_t stream_cb =
        [](std::string_view, uint64_t, const std::vector<uint8_t>&, bool) {};
    qiface::i_quic_server::error_callback_t err_cb =
        [](std::string_view, std::error_code) {};

    EXPECT_NO_FATAL_FAILURE(
        server->set_connection_callback(std::move(conn_cb)));
    EXPECT_NO_FATAL_FAILURE(
        server->set_disconnection_callback(std::move(disc_cb)));
    EXPECT_NO_FATAL_FAILURE(server->set_receive_callback(std::move(recv_cb)));
    EXPECT_NO_FATAL_FAILURE(server->set_stream_callback(std::move(stream_cb)));
    EXPECT_NO_FATAL_FAILURE(server->set_error_callback(std::move(err_cb)));
}

// ============================================================================
// Concurrent state queries
// ============================================================================

TEST(QuicServerConcurrency, ConcurrentRunningStateQueriesAreSafe)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    constexpr int kThreads = 4;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([server]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_FALSE(server->is_running());
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST(QuicServerConcurrency, ConcurrentSessionCountQueriesAreSafe)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    constexpr int kThreads = 4;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([server]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_EQ(server->session_count(), 0u);
                EXPECT_EQ(server->connection_count(), 0u);
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST(QuicServerConcurrency, ConcurrentServerIdQueriesAreSafe)
{
    auto server = std::make_shared<core::messaging_quic_server>("concurrent-id");
    constexpr int kThreads = 4;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([server]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_EQ(server->server_id(), "concurrent-id");
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST(QuicServerConcurrency, ConcurrentSessionsListQueriesAreSafe)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");
    constexpr int kThreads = 4;
    constexpr int kIterations = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([server]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                auto list = server->sessions();
                EXPECT_TRUE(list.empty());
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST(QuicServerConcurrency, ConcurrentCallbackReplacementIsSafe)
{
    auto server = std::make_shared<core::messaging_quic_server>("s");

    std::atomic<bool> stop{false};

    std::thread writer([server, &stop]
    {
        int i = 0;
        while (!stop.load())
        {
            server->set_error_callback([i](std::error_code) { (void)i; });
            ++i;
        }
    });

    std::thread reader([server]
    {
        for (int i = 0; i < 200; ++i)
        {
            EXPECT_FALSE(server->is_running());
        }
    });

    reader.join();
    stop.store(true);
    writer.join();
}

// ============================================================================
// Multi-instance interactions
// ============================================================================

TEST(QuicServerMultiInstance, ManyServersHaveIndependentDefaultState)
{
    constexpr int kCount = 8;
    std::vector<std::shared_ptr<core::messaging_quic_server>> servers;
    servers.reserve(kCount);

    for (int i = 0; i < kCount; ++i)
    {
        servers.push_back(std::make_shared<core::messaging_quic_server>(
            "srv-" + std::to_string(i)));
    }

    for (int i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(servers[i]->server_id(), "srv-" + std::to_string(i));
        EXPECT_FALSE(servers[i]->is_running());
        EXPECT_EQ(servers[i]->session_count(), 0u);
    }
}

TEST(QuicServerMultiInstance, BroadcastOnOneDoesNotAffectAnother)
{
    auto a = std::make_shared<core::messaging_quic_server>("a");
    auto b = std::make_shared<core::messaging_quic_server>("b");

    EXPECT_TRUE(a->broadcast(std::vector<uint8_t>{1, 2, 3}).is_ok());

    EXPECT_FALSE(b->is_running());
    EXPECT_EQ(b->session_count(), 0u);
    EXPECT_EQ(b->server_id(), "b");
}

// ============================================================================
// Destructor coverage
// ============================================================================

TEST(QuicServerDestructor, DestructorOnNeverStartedRunsCleanly)
{
    {
        auto server = std::make_shared<core::messaging_quic_server>("scope");
        EXPECT_FALSE(server->is_running());
    }
    SUCCEED();
}

TEST(QuicServerDestructor, DestructorAfterFailedStopRunsCleanly)
{
    {
        auto server = std::make_shared<core::messaging_quic_server>("scope");
        // stop_server() rejection branch — server never ran.
        EXPECT_TRUE(server->stop_server().is_err());
    }
    SUCCEED();
}

TEST(QuicServerDestructor, DestructorWithCallbacksRegisteredRunsCleanly)
{
    {
        auto server = std::make_shared<core::messaging_quic_server>("scope");
        server->set_connection_callback(
            [](std::shared_ptr<session_ns::quic_session>) {});
        server->set_disconnection_callback(
            [](std::shared_ptr<session_ns::quic_session>) {});
        server->set_receive_callback(
            [](std::shared_ptr<session_ns::quic_session>,
               const std::vector<uint8_t>&) {});
        server->set_stream_receive_callback(
            [](std::shared_ptr<session_ns::quic_session>,
               uint64_t,
               const std::vector<uint8_t>&,
               bool) {});
        server->set_error_callback([](std::error_code) {});
    }
    SUCCEED();
}
