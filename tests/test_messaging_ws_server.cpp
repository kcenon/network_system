// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>

#include "internal/http/websocket_server.h"
#include "internal/websocket/websocket_protocol.h"
#include "kcenon/network/detail/utils/result_types.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace kcenon::network::core::test
{

class MessagingWsServerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};

// =============================================================================
// Basic Construction Tests
// =============================================================================

TEST_F(MessagingWsServerTest, BasicConstruction)
{
	auto server = std::make_shared<messaging_ws_server>("test_server");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), "test_server");
}

// =============================================================================
// Unified Pattern Type Alias Tests
// =============================================================================

TEST_F(MessagingWsServerTest, TypeAliasWsServer)
{
	// Verify ws_server is an alias for messaging_ws_server
	static_assert(std::is_same_v<ws_server, messaging_ws_server>,
	              "ws_server should be an alias for messaging_ws_server");

	auto server = std::make_shared<ws_server>("alias_test");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), "alias_test");
}

TEST_F(MessagingWsServerTest, TypeAliasSecureWsServer)
{
	// Verify secure_ws_server is an alias for messaging_ws_server
	// WSS (WebSocket Secure) uses TLS, but shares the same base implementation
	static_assert(std::is_same_v<secure_ws_server, messaging_ws_server>,
	              "secure_ws_server should be an alias for messaging_ws_server");
	static_assert(std::is_same_v<ws_server, secure_ws_server>,
	              "ws_server and secure_ws_server should be the same type");

	auto server = std::make_shared<secure_ws_server>("secure_alias_test");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), "secure_alias_test");
}

// =============================================================================
// Additional coverage tests for src/http/websocket_server.cpp (Issue #1067)
//
// Complements tests/unit/websocket_server_branch_test.cpp by exercising
// further public-API surfaces reachable without a live WebSocket peer:
//   - ws_message struct as_text() / as_binary() helpers used by
//     invoke_message_callback() routing
//   - ws_close_code enum value invariants (RFC 6455 Section 7.4) used by close()
//   - i_websocket_server::start() interface delegation (default state)
//   - Constructor variations (c-string, std::string, string_view, substring)
//   - Lookup edge cases on never-started server (empty / long / IPv4-style ids)
//   - Broadcast under registered callbacks does not invoke them when no peers
//   - Interface callback adapters: lambda capture is stored, not invoked
//   - Multi-instance state isolation across many sequential constructions
//   - wait_for_stop on never-started server returns immediately
//   - stop_server idempotency across many calls and via interface pointer
//
// All tests operate on the public API only and are hermetic (no real TCP
// peer or live io_context loop). The acceptance criteria of #1067
// (line >= 80% / branch >= 70%) are not reachable from these unit tests
// alone because they require a live WebSocket loopback fixture; this PR
// is therefore "Part of #1067" not "Closes #1067".
// =============================================================================

// -----------------------------------------------------------------------------
// ws_message struct invariants (used by on_message dispatch)
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, WsMessageTextAsTextRoundTrip)
{
	internal::ws_message msg;
	msg.type = internal::ws_message_type::text;
	const std::string payload = "hello websocket";
	msg.data.assign(payload.begin(), payload.end());

	EXPECT_EQ(msg.as_text(), payload);
}

TEST_F(MessagingWsServerTest, WsMessageEmptyTextAsTextIsEmpty)
{
	internal::ws_message msg;
	msg.type = internal::ws_message_type::text;
	msg.data.clear();

	EXPECT_TRUE(msg.as_text().empty());
}

TEST_F(MessagingWsServerTest, WsMessageBinaryAsBinaryRoundTrip)
{
	internal::ws_message msg;
	msg.type = internal::ws_message_type::binary;
	msg.data = {0x00, 0x01, 0x7f, 0x80, 0xff};

	const auto& bin = msg.as_binary();
	ASSERT_EQ(bin.size(), 5u);
	EXPECT_EQ(bin[0], 0x00);
	EXPECT_EQ(bin[1], 0x01);
	EXPECT_EQ(bin[2], 0x7f);
	EXPECT_EQ(bin[3], 0x80);
	EXPECT_EQ(bin[4], 0xff);
}

