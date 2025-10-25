/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include "network_system/internal/websocket_handshake.h"

using namespace network_system::internal;

/**
 * @file websocket_handshake_test.cpp
 * @brief Unit tests for WebSocket HTTP/1.1 upgrade handshake
 *
 * Tests validate:
 * - Client handshake request generation
 * - Server handshake response generation
 * - Handshake validation
 * - Sec-WebSocket-Key generation
 * - Sec-WebSocket-Accept calculation
 * - Header parsing
 * - Error handling for invalid handshakes
 */

// ============================================================================
// WebSocket Key Generation Tests
// ============================================================================

TEST(WebSocketHandshakeTest, GenerateWebSocketKey)
{
	auto key1 = websocket_handshake::generate_websocket_key();
	auto key2 = websocket_handshake::generate_websocket_key();

	// Keys should be non-empty
	EXPECT_FALSE(key1.empty());
	EXPECT_FALSE(key2.empty());

	// Keys should be 24 characters (16 bytes Base64 encoded)
	EXPECT_EQ(key1.length(), 24);
	EXPECT_EQ(key2.length(), 24);

	// Keys should be different (with very high probability)
	EXPECT_NE(key1, key2);
}

// ============================================================================
// Accept Key Calculation Tests
// ============================================================================

TEST(WebSocketHandshakeTest, CalculateAcceptKey)
{
	// Test vector from RFC 6455
	const std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
	const std::string expected_accept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

	auto accept_key = websocket_handshake::calculate_accept_key(client_key);
	EXPECT_EQ(accept_key, expected_accept);
}

TEST(WebSocketHandshakeTest, CalculateAcceptKeyConsistent)
{
	const std::string client_key = "test-key-12345";

	auto accept1 = websocket_handshake::calculate_accept_key(client_key);
	auto accept2 = websocket_handshake::calculate_accept_key(client_key);

	// Same input should produce same output
	EXPECT_EQ(accept1, accept2);
}

// ============================================================================
// Client Handshake Request Tests
// ============================================================================

TEST(WebSocketHandshakeTest, CreateClientHandshakeBasic)
{
	auto request = websocket_handshake::create_client_handshake(
		"example.com", "/", 80);

	// Should contain required elements
	EXPECT_NE(request.find("GET / HTTP/1.1"), std::string::npos);
	EXPECT_NE(request.find("Host: example.com"), std::string::npos);
	EXPECT_NE(request.find("Upgrade: websocket"), std::string::npos);
	EXPECT_NE(request.find("Connection: Upgrade"), std::string::npos);
	EXPECT_NE(request.find("Sec-WebSocket-Key: "), std::string::npos);
	EXPECT_NE(request.find("Sec-WebSocket-Version: 13"), std::string::npos);
}

TEST(WebSocketHandshakeTest, CreateClientHandshakeCustomPath)
{
	auto request = websocket_handshake::create_client_handshake(
		"example.com", "/chat", 80);

	EXPECT_NE(request.find("GET /chat HTTP/1.1"), std::string::npos);
}

TEST(WebSocketHandshakeTest, CreateClientHandshakeNonStandardPort)
{
	auto request = websocket_handshake::create_client_handshake(
		"example.com", "/", 8080);

	// Non-standard port should be included in Host header
	EXPECT_NE(request.find("Host: example.com:8080"), std::string::npos);
}

TEST(WebSocketHandshakeTest, CreateClientHandshakeStandardPort80)
{
	auto request = websocket_handshake::create_client_handshake(
		"example.com", "/", 80);

	// Standard port 80 should not be included
	EXPECT_EQ(request.find("Host: example.com:80"), std::string::npos);
	EXPECT_NE(request.find("Host: example.com\r\n"), std::string::npos);
}

TEST(WebSocketHandshakeTest, CreateClientHandshakeStandardPort443)
{
	auto request = websocket_handshake::create_client_handshake(
		"example.com", "/", 443);

	// Standard port 443 should not be included
	EXPECT_EQ(request.find("Host: example.com:443"), std::string::npos);
	EXPECT_NE(request.find("Host: example.com\r\n"), std::string::npos);
}

TEST(WebSocketHandshakeTest, CreateClientHandshakeWithExtraHeaders)
{
	std::map<std::string, std::string> extra_headers = {
		{"Origin", "https://example.com"},
		{"User-Agent", "TestClient/1.0"}
	};

	auto request = websocket_handshake::create_client_handshake(
		"example.com", "/", 80, extra_headers);

	EXPECT_NE(request.find("Origin: https://example.com"), std::string::npos);
	EXPECT_NE(request.find("User-Agent: TestClient/1.0"), std::string::npos);
}

// ============================================================================
// Server Handshake Response Tests
// ============================================================================

TEST(WebSocketHandshakeTest, CreateServerResponse)
{
	const std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
	auto response = websocket_handshake::create_server_response(client_key);

	// Should contain required elements
	EXPECT_NE(response.find("HTTP/1.1 101 Switching Protocols"), std::string::npos);
	EXPECT_NE(response.find("Upgrade: websocket"), std::string::npos);
	EXPECT_NE(response.find("Connection: Upgrade"), std::string::npos);
	EXPECT_NE(response.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo="),
			  std::string::npos);
}

// ============================================================================
// Parse Client Request Tests
// ============================================================================

