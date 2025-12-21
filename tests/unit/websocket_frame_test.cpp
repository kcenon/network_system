/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include "kcenon/network/internal/websocket_frame.h"

using namespace kcenon::network::internal;

/**
 * @file websocket_frame_test.cpp
 * @brief Unit tests for WebSocket frame encoding and decoding
 *
 * Tests validate:
 * - Frame encoding with different opcodes
 * - Frame decoding (header and payload)
 * - Masking and unmasking
 * - Various payload sizes (0, 125, 126, 127, 65536 bytes)
 * - Edge cases and invalid frames
 */

// ============================================================================
// Opcode Enum Tests
// ============================================================================

TEST(WebSocketOpcodeTest, EnumValues)
{
	EXPECT_EQ(static_cast<uint8_t>(ws_opcode::continuation), 0x0);
	EXPECT_EQ(static_cast<uint8_t>(ws_opcode::text), 0x1);
	EXPECT_EQ(static_cast<uint8_t>(ws_opcode::binary), 0x2);
	EXPECT_EQ(static_cast<uint8_t>(ws_opcode::close), 0x8);
	EXPECT_EQ(static_cast<uint8_t>(ws_opcode::ping), 0x9);
	EXPECT_EQ(static_cast<uint8_t>(ws_opcode::pong), 0xA);
}

// ============================================================================
// Frame Encoding Tests
// ============================================================================

TEST(WebSocketFrameTest, EncodeTextFrameSmall)
{
	std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o'};
	auto frame = websocket_frame::encode_frame(ws_opcode::text, std::move(payload), true, false);

	// Header: FIN=1, opcode=1, mask=0, len=5
	EXPECT_EQ(frame.size(), 7); // 2 bytes header + 5 bytes payload
	EXPECT_EQ(frame[0], 0x81);  // FIN | TEXT
	EXPECT_EQ(frame[1], 0x05);  // No mask, length=5
}

TEST(WebSocketFrameTest, EncodeBinaryFrameSmall)
{
	std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
	auto frame = websocket_frame::encode_frame(ws_opcode::binary, std::move(payload), true, false);

	EXPECT_EQ(frame.size(), 5); // 2 bytes header + 3 bytes payload
	EXPECT_EQ(frame[0], 0x82);  // FIN | BINARY
	EXPECT_EQ(frame[1], 0x03);  // No mask, length=3
}

TEST(WebSocketFrameTest, EncodeFrameWithMask)
{
	std::vector<uint8_t> payload = {'t', 'e', 's', 't'};
	auto original_payload = payload;
	auto frame = websocket_frame::encode_frame(ws_opcode::text, std::move(payload), true, true);

	// Header: 2 bytes + masking key (4 bytes) + masked payload (4 bytes)
	EXPECT_EQ(frame.size(), 10);
	EXPECT_EQ(frame[0], 0x81); // FIN | TEXT
	EXPECT_EQ(frame[1], 0x84); // MASK | length=4

	// Verify masking key is present
	std::array<uint8_t, 4> mask = {frame[2], frame[3], frame[4], frame[5]};

	// Verify payload is masked
	std::vector<uint8_t> masked_payload(frame.begin() + 6, frame.end());
	websocket_frame::apply_mask(masked_payload, mask);
	EXPECT_EQ(masked_payload, original_payload);
}

TEST(WebSocketFrameTest, EncodeFrameMediumPayload)
{
	// Payload length = 126 requires 16-bit extended length
	std::vector<uint8_t> payload(126, 0xAB);
	auto frame = websocket_frame::encode_frame(ws_opcode::binary, std::move(payload), true, false);

	// Header: 2 bytes + 2 bytes extended length + 126 bytes payload
	EXPECT_EQ(frame.size(), 130);
	EXPECT_EQ(frame[0], 0x82);   // FIN | BINARY
	EXPECT_EQ(frame[1], 126);    // Extended 16-bit length indicator
	EXPECT_EQ(frame[2], 0x00);   // High byte of length
	EXPECT_EQ(frame[3], 126);    // Low byte of length
}