TEST_F(MessagingWsServerTest, WsMessageBinaryAsBinaryIsReference)
{
	internal::ws_message msg;
	msg.type = internal::ws_message_type::binary;
	msg.data = {0x10, 0x20};

	const auto& ref1 = msg.as_binary();
	const auto& ref2 = msg.as_binary();
	// Both references should refer to the same vector (no copy)
	EXPECT_EQ(&ref1, &ref2);
}

// -----------------------------------------------------------------------------
// ws_close_code enum invariants (RFC 6455 Section 7.4)
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, WsCloseCodeNormalIs1000)
{
	EXPECT_EQ(static_cast<uint16_t>(internal::ws_close_code::normal), 1000u);
}

TEST_F(MessagingWsServerTest, WsCloseCodeGoingAwayIs1001)
{
	EXPECT_EQ(static_cast<uint16_t>(internal::ws_close_code::going_away), 1001u);
}

TEST_F(MessagingWsServerTest, WsCloseCodeProtocolErrorIs1002)
{
	EXPECT_EQ(static_cast<uint16_t>(internal::ws_close_code::protocol_error), 1002u);
}

TEST_F(MessagingWsServerTest, WsCloseCodeUnsupportedDataIs1003)
{
	EXPECT_EQ(static_cast<uint16_t>(internal::ws_close_code::unsupported_data), 1003u);
}

TEST_F(MessagingWsServerTest, WsCloseCodeMessageTooBigIs1009)
{
	EXPECT_EQ(static_cast<uint16_t>(internal::ws_close_code::message_too_big), 1009u);
}

TEST_F(MessagingWsServerTest, WsCloseCodeInternalErrorIs1011)
{
	EXPECT_EQ(static_cast<uint16_t>(internal::ws_close_code::internal_error), 1011u);
}

// -----------------------------------------------------------------------------
// Constructor variations
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, ConstructFromCStringLiteral)
{
	auto server = std::make_shared<messaging_ws_server>("literal_id");
	EXPECT_EQ(server->server_id(), "literal_id");
}

TEST_F(MessagingWsServerTest, ConstructFromStdString)
{
	const std::string id = "std_string_id";
	auto server = std::make_shared<messaging_ws_server>(id);
	EXPECT_EQ(server->server_id(), id);
}

TEST_F(MessagingWsServerTest, ConstructFromStringView)
{
	const std::string_view id_view = "view_id";
	auto server = std::make_shared<messaging_ws_server>(id_view);
	EXPECT_EQ(server->server_id(), std::string(id_view));
}

TEST_F(MessagingWsServerTest, ConstructFromSubstringStringView)
{
	const std::string source = "prefix_actual_id_suffix";
	const std::string_view sub = std::string_view(source).substr(7, 9);
	auto server = std::make_shared<messaging_ws_server>(sub);
	EXPECT_EQ(server->server_id(), "actual_id");
}

TEST_F(MessagingWsServerTest, ServerIdReferenceStableAcrossManyCalls)
{
	auto server = std::make_shared<messaging_ws_server>("stable_id");
	const std::string& ref1 = server->server_id();
	for (int i = 0; i < 16; ++i)
	{
		const std::string& refN = server->server_id();
		EXPECT_EQ(&ref1, &refN);
	}
}

// -----------------------------------------------------------------------------
// Default-state queries via interface pointer
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, InterfacePointerConnectionCountIsZeroBeforeStart)
{
	auto server = std::make_shared<messaging_ws_server>("iface_count");
	std::shared_ptr<interfaces::i_websocket_server> as_iface = server;
	EXPECT_EQ(as_iface->connection_count(), 0u);
}

TEST_F(MessagingWsServerTest, InterfacePointerStopBeforeStartReturnsOk)
{
	auto server = std::make_shared<messaging_ws_server>("iface_stop");
	std::shared_ptr<interfaces::i_websocket_server> as_iface = server;
	auto result = as_iface->stop();
	EXPECT_TRUE(result.is_ok());
}

