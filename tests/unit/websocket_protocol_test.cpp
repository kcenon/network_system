/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include "kcenon/network/internal/websocket_protocol.h"

using namespace network_system::internal;

/**
 * @file websocket_protocol_test.cpp
 * @brief Unit tests for WebSocket protocol message handling
 *
 * Tests validate:
 * - Message creation (text, binary)
 * - Control frame creation (ping, pong, close)
 * - Message reception and callbacks
 * - Fragmentation and reassembly
 * - UTF-8 validation
 * - Control frame handling
 * - Close code parsing
 */

// ============================================================================
// Message Type Tests
// ============================================================================

TEST(WebSocketMessageTypeTest, EnumValues)
{
	EXPECT_EQ(static_cast<int>(ws_message_type::text), 0);
	EXPECT_EQ(static_cast<int>(ws_message_type::binary), 1);
}

// ============================================================================
// Close Code Tests
// ============================================================================

TEST(WebSocketCloseCodeTest, EnumValues)
{
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::normal), 1000);
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::going_away), 1001);
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::protocol_error), 1002);
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::unsupported_data), 1003);
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::invalid_frame), 1007);
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::policy_violation), 1008);
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::message_too_big), 1009);
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::internal_error), 1011);
}

// ============================================================================
// ws_message Tests
// ============================================================================

TEST(WebSocketMessageTest, TextConversion)
{
	ws_message msg;
	msg.type = ws_message_type::text;
	msg.data = {'H', 'e', 'l', 'l', 'o'};

	EXPECT_EQ(msg.as_text(), "Hello");
}

TEST(WebSocketMessageTest, BinaryAccess)
{
	ws_message msg;
	msg.type = ws_message_type::binary;
	msg.data = {0x01, 0x02, 0x03};

	const auto& binary = msg.as_binary();
	EXPECT_EQ(binary.size(), 3);
	EXPECT_EQ(binary[0], 0x01);
	EXPECT_EQ(binary[1], 0x02);
	EXPECT_EQ(binary[2], 0x03);
}

// ============================================================================
// Protocol Construction Tests
// ============================================================================

TEST(WebSocketProtocolTest, ConstructClient)
{
	websocket_protocol protocol(true);
	// Client protocol should apply masking
	auto frame = protocol.create_text_message("test");
	EXPECT_FALSE(frame.empty());
}

TEST(WebSocketProtocolTest, ConstructServer)
{
	websocket_protocol protocol(false);
	// Server protocol should not apply masking
	auto frame = protocol.create_text_message("test");
	EXPECT_FALSE(frame.empty());
}

// ============================================================================
// Text Message Creation Tests
// ============================================================================

TEST(WebSocketProtocolTest, CreateTextMessageBasic)
{
	websocket_protocol protocol(false);
	auto frame = protocol.create_text_message("Hello");

	EXPECT_FALSE(frame.empty());
	// Frame should start with FIN | TEXT opcode (0x81)
	EXPECT_EQ(frame[0], 0x81);
}

TEST(WebSocketProtocolTest, CreateTextMessageEmpty)
{
	websocket_protocol protocol(false);
	auto frame = protocol.create_text_message("");

	EXPECT_FALSE(frame.empty());
	// Empty message is valid
	EXPECT_EQ(frame[0], 0x81);
	EXPECT_EQ(frame[1], 0x00); // Length = 0
}

TEST(WebSocketProtocolTest, CreateTextMessageUTF8)
{
	websocket_protocol protocol(false);
	auto frame = protocol.create_text_message("Hello 世界");

	EXPECT_FALSE(frame.empty());
	EXPECT_EQ(frame[0], 0x81);
}

// ============================================================================
// Binary Message Creation Tests
// ============================================================================

TEST(WebSocketProtocolTest, CreateBinaryMessageBasic)
{
	websocket_protocol protocol(false);
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	auto frame = protocol.create_binary_message(std::move(data));

	EXPECT_FALSE(frame.empty());
	// Frame should start with FIN | BINARY opcode (0x82)
	EXPECT_EQ(frame[0], 0x82);
}

