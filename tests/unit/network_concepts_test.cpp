/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/detail/concepts/network_concepts.h"
#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace concepts = kcenon::network::concepts;

/**
 * @file network_concepts_test.cpp
 * @brief Unit tests for C++20 network concepts
 *
 * Tests validate:
 * - ByteBuffer concept satisfaction and rejection
 * - MutableByteBuffer concept satisfaction and rejection
 * - DataReceiveHandler concept satisfaction
 * - ErrorHandler concept satisfaction
 * - ConnectionHandler concept satisfaction
 * - SessionHandler concept satisfaction
 * - DisconnectionHandler concept satisfaction
 * - RetryCallback concept satisfaction
 * - DataTransformer concept satisfaction
 * - Duration concept satisfaction
 */

// ============================================================================
// Test Helper Types
// ============================================================================

// Satisfies ByteBuffer
struct mock_buffer
{
	const void* data() const { return nullptr; }
	std::size_t size() const { return 0; }
};

// Satisfies MutableByteBuffer
struct mock_mutable_buffer
{
	void* data() { return nullptr; }
	const void* data() const { return nullptr; }
	std::size_t size() const { return 0; }
	void resize(std::size_t) {}
};

// Does NOT satisfy ByteBuffer (missing size())
struct no_size_buffer
{
	const void* data() const { return nullptr; }
};

// Does NOT satisfy ByteBuffer (missing data())
struct no_data_buffer
{
	std::size_t size() const { return 0; }
};

// Satisfies DataTransformer
struct mock_transformer
{
	bool transform(std::vector<uint8_t>&) { return true; }
};

// Satisfies ReversibleDataTransformer
struct mock_reversible_transformer
{
	bool transform(std::vector<uint8_t>&) { return true; }
	bool reverse_transform(std::vector<uint8_t>&) { return true; }
};

// Does NOT satisfy DataTransformer (wrong return type)
struct bad_transformer
{
	void transform(std::vector<uint8_t>&) {}
};

// Mock session for SessionHandler tests
struct mock_session
{
	std::string get_session_id() { return "test-session"; }
	void start_session() {}
	void stop_session() {}
};

// Mock client for NetworkClient tests
struct mock_client
{
	bool is_connected() { return true; }
	void send_packet(std::vector<uint8_t>) {}
	void stop_client() {}
};

// Mock server for NetworkServer tests
struct mock_server
{
	void start_server(unsigned short) {}
	void stop_server() {}
};

// ============================================================================
// ByteBuffer Concept Tests
// ============================================================================

class ByteBufferConceptTest : public ::testing::Test
{
};

TEST_F(ByteBufferConceptTest, VectorUint8Satisfies)
{
	static_assert(concepts::ByteBuffer<std::vector<uint8_t>>,
				  "vector<uint8_t> should satisfy ByteBuffer");
	SUCCEED();
}

TEST_F(ByteBufferConceptTest, StringSatisfies)
{
	static_assert(concepts::ByteBuffer<std::string>,
				  "string should satisfy ByteBuffer");
	SUCCEED();
}

TEST_F(ByteBufferConceptTest, ArraySatisfies)
{
	static_assert(concepts::ByteBuffer<std::array<uint8_t, 16>>,
				  "array<uint8_t, 16> should satisfy ByteBuffer");
	SUCCEED();
}

TEST_F(ByteBufferConceptTest, CustomBufferSatisfies)
{
	static_assert(concepts::ByteBuffer<mock_buffer>,
				  "mock_buffer should satisfy ByteBuffer");
	SUCCEED();
}

TEST_F(ByteBufferConceptTest, MissingSizeDoesNotSatisfy)
{
	static_assert(!concepts::ByteBuffer<no_size_buffer>,
				  "no_size_buffer should NOT satisfy ByteBuffer");
	SUCCEED();
}

TEST_F(ByteBufferConceptTest, MissingDataDoesNotSatisfy)
{
	static_assert(!concepts::ByteBuffer<no_data_buffer>,
				  "no_data_buffer should NOT satisfy ByteBuffer");
	SUCCEED();
}