TEST_F(MessagingWsServerTest, InterfacePointerStopIdempotentAcrossManyCalls)
{
	auto server = std::make_shared<messaging_ws_server>("iface_stop_idem");
	std::shared_ptr<interfaces::i_websocket_server> as_iface = server;
	for (int i = 0; i < 5; ++i)
	{
		auto result = as_iface->stop();
		EXPECT_TRUE(result.is_ok());
	}
	EXPECT_EQ(as_iface->connection_count(), 0u);
}

// -----------------------------------------------------------------------------
// Lookup edge cases on never-started server
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, GetConnectionWithEmptyIdReturnsNullptr)
{
	auto server = std::make_shared<messaging_ws_server>("lookup_empty");
	EXPECT_EQ(server->get_connection(""), nullptr);
}

TEST_F(MessagingWsServerTest, GetConnectionWithLongIdReturnsNullptr)
{
	auto server = std::make_shared<messaging_ws_server>("lookup_long");
	const std::string long_id(2048, 'x');
	EXPECT_EQ(server->get_connection(long_id), nullptr);
}

TEST_F(MessagingWsServerTest, GetConnectionWithIpv4StyleIdReturnsNullptr)
{
	auto server = std::make_shared<messaging_ws_server>("lookup_ipv4");
	EXPECT_EQ(server->get_connection("192.168.1.1:54321"), nullptr);
}

TEST_F(MessagingWsServerTest, GetAllConnectionsIsEmptyAndStable)
{
	auto server = std::make_shared<messaging_ws_server>("lookup_all");
	auto first = server->get_all_connections();
	auto second = server->get_all_connections();
	EXPECT_TRUE(first.empty());
	EXPECT_TRUE(second.empty());
	EXPECT_EQ(first.size(), second.size());
}

// -----------------------------------------------------------------------------
// Broadcast on never-started server with callbacks registered
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, BroadcastTextWithRegisteredCallbacksIsHarmless)
{
	auto server = std::make_shared<messaging_ws_server>("bcast_with_cb");

	std::atomic<int> text_calls{0};
	server->set_text_callback(
		[&text_calls](std::string_view, const std::string&) { text_calls.fetch_add(1); });

	// No connections exist; broadcast should not invoke the registered callback
	server->broadcast_text("hello");
	EXPECT_EQ(text_calls.load(), 0);
}

TEST_F(MessagingWsServerTest, BroadcastBinaryWithRegisteredCallbacksIsHarmless)
{
	auto server = std::make_shared<messaging_ws_server>("bcast_bin_with_cb");

	std::atomic<int> bin_calls{0};
	server->set_binary_callback(
		[&bin_calls](std::string_view, const std::vector<uint8_t>&) { bin_calls.fetch_add(1); });

	const std::vector<uint8_t> payload = {0xde, 0xad, 0xbe, 0xef};
	server->broadcast_binary(payload);
	EXPECT_EQ(bin_calls.load(), 0);
}

// -----------------------------------------------------------------------------
// Interface callback adapters: lambda capture semantics
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, ConnectionCallbackLambdaCapturedNotInvokedImmediately)
{
	auto server = std::make_shared<messaging_ws_server>("cb_capture_conn");

	auto invoked = std::make_shared<std::atomic<int>>(0);
	server->set_connection_callback(
		[invoked](std::shared_ptr<interfaces::i_websocket_session>) {
			invoked->fetch_add(1);
		});

	// Setting the callback must not invoke it
	EXPECT_EQ(invoked->load(), 0);
}

TEST_F(MessagingWsServerTest, DisconnectionCallbackLambdaCapturedNotInvokedImmediately)
{
	auto server = std::make_shared<messaging_ws_server>("cb_capture_disc");

	auto invoked = std::make_shared<std::atomic<int>>(0);
	server->set_disconnection_callback(
		[invoked](std::string_view, uint16_t, std::string_view) {
			invoked->fetch_add(1);
		});

	EXPECT_EQ(invoked->load(), 0);
}

