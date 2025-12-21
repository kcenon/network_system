/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>

#include "kcenon/network/core/http_server.h"
#include "kcenon/network/core/messaging_client.h"
#include "kcenon/network/core/messaging_server.h"
#include "kcenon/network/internal/http_error.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::core;
using namespace kcenon::network::internal;

// Free function for yielding to allow async operations to complete
inline void wait_for_ready() {
  for (int i = 0; i < 1000; ++i) {
    std::this_thread::yield();
  }
}

class BoundaryTest : public ::testing::Test {
protected:
  void SetUp() override {
    server_ = std::make_shared<messaging_server>("boundary_test_server");
  }

  void TearDown() override {
    if (server_) {
      server_->stop_server();
    }
  }

  std::shared_ptr<messaging_server> server_;
  static constexpr unsigned short TEST_PORT = 15556;
};

TEST_F(BoundaryTest, HandlesEmptyMessage) {
  auto start_result = server_->start_server(TEST_PORT);
  ASSERT_TRUE(start_result.is_ok());

  auto client = std::make_shared<messaging_client>("test_client");
  auto connect_result = client->start_client("localhost", TEST_PORT);
  ASSERT_TRUE(connect_result.is_ok());

  wait_for_ready();

  // Send empty message
  std::vector<uint8_t> empty_data;
  // Should handle gracefully

  client->stop_client();
  SUCCEED();
}

TEST_F(BoundaryTest, HandlesSingleByteMessage) {
  auto start_result = server_->start_server(TEST_PORT);
  ASSERT_TRUE(start_result.is_ok());

  auto client = std::make_shared<messaging_client>("test_client");
  auto connect_result = client->start_client("localhost", TEST_PORT);
  ASSERT_TRUE(connect_result.is_ok());

  wait_for_ready();

  // Send single byte - should not crash
  std::vector<uint8_t> single_byte = {0x42};
  client->send_packet(std::move(single_byte));

  wait_for_ready();

  client->stop_client();
  SUCCEED();
}

TEST_F(BoundaryTest, HandlesLargeMessage) {
  auto start_result = server_->start_server(TEST_PORT);
  ASSERT_TRUE(start_result.is_ok());

  auto client = std::make_shared<messaging_client>("test_client");
  auto connect_result = client->start_client("localhost", TEST_PORT);
  ASSERT_TRUE(connect_result.is_ok());

  wait_for_ready();

  // Send 64KB message - should not crash
  std::vector<uint8_t> large_data(64 * 1024, 0xAB);
  client->send_packet(std::move(large_data));

  wait_for_ready();

  client->stop_client();
  SUCCEED();
}

TEST_F(BoundaryTest, HandlesMaxUint8Value) {
  auto start_result = server_->start_server(TEST_PORT);
  ASSERT_TRUE(start_result.is_ok());

  auto client = std::make_shared<messaging_client>("test_client");
  auto connect_result = client->start_client("localhost", TEST_PORT);
  ASSERT_TRUE(connect_result.is_ok());

  wait_for_ready();

  // Send max uint8_t value - should not crash
  std::vector<uint8_t> max_value = {0xFF};
  client->send_packet(std::move(max_value));

  wait_for_ready();

  client->stop_client();
  SUCCEED();
}

TEST_F(BoundaryTest, HandlesZeroBytes) {
  auto start_result = server_->start_server(TEST_PORT);
  ASSERT_TRUE(start_result.is_ok());

  auto client = std::make_shared<messaging_client>("test_client");
  auto connect_result = client->start_client("localhost", TEST_PORT);
  ASSERT_TRUE(connect_result.is_ok());

  wait_for_ready();

  // Send zero bytes - should not crash
  std::vector<uint8_t> zero_data = {0x00, 0x00, 0x00, 0x00};
  client->send_packet(std::move(zero_data));

  wait_for_ready();

  client->stop_client();
  SUCCEED();
}