TEST(WebSocketFrameTest, EncodeLargePayload)
{
	// Payload length = 65536 requires 64-bit extended length
	std::vector<uint8_t> payload(65536, 0xCD);
	auto frame = websocket_frame::encode_frame(ws_opcode::binary, std::move(payload), true, false);

	// Header: 2 bytes + 8 bytes extended length + 65536 bytes payload
	EXPECT_EQ(frame.size(), 65546);
	EXPECT_EQ(frame[0], 0x82); // FIN | BINARY
	EXPECT_EQ(frame[1], 127);  // Extended 64-bit length indicator

	// Verify 64-bit length encoding (big-endian)
	EXPECT_EQ(frame[2], 0x00);
	EXPECT_EQ(frame[3], 0x00);
	EXPECT_EQ(frame[4], 0x00);
	EXPECT_EQ(frame[5], 0x00);
	EXPECT_EQ(frame[6], 0x00);
	EXPECT_EQ(frame[7], 0x01);
	EXPECT_EQ(frame[8], 0x00);
	EXPECT_EQ(frame[9], 0x00);
}

TEST(WebSocketFrameTest, EncodeEmptyPayload)
{
	std::vector<uint8_t> payload;
	auto frame = websocket_frame::encode_frame(ws_opcode::ping, std::move(payload), true, false);

	EXPECT_EQ(frame.size(), 2); // Only header
	EXPECT_EQ(frame[0], 0x89);  // FIN | PING
	EXPECT_EQ(frame[1], 0x00);  // No mask, length=0
}

TEST(WebSocketFrameTest, EncodeFragmentedFrame)
{
	std::vector<uint8_t> payload = {'f', 'r', 'a', 'g'};
	auto frame = websocket_frame::encode_frame(ws_opcode::text, std::move(payload), false, false);

	EXPECT_EQ(frame[0], 0x01); // No FIN, TEXT opcode
}

// ============================================================================
// Frame Decoding Tests
// ============================================================================

TEST(WebSocketFrameTest, DecodeHeaderSimple)
{
	std::vector<uint8_t> frame = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};
	auto header = websocket_frame::decode_header(frame);

	ASSERT_TRUE(header.has_value());
	EXPECT_TRUE(header->fin);
	EXPECT_FALSE(header->rsv1);
	EXPECT_FALSE(header->rsv2);
	EXPECT_FALSE(header->rsv3);
	EXPECT_EQ(header->opcode, ws_opcode::text);
	EXPECT_FALSE(header->mask);
	EXPECT_EQ(header->payload_len, 5);
}

TEST(WebSocketFrameTest, DecodeHeaderWithMask)
{
	std::vector<uint8_t> frame = {0x81, 0x84, 0x12, 0x34, 0x56, 0x78, 't', 'e', 's', 't'};
	auto header = websocket_frame::decode_header(frame);

	ASSERT_TRUE(header.has_value());
	EXPECT_TRUE(header->mask);
	EXPECT_EQ(header->payload_len, 4);
	EXPECT_EQ(header->masking_key[0], 0x12);
	EXPECT_EQ(header->masking_key[1], 0x34);
	EXPECT_EQ(header->masking_key[2], 0x56);
	EXPECT_EQ(header->masking_key[3], 0x78);
}

TEST(WebSocketFrameTest, DecodeHeader16BitLength)
{
	std::vector<uint8_t> frame = {0x82, 126, 0x00, 126};
	frame.resize(130, 0xAB); // Add payload
	auto header = websocket_frame::decode_header(frame);

	ASSERT_TRUE(header.has_value());
	EXPECT_EQ(header->opcode, ws_opcode::binary);
	EXPECT_EQ(header->payload_len, 126);
}

TEST(WebSocketFrameTest, DecodeHeader64BitLength)
{
	std::vector<uint8_t> frame = {0x82, 127, 0x00, 0x00, 0x00, 0x00,
								  0x00, 0x01, 0x00, 0x00};
	frame.resize(65546, 0xCD); // Add payload
	auto header = websocket_frame::decode_header(frame);

	ASSERT_TRUE(header.has_value());
	EXPECT_EQ(header->opcode, ws_opcode::binary);
	EXPECT_EQ(header->payload_len, 65536);
}

TEST(WebSocketFrameTest, DecodeHeaderTruncated)
{
	std::vector<uint8_t> frame = {0x81}; // Only 1 byte
	auto header = websocket_frame::decode_header(frame);

	EXPECT_FALSE(header.has_value());
}

TEST(WebSocketFrameTest, DecodeHeaderTruncatedExtendedLength)
{
	std::vector<uint8_t> frame = {0x82, 126, 0x00}; // Missing 1 byte of length
	auto header = websocket_frame::decode_header(frame);

	EXPECT_FALSE(header.has_value());
}