TEST(WebSocketProtocolTest, CreateBinaryMessageEmpty)
{
	websocket_protocol protocol(false);
	std::vector<uint8_t> data;
	auto frame = protocol.create_binary_message(std::move(data));

	EXPECT_FALSE(frame.empty());
	EXPECT_EQ(frame[0], 0x82);
	EXPECT_EQ(frame[1], 0x00); // Length = 0
}

// ============================================================================
// Control Frame Creation Tests
// ============================================================================

TEST(WebSocketProtocolTest, CreatePing)
{
	websocket_protocol protocol(false);
	auto frame = protocol.create_ping({});

	EXPECT_FALSE(frame.empty());
	// Frame should start with FIN | PING opcode (0x89)
	EXPECT_EQ(frame[0], 0x89);
}

TEST(WebSocketProtocolTest, CreatePingWithPayload)
{
	websocket_protocol protocol(false);
	std::vector<uint8_t> payload = {'p', 'i', 'n', 'g'};
	auto frame = protocol.create_ping(std::move(payload));

	EXPECT_FALSE(frame.empty());
	EXPECT_EQ(frame[0], 0x89);
}

TEST(WebSocketProtocolTest, CreatePong)
{
	websocket_protocol protocol(false);
	auto frame = protocol.create_pong({});

	EXPECT_FALSE(frame.empty());
	// Frame should start with FIN | PONG opcode (0x8A)
	EXPECT_EQ(frame[0], 0x8A);
}

TEST(WebSocketProtocolTest, CreatePongWithPayload)
{
	websocket_protocol protocol(false);
	std::vector<uint8_t> payload = {'p', 'o', 'n', 'g'};
	auto frame = protocol.create_pong(std::move(payload));

	EXPECT_FALSE(frame.empty());
	EXPECT_EQ(frame[0], 0x8A);
}

TEST(WebSocketProtocolTest, CreateClose)
{
	websocket_protocol protocol(false);
	auto frame = protocol.create_close(ws_close_code::normal, "");

	EXPECT_FALSE(frame.empty());
	// Frame should start with FIN | CLOSE opcode (0x88)
	EXPECT_EQ(frame[0], 0x88);
}

TEST(WebSocketProtocolTest, CreateCloseWithReason)
{
	websocket_protocol protocol(false);
	auto frame = protocol.create_close(ws_close_code::going_away, "Bye");

	EXPECT_FALSE(frame.empty());
	EXPECT_EQ(frame[0], 0x88);
	// Payload should contain close code and reason
	EXPECT_GT(frame.size(), 4); // Header + code + reason
}

// ============================================================================
// Message Reception Tests
// ============================================================================

TEST(WebSocketProtocolTest, ReceiveTextMessage)
{
	websocket_protocol protocol(false);

	// Setup callback
	bool callback_invoked = false;
	ws_message received_msg;
	protocol.set_message_callback([&](const ws_message& msg) {
		callback_invoked = true;
		received_msg = msg;
	});

	// Create a text frame
	auto frame = websocket_frame::encode_frame(
		ws_opcode::text,
		std::vector<uint8_t>{'H', 'e', 'l', 'l', 'o'},
		true,
		false
	);

	// Process frame
	protocol.process_data(frame);

	EXPECT_TRUE(callback_invoked);
	EXPECT_EQ(received_msg.type, ws_message_type::text);
	EXPECT_EQ(received_msg.as_text(), "Hello");
}

TEST(WebSocketProtocolTest, ReceiveBinaryMessage)
{
	websocket_protocol protocol(false);

	bool callback_invoked = false;
	ws_message received_msg;
	protocol.set_message_callback([&](const ws_message& msg) {
		callback_invoked = true;
		received_msg = msg;
	});

	// Create a binary frame
	auto frame = websocket_frame::encode_frame(
		ws_opcode::binary,
		std::vector<uint8_t>{0x01, 0x02, 0x03},
		true,
		false
	);

	protocol.process_data(frame);

	EXPECT_TRUE(callback_invoked);
	EXPECT_EQ(received_msg.type, ws_message_type::binary);
	EXPECT_EQ(received_msg.as_binary().size(), 3);
}

// ============================================================================
// Fragmentation Tests
// ============================================================================