TEST_F(MessagingWsServerTest, ErrorCallbackLambdaCapturedNotInvokedImmediately)
{
	auto server = std::make_shared<messaging_ws_server>("cb_capture_err");

	auto invoked = std::make_shared<std::atomic<int>>(0);
	server->set_error_callback(
		[invoked](std::string_view, std::error_code) { invoked->fetch_add(1); });

	EXPECT_EQ(invoked->load(), 0);
}

TEST_F(MessagingWsServerTest, AllInterfaceCallbacksReplaceableWithNullptr)
{
	auto server = std::make_shared<messaging_ws_server>("cb_null_replace");

	// Populate first
	server->set_connection_callback(
		[](std::shared_ptr<interfaces::i_websocket_session>) {});
	server->set_disconnection_callback(
		[](std::string_view, uint16_t, std::string_view) {});
	server->set_text_callback([](std::string_view, const std::string&) {});
	server->set_binary_callback([](std::string_view, const std::vector<uint8_t>&) {});
	server->set_error_callback([](std::string_view, std::error_code) {});

	// Replace each with null callback (covers empty-function branch in adapter)
	server->set_connection_callback(interfaces::i_websocket_server::connection_callback_t{});
	server->set_disconnection_callback(interfaces::i_websocket_server::disconnection_callback_t{});
	server->set_text_callback(interfaces::i_websocket_server::text_callback_t{});
	server->set_binary_callback(interfaces::i_websocket_server::binary_callback_t{});
	server->set_error_callback(interfaces::i_websocket_server::error_callback_t{});

	// Server state remains hermetic
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->connection_count(), 0u);
}

// -----------------------------------------------------------------------------
// Multi-instance state isolation
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, ManySequentialConstructionsHaveUniqueIds)
{
	for (int i = 0; i < 32; ++i)
	{
		const std::string id = "seq_" + std::to_string(i);
		auto server = std::make_shared<messaging_ws_server>(id);
		EXPECT_EQ(server->server_id(), id);
		EXPECT_FALSE(server->is_running());
		EXPECT_EQ(server->connection_count(), 0u);
	}
}

TEST_F(MessagingWsServerTest, TwoServersWithSameIdAreIndependentInstances)
{
	auto a = std::make_shared<messaging_ws_server>("dup_id");
	auto b = std::make_shared<messaging_ws_server>("dup_id");

	EXPECT_NE(a.get(), b.get());
	EXPECT_EQ(a->server_id(), b->server_id());

	a->set_text_callback([](std::string_view, const std::string&) {});
	// Setting a callback on `a` must not affect `b`'s state
	EXPECT_FALSE(b->is_running());
	EXPECT_EQ(b->connection_count(), 0u);
}

// -----------------------------------------------------------------------------
// wait_for_stop on never-started server returns immediately
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, WaitForStopOnNeverStartedReturnsImmediately)
{
	auto server = std::make_shared<messaging_ws_server>("wait_never");
	const auto t0 = std::chrono::steady_clock::now();
	server->wait_for_stop();
	const auto elapsed = std::chrono::steady_clock::now() - t0;
	// Should return immediately (under 1 second is generous)
	EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 1);
}

// -----------------------------------------------------------------------------
// stop_server idempotency on never-started server
// -----------------------------------------------------------------------------

TEST_F(MessagingWsServerTest, StopServerCalledManyTimesAllReturnOk)
{
	auto server = std::make_shared<messaging_ws_server>("stop_many");
	for (int i = 0; i < 10; ++i)
	{
		auto result = server->stop_server();
		EXPECT_TRUE(result.is_ok());
	}
}

TEST_F(MessagingWsServerTest, StopServerThenStopInterfaceBothReturnOk)
{
	auto server = std::make_shared<messaging_ws_server>("stop_mixed");
	auto r1 = server->stop_server();
	EXPECT_TRUE(r1.is_ok());

	std::shared_ptr<interfaces::i_websocket_server> as_iface = server;
	auto r2 = as_iface->stop();
	EXPECT_TRUE(r2.is_ok());
}

} // namespace kcenon::network::core::test