TEST(WebSocketFrameTest, DecodePayloadSimple)
{
	std::vector<uint8_t> frame = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};
	auto header = websocket_frame::decode_header(frame);
	ASSERT_TRUE(header.has_value());

	auto payload = websocket_frame::decode_payload(*header, frame);
	EXPECT_EQ(payload.size(), 5);
	EXPECT_EQ(payload, std::vector<uint8_t>({'H', 'e', 'l', 'l', 'o'}));
}

TEST(WebSocketFrameTest, DecodePayloadWithMask)
{
	// Create masked frame
	std::vector<uint8_t> original_payload = {'t', 'e', 's', 't'};
	std::array<uint8_t, 4> mask = {0x12, 0x34, 0x56, 0x78};

	std::vector<uint8_t> masked_payload = original_payload;
	websocket_frame::apply_mask(masked_payload, mask);

	std::vector<uint8_t> frame = {0x81, 0x84, 0x12, 0x34, 0x56, 0x78};
	frame.insert(frame.end(), masked_payload.begin(), masked_payload.end());

	auto header = websocket_frame::decode_header(frame);
	ASSERT_TRUE(header.has_value());

	auto payload = websocket_frame::decode_payload(*header, frame);
	EXPECT_EQ(payload, original_payload);
}

// ============================================================================
// Masking Tests
// ============================================================================

TEST(WebSocketFrameTest, ApplyMaskSymmetric)
{
	std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
	std::array<uint8_t, 4> mask = {0x12, 0x34, 0x56, 0x78};
	auto original = data;

	websocket_frame::apply_mask(data, mask);
	EXPECT_NE(data, original); // Data should be masked

	websocket_frame::apply_mask(data, mask);
	EXPECT_EQ(data, original); // Applying mask twice restores original
}

TEST(WebSocketFrameTest, GenerateMaskRandomness)
{
	auto mask1 = websocket_frame::generate_mask();
	auto mask2 = websocket_frame::generate_mask();

	// Masks should be different (with very high probability)
	EXPECT_NE(mask1, mask2);
}

// ============================================================================
// Control Frame Tests
// ============================================================================

TEST(WebSocketFrameTest, EncodePingFrame)
{
	std::vector<uint8_t> payload = {'p', 'i', 'n', 'g'};
	auto frame = websocket_frame::encode_frame(ws_opcode::ping, std::move(payload), true, false);

	EXPECT_EQ(frame[0], 0x89); // FIN | PING
}

TEST(WebSocketFrameTest, EncodePongFrame)
{
	std::vector<uint8_t> payload = {'p', 'o', 'n', 'g'};
	auto frame = websocket_frame::encode_frame(ws_opcode::pong, std::move(payload), true, false);

	EXPECT_EQ(frame[0], 0x8A); // FIN | PONG
}

TEST(WebSocketFrameTest, EncodeCloseFrame)
{
	std::vector<uint8_t> payload = {0x03, 0xE8}; // Status code 1000
	auto frame = websocket_frame::encode_frame(ws_opcode::close, std::move(payload), true, false);

	EXPECT_EQ(frame[0], 0x88); // FIN | CLOSE
}

// ============================================================================
// Header Size Calculation Tests
// ============================================================================

TEST(WebSocketFrameTest, CalculateHeaderSizeSmallPayload)
{
	EXPECT_EQ(websocket_frame::calculate_header_size(0, false), 2);
	EXPECT_EQ(websocket_frame::calculate_header_size(125, false), 2);
	EXPECT_EQ(websocket_frame::calculate_header_size(0, true), 6);
	EXPECT_EQ(websocket_frame::calculate_header_size(125, true), 6);
}

TEST(WebSocketFrameTest, CalculateHeaderSizeMediumPayload)
{
	EXPECT_EQ(websocket_frame::calculate_header_size(126, false), 4);
	EXPECT_EQ(websocket_frame::calculate_header_size(65535, false), 4);
	EXPECT_EQ(websocket_frame::calculate_header_size(126, true), 8);
	EXPECT_EQ(websocket_frame::calculate_header_size(65535, true), 8);
}

TEST(WebSocketFrameTest, CalculateHeaderSizeLargePayload)
{
	EXPECT_EQ(websocket_frame::calculate_header_size(65536, false), 10);
	EXPECT_EQ(websocket_frame::calculate_header_size(1000000, false), 10);
	EXPECT_EQ(websocket_frame::calculate_header_size(65536, true), 14);
	EXPECT_EQ(websocket_frame::calculate_header_size(1000000, true), 14);
}