TEST_F(ByteBufferConceptTest, IntDoesNotSatisfy)
{
	static_assert(!concepts::ByteBuffer<int>,
				  "int should NOT satisfy ByteBuffer");
	SUCCEED();
}

// ============================================================================
// MutableByteBuffer Concept Tests
// ============================================================================

class MutableByteBufferConceptTest : public ::testing::Test
{
};

TEST_F(MutableByteBufferConceptTest, VectorUint8Satisfies)
{
	static_assert(concepts::MutableByteBuffer<std::vector<uint8_t>>,
				  "vector<uint8_t> should satisfy MutableByteBuffer");
	SUCCEED();
}

TEST_F(MutableByteBufferConceptTest, CustomMutableBufferSatisfies)
{
	static_assert(concepts::MutableByteBuffer<mock_mutable_buffer>,
				  "mock_mutable_buffer should satisfy MutableByteBuffer");
	SUCCEED();
}

TEST_F(MutableByteBufferConceptTest, ConstBufferDoesNotSatisfy)
{
	// mock_buffer has no resize() and data() returns const void*
	static_assert(!concepts::MutableByteBuffer<mock_buffer>,
				  "mock_buffer should NOT satisfy MutableByteBuffer");
	SUCCEED();
}

TEST_F(MutableByteBufferConceptTest, StringSatisfies)
{
	static_assert(concepts::MutableByteBuffer<std::string>,
				  "string should satisfy MutableByteBuffer");
	SUCCEED();
}

// ============================================================================
// Callback Concept Tests
// ============================================================================

class CallbackConceptTest : public ::testing::Test
{
};

TEST_F(CallbackConceptTest, DataReceiveHandlerLambda)
{
	auto handler = [](const std::vector<uint8_t>&) {};
	static_assert(concepts::DataReceiveHandler<decltype(handler)>,
				  "Lambda should satisfy DataReceiveHandler");
	SUCCEED();
}

TEST_F(CallbackConceptTest, DataReceiveHandlerFunction)
{
	static_assert(
		concepts::DataReceiveHandler<std::function<void(const std::vector<uint8_t>&)>>,
		"std::function should satisfy DataReceiveHandler");
	SUCCEED();
}

TEST_F(CallbackConceptTest, ErrorHandlerLambda)
{
	auto handler = [](std::error_code) {};
	static_assert(concepts::ErrorHandler<decltype(handler)>,
				  "Lambda should satisfy ErrorHandler");
	SUCCEED();
}

TEST_F(CallbackConceptTest, ConnectionHandlerLambda)
{
	auto handler = []() {};
	static_assert(concepts::ConnectionHandler<decltype(handler)>,
				  "Lambda should satisfy ConnectionHandler");
	SUCCEED();
}

TEST_F(CallbackConceptTest, DisconnectionHandlerLambda)
{
	auto handler = [](const std::string&) {};
	static_assert(concepts::DisconnectionHandler<decltype(handler)>,
				  "Lambda should satisfy DisconnectionHandler");
	SUCCEED();
}

TEST_F(CallbackConceptTest, RetryCallbackLambda)
{
	auto handler = [](std::size_t) {};
	static_assert(concepts::RetryCallback<decltype(handler)>,
				  "Lambda should satisfy RetryCallback");
	SUCCEED();
}

TEST_F(CallbackConceptTest, SessionHandlerLambda)
{
	auto handler = [](std::shared_ptr<mock_session>) {};
	static_assert(concepts::SessionHandler<decltype(handler), mock_session>,
				  "Lambda should satisfy SessionHandler");
	SUCCEED();
}

TEST_F(CallbackConceptTest, SessionDataHandlerLambda)
{
	auto handler = [](std::shared_ptr<mock_session>,
					  const std::vector<uint8_t>&) {};
	static_assert(
		concepts::SessionDataHandler<decltype(handler), mock_session>,
		"Lambda should satisfy SessionDataHandler");
	SUCCEED();
}