TEST(WebSocketProtocolTest, ReceiveFragmentedTextMessage)
{
	websocket_protocol protocol(false);

	bool callback_invoked = false;
	ws_message received_msg;
	protocol.set_message_callback([&](const ws_message& msg) {
		callback_invoked = true;
		received_msg = msg;
	});

	// Create first fragment (FIN=false)
	auto frame1 = websocket_frame::encode_frame(
		ws_opcode::text,
		std::vector<uint8_t>{'H', 'e', 'l'},
		false, // Not final
		false
	);

	// Create continuation fragment (FIN=true)
	auto frame2 = websocket_frame::encode_frame(
		ws_opcode::continuation,
		std::vector<uint8_t>{'l', 'o'},
		true, // Final
		false
	);

	// Process fragments
	protocol.process_data(frame1);
	EXPECT_FALSE(callback_invoked); // Not complete yet

	protocol.process_data(frame2);
	EXPECT_TRUE(callback_invoked); // Now complete

	EXPECT_EQ(received_msg.type, ws_message_type::text);
	EXPECT_EQ(received_msg.as_text(), "Hello");
}

TEST(WebSocketProtocolTest, ReceiveFragmentedBinaryMessage)
{
	websocket_protocol protocol(false);

	bool callback_invoked = false;
	ws_message received_msg;
	protocol.set_message_callback([&](const ws_message& msg) {
		callback_invoked = true;
		received_msg = msg;
	});

	// Create first fragment
	auto frame1 = websocket_frame::encode_frame(
		ws_opcode::binary,
		std::vector<uint8_t>{0x01, 0x02},
		false,
		false
	);

	// Create continuation fragment
	auto frame2 = websocket_frame::encode_frame(
		ws_opcode::continuation,
		std::vector<uint8_t>{0x03, 0x04},
		true,
		false
	);

	protocol.process_data(frame1);
	EXPECT_FALSE(callback_invoked);

	protocol.process_data(frame2);
	EXPECT_TRUE(callback_invoked);

	EXPECT_EQ(received_msg.type, ws_message_type::binary);
	EXPECT_EQ(received_msg.as_binary().size(), 4);
	EXPECT_EQ(received_msg.as_binary()[0], 0x01);
	EXPECT_EQ(received_msg.as_binary()[3], 0x04);
}

TEST(WebSocketProtocolTest, ReceiveMultipleFragments)
{
	websocket_protocol protocol(false);

	bool callback_invoked = false;
	ws_message received_msg;
	protocol.set_message_callback([&](const ws_message& msg) {
		callback_invoked = true;
		received_msg = msg;
	});

	// Create three fragments
	auto frame1 = websocket_frame::encode_frame(
		ws_opcode::text,
		std::vector<uint8_t>{'A'},
		false,
		false
	);

	auto frame2 = websocket_frame::encode_frame(
		ws_opcode::continuation,
		std::vector<uint8_t>{'B'},
		false,
		false
	);

	auto frame3 = websocket_frame::encode_frame(
		ws_opcode::continuation,
		std::vector<uint8_t>{'C'},
		true,
		false
	);

	protocol.process_data(frame1);
	EXPECT_FALSE(callback_invoked);

	protocol.process_data(frame2);
	EXPECT_FALSE(callback_invoked);

	protocol.process_data(frame3);
	EXPECT_TRUE(callback_invoked);

	EXPECT_EQ(received_msg.as_text(), "ABC");
}

// ============================================================================
// Control Frame Handling Tests
// ============================================================================

TEST(WebSocketProtocolTest, ReceivePing)
{
	websocket_protocol protocol(false);

	bool callback_invoked = false;
	std::vector<uint8_t> received_payload;
	protocol.set_ping_callback([&](const std::vector<uint8_t>& payload) {
		callback_invoked = true;
		received_payload = payload;
	});

	auto frame = websocket_frame::encode_frame(
		ws_opcode::ping,
		std::vector<uint8_t>{'p', 'i', 'n', 'g'},
		true,
		false
	);

	protocol.process_data(frame);

	EXPECT_TRUE(callback_invoked);
	EXPECT_EQ(received_payload.size(), 4);
}