TEST(WebSocketHandshakeTest, ParseValidClientRequest)
{
	std::string request =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n";

	auto result = websocket_handshake::parse_client_request(request);

	EXPECT_TRUE(result.success);
	EXPECT_TRUE(result.error_message.empty());
	EXPECT_EQ(result.headers["upgrade"], "websocket");
	EXPECT_EQ(result.headers["connection"], "Upgrade");
	EXPECT_EQ(result.headers["sec-websocket-key"], "dGhlIHNhbXBsZSBub25jZQ==");
	EXPECT_EQ(result.headers["sec-websocket-version"], "13");
}

TEST(WebSocketHandshakeTest, ParseClientRequestMissingUpgrade)
{
	std::string request =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n";

	auto result = websocket_handshake::parse_client_request(request);

	EXPECT_FALSE(result.success);
	EXPECT_NE(result.error_message.find("Upgrade"), std::string::npos);
}

TEST(WebSocketHandshakeTest, ParseClientRequestMissingConnection)
{
	std::string request =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Upgrade: websocket\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n";

	auto result = websocket_handshake::parse_client_request(request);

	EXPECT_FALSE(result.success);
	EXPECT_NE(result.error_message.find("Connection"), std::string::npos);
}

TEST(WebSocketHandshakeTest, ParseClientRequestMissingKey)
{
	std::string request =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n";

	auto result = websocket_handshake::parse_client_request(request);

	EXPECT_FALSE(result.success);
	EXPECT_NE(result.error_message.find("Sec-WebSocket-Key"), std::string::npos);
}

TEST(WebSocketHandshakeTest, ParseClientRequestInvalidVersion)
{
	std::string request =
		"GET /chat HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 12\r\n"
		"\r\n";

	auto result = websocket_handshake::parse_client_request(request);

	EXPECT_FALSE(result.success);
	EXPECT_NE(result.error_message.find("Sec-WebSocket-Version"), std::string::npos);
}

TEST(WebSocketHandshakeTest, ParseClientRequestInvalidMethod)
{
	std::string request =
		"POST /chat HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n";

	auto result = websocket_handshake::parse_client_request(request);

	EXPECT_FALSE(result.success);
	EXPECT_NE(result.error_message.find("GET"), std::string::npos);
}

// ============================================================================
// Validate Server Response Tests
// ============================================================================

TEST(WebSocketHandshakeTest, ValidateValidServerResponse)
{
	const std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
	std::string response =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
		"\r\n";

	auto result = websocket_handshake::validate_server_response(response, client_key);

	EXPECT_TRUE(result.success);
	EXPECT_TRUE(result.error_message.empty());
}

TEST(WebSocketHandshakeTest, ValidateServerResponseInvalidStatusCode)
{
	const std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
	std::string response =
		"HTTP/1.1 200 OK\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
		"\r\n";

	auto result = websocket_handshake::validate_server_response(response, client_key);

	EXPECT_FALSE(result.success);
	EXPECT_NE(result.error_message.find("status code"), std::string::npos);
}

TEST(WebSocketHandshakeTest, ValidateServerResponseMissingUpgrade)
{
	const std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
	std::string response =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
		"\r\n";

	auto result = websocket_handshake::validate_server_response(response, client_key);

	EXPECT_FALSE(result.success);
	EXPECT_NE(result.error_message.find("Upgrade"), std::string::npos);
}

TEST(WebSocketHandshakeTest, ValidateServerResponseInvalidAcceptKey)
{
	const std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
	std::string response =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: invalid-key\r\n"
		"\r\n";

	auto result = websocket_handshake::validate_server_response(response, client_key);

	EXPECT_FALSE(result.success);
	EXPECT_NE(result.error_message.find("Sec-WebSocket-Accept"), std::string::npos);
}

// ============================================================================
// Header Parsing Tests
// ============================================================================

TEST(WebSocketHandshakeTest, HeaderParsingCaseInsensitive)
{
	std::string request =
		"GET /chat HTTP/1.1\r\n"
		"HOST: example.com\r\n"
		"UPGRADE: WEBSOCKET\r\n"
		"CONNECTION: UPGRADE\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n";

	auto result = websocket_handshake::parse_client_request(request);

	EXPECT_TRUE(result.success);
	// Header names should be lowercase
	EXPECT_EQ(result.headers["host"], "example.com");
	EXPECT_EQ(result.headers["upgrade"], "WEBSOCKET");
	EXPECT_EQ(result.headers["connection"], "UPGRADE");
}

TEST(WebSocketHandshakeTest, HeaderParsingWithWhitespace)
{
	std::string request =
		"GET /chat HTTP/1.1\r\n"
		"Host:   example.com   \r\n"
		"Upgrade:  websocket  \r\n"
		"Connection:  Upgrade  \r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n";

	auto result = websocket_handshake::parse_client_request(request);

	EXPECT_TRUE(result.success);
	// Values should be trimmed
	EXPECT_EQ(result.headers["host"], "example.com");
	EXPECT_EQ(result.headers["upgrade"], "websocket");
}

// ============================================================================
// End-to-End Handshake Tests
// ============================================================================

TEST(WebSocketHandshakeTest, EndToEndHandshake)
{
	// Client creates request
	auto request = websocket_handshake::create_client_handshake(
		"example.com", "/chat", 80);

	// Server parses request
	auto parse_result = websocket_handshake::parse_client_request(request);
	ASSERT_TRUE(parse_result.success);

	// Extract client key
	std::string client_key = parse_result.headers["sec-websocket-key"];
	ASSERT_FALSE(client_key.empty());

	// Server creates response
	auto response = websocket_handshake::create_server_response(client_key);

	// Client validates response
	auto validate_result = websocket_handshake::validate_server_response(
		response, client_key);
	EXPECT_TRUE(validate_result.success);
}