TEST_F(CallbackConceptTest, SessionErrorHandlerLambda)
{
	auto handler = [](std::shared_ptr<mock_session>, std::error_code) {};
	static_assert(
		concepts::SessionErrorHandler<decltype(handler), mock_session>,
		"Lambda should satisfy SessionErrorHandler");
	SUCCEED();
}

// ============================================================================
// Network Component Concept Tests
// ============================================================================

class NetworkComponentConceptTest : public ::testing::Test
{
};

TEST_F(NetworkComponentConceptTest, MockClientSatisfiesNetworkClient)
{
	static_assert(concepts::NetworkClient<mock_client>,
				  "mock_client should satisfy NetworkClient");
	SUCCEED();
}

TEST_F(NetworkComponentConceptTest, MockServerSatisfiesNetworkServer)
{
	static_assert(concepts::NetworkServer<mock_server>,
				  "mock_server should satisfy NetworkServer");
	SUCCEED();
}

TEST_F(NetworkComponentConceptTest, MockSessionSatisfiesNetworkSession)
{
	static_assert(concepts::NetworkSession<mock_session>,
				  "mock_session should satisfy NetworkSession");
	SUCCEED();
}

TEST_F(NetworkComponentConceptTest, IntDoesNotSatisfyNetworkClient)
{
	static_assert(!concepts::NetworkClient<int>,
				  "int should NOT satisfy NetworkClient");
	SUCCEED();
}

// ============================================================================
// DataTransformer Concept Tests
// ============================================================================

class DataTransformerConceptTest : public ::testing::Test
{
};

TEST_F(DataTransformerConceptTest, MockTransformerSatisfies)
{
	static_assert(concepts::DataTransformer<mock_transformer>,
				  "mock_transformer should satisfy DataTransformer");
	SUCCEED();
}

TEST_F(DataTransformerConceptTest, ReversibleTransformerSatisfiesBase)
{
	static_assert(concepts::DataTransformer<mock_reversible_transformer>,
				  "mock_reversible_transformer should satisfy DataTransformer");
	SUCCEED();
}

TEST_F(DataTransformerConceptTest, ReversibleTransformerSatisfiesFull)
{
	static_assert(
		concepts::ReversibleDataTransformer<mock_reversible_transformer>,
		"mock_reversible_transformer should satisfy ReversibleDataTransformer");
	SUCCEED();
}

TEST_F(DataTransformerConceptTest, BasicTransformerDoesNotSatisfyReversible)
{
	static_assert(!concepts::ReversibleDataTransformer<mock_transformer>,
				  "mock_transformer should NOT satisfy ReversibleDataTransformer");
	SUCCEED();
}

TEST_F(DataTransformerConceptTest, BadTransformerDoesNotSatisfy)
{
	static_assert(!concepts::DataTransformer<bad_transformer>,
				  "bad_transformer should NOT satisfy DataTransformer");
	SUCCEED();
}

// ============================================================================
// Duration Concept Tests
// ============================================================================

class DurationConceptTest : public ::testing::Test
{
};

TEST_F(DurationConceptTest, MillisecondsSatisfies)
{
	static_assert(concepts::Duration<std::chrono::milliseconds>,
				  "milliseconds should satisfy Duration");
	SUCCEED();
}

TEST_F(DurationConceptTest, SecondsSatisfies)
{
	static_assert(concepts::Duration<std::chrono::seconds>,
				  "seconds should satisfy Duration");
	SUCCEED();
}

TEST_F(DurationConceptTest, MicrosecondsSatisfies)
{
	static_assert(concepts::Duration<std::chrono::microseconds>,
				  "microseconds should satisfy Duration");
	SUCCEED();
}

TEST_F(DurationConceptTest, IntDoesNotSatisfy)
{
	static_assert(!concepts::Duration<int>,
				  "int should NOT satisfy Duration");
	SUCCEED();
}

TEST_F(DurationConceptTest, StringDoesNotSatisfy)
{
	static_assert(!concepts::Duration<std::string>,
				  "string should NOT satisfy Duration");
	SUCCEED();
}