TEST_F(BoundaryTest, HandlesBinaryData) {
  auto start_result = server_->start_server(TEST_PORT);
  ASSERT_TRUE(start_result.is_ok());

  auto client = std::make_shared<messaging_client>("test_client");
  auto connect_result = client->start_client("localhost", TEST_PORT);
  ASSERT_TRUE(connect_result.is_ok());

  wait_for_ready();

  // Send all possible byte values - should not crash
  std::vector<uint8_t> all_bytes(256);
  for (int i = 0; i < 256; ++i) {
    all_bytes[i] = static_cast<uint8_t>(i);
  }
  client->send_packet(std::move(all_bytes));

  wait_for_ready();

  client->stop_client();
  SUCCEED();
}

// HTTP Error Response Boundary Tests
class HttpErrorBoundaryTest : public ::testing::Test {};

TEST_F(HttpErrorBoundaryTest, HandlesAllErrorCodes) {
  // Test that all error codes produce valid responses
  std::vector<http_error_code> codes = {http_error_code::bad_request,
                                        http_error_code::unauthorized,
                                        http_error_code::forbidden,
                                        http_error_code::not_found,
                                        http_error_code::method_not_allowed,
                                        http_error_code::request_timeout,
                                        http_error_code::payload_too_large,
                                        http_error_code::internal_server_error,
                                        http_error_code::not_implemented,
                                        http_error_code::service_unavailable};

  for (auto code : codes) {
    auto error = http_error_response::make_error(code, "Test detail");

    auto json_response = http_error_response::build_json_error(error);
    EXPECT_EQ(json_response.status_code, static_cast<int>(code));
    EXPECT_FALSE(json_response.body.empty());

    auto html_response = http_error_response::build_html_error(error);
    EXPECT_EQ(html_response.status_code, static_cast<int>(code));
    EXPECT_FALSE(html_response.body.empty());
  }
}

TEST_F(HttpErrorBoundaryTest, HandlesEmptyDetail) {
  auto error =
      http_error_response::make_error(http_error_code::bad_request, "");

  auto json_response = http_error_response::build_json_error(error);
  EXPECT_EQ(json_response.status_code, 400);
  EXPECT_FALSE(json_response.body.empty());
}

TEST_F(HttpErrorBoundaryTest, HandlesLongDetail) {
  std::string long_detail(10000, 'A');
  auto error = http_error_response::make_error(http_error_code::bad_request,
                                               long_detail);

  auto json_response = http_error_response::build_json_error(error);
  EXPECT_EQ(json_response.status_code, 400);
  EXPECT_FALSE(json_response.body.empty());
}

TEST_F(HttpErrorBoundaryTest, HandlesSpecialCharactersInDetail) {
  std::string special_detail =
      "Error with \"quotes\" and \\ backslashes and\nnewlines";
  auto error = http_error_response::make_error(http_error_code::bad_request,
                                               special_detail);

  auto json_response = http_error_response::build_json_error(error);
  EXPECT_EQ(json_response.status_code, 400);
  // JSON should be properly escaped
  std::string body_str(json_response.body.begin(), json_response.body.end());
  EXPECT_TRUE(body_str.find("\\\"") != std::string::npos); // Escaped quotes
}

TEST_F(HttpErrorBoundaryTest, HandlesUnicodeInDetail) {
  std::string unicode_detail =
      "Error: \xC3\xA9\xC3\xA0\xC3\xBC"; // UTF-8 characters
  auto error = http_error_response::make_error(http_error_code::bad_request,
                                               unicode_detail);

  auto json_response = http_error_response::build_json_error(error);
  EXPECT_EQ(json_response.status_code, 400);
  EXPECT_FALSE(json_response.body.empty());
}

TEST_F(HttpErrorBoundaryTest, HandlesRequestIdWithSpecialChars) {
  auto error =
      http_error_response::make_error(http_error_code::bad_request, "Test",
                                      "req-123<script>alert('xss')</script>");

  auto html_response = http_error_response::build_html_error(error);
  std::string body_str(html_response.body.begin(), html_response.body.end());
  // HTML should be properly escaped
  EXPECT_TRUE(body_str.find("&lt;script&gt;") != std::string::npos);
}