TEST(WebSocketProtocolTest, ReceivePong)
{
	websocket_protocol protocol(false);

	bool callback_invoked = false;
	std::vector<uint8_t> received_payload;
	protocol.set_pong_callback([&](const std::vector<uint8_t>& payload) {
		callback_invoked = true;
		received_payload = payload;
	});

	auto frame = websocket_frame::encode_frame(
		ws_opcode::pong,
		std::vector<uint8_t>{'p', 'o', 'n', 'g'},
		true,
		false
	);

	protocol.process_data(frame);

	EXPECT_TRUE(callback_invoked);
	EXPECT_EQ(received_payload.size(), 4);
}

TEST(WebSocketProtocolTest, ReceiveClose)
{
	websocket_protocol protocol(false);

	bool callback_invoked = false;
	ws_close_code received_code;
	std::string received_reason;
	protocol.set_close_callback([&](ws_close_code code, const std::string& reason) {
		callback_invoked = true;
		received_code = code;
		received_reason = reason;
	});

	// Create close payload with code 1000
	std::vector<uint8_t> payload = {0x03, 0xE8}; // 1000 in network byte order
	auto frame = websocket_frame::encode_frame(
		ws_opcode::close,
		std::move(payload),
		true,
		false
	);

	protocol.process_data(frame);

	EXPECT_TRUE(callback_invoked);
	EXPECT_EQ(static_cast<uint16_t>(received_code), 1000);
	EXPECT_TRUE(received_reason.empty());
}

TEST(WebSocketProtocolTest, ReceiveCloseWithReason)
{
	websocket_protocol protocol(false);

	bool callback_invoked = false;
	ws_close_code received_code;
	std::string received_reason;
	protocol.set_close_callback([&](ws_close_code code, const std::string& reason) {
		callback_invoked = true;
		received_code = code;
		received_reason = reason;
	});

	// Create close payload with code 1001 and reason "Bye"
	std::vector<uint8_t> payload = {0x03, 0xE9, 'B', 'y', 'e'};
	auto frame = websocket_frame::encode_frame(
		ws_opcode::close,
		std::move(payload),
		true,
		false
	);

	protocol.process_data(frame);

	EXPECT_TRUE(callback_invoked);
	EXPECT_EQ(static_cast<uint16_t>(received_code), 1001);
	EXPECT_EQ(received_reason, "Bye");
}

// ============================================================================
// Multiple Message Tests
// ============================================================================

TEST(WebSocketProtocolTest, ReceiveMultipleMessages)
{
	websocket_protocol protocol(false);

	int callback_count = 0;
	protocol.set_message_callback([&](const ws_message& msg) {
		callback_count++;
	});

	// Send three separate messages
	auto frame1 = websocket_frame::encode_frame(
		ws_opcode::text, std::vector<uint8_t>{'A'}, true, false);
	auto frame2 = websocket_frame::encode_frame(
		ws_opcode::text, std::vector<uint8_t>{'B'}, true, false);
	auto frame3 = websocket_frame::encode_frame(
		ws_opcode::text, std::vector<uint8_t>{'C'}, true, false);

	protocol.process_data(frame1);
	protocol.process_data(frame2);
	protocol.process_data(frame3);

	EXPECT_EQ(callback_count, 3);
}

// ============================================================================
// Partial Frame Tests
// ============================================================================

TEST(WebSocketProtocolTest, ReceivePartialFrame)
{
	websocket_protocol protocol(false);

	bool callback_invoked = false;
	protocol.set_message_callback([&](const ws_message& msg) {
		callback_invoked = true;
	});

	auto frame = websocket_frame::encode_frame(
		ws_opcode::text, std::vector<uint8_t>{'H', 'e', 'l', 'l', 'o'}, true, false);

	// Send only first half of frame
	std::vector<uint8_t> partial(frame.begin(), frame.begin() + frame.size() / 2);
	protocol.process_data(partial);

	EXPECT_FALSE(callback_invoked); // Should not invoke callback yet

	// Send rest of frame
	std::vector<uint8_t> rest(frame.begin() + frame.size() / 2, frame.end());
	protocol.process_data(rest);

	EXPECT_TRUE(callback_invoked); // Now should invoke callback
}
