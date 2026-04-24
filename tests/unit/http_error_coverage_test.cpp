// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

// Coverage-expansion tests for src/internal/http_error.cpp targeting branches
// not reached by tests/test_network_config_http_error.cpp: the default branch
// of get_error_status_text, every branch of the file-local escape_json_string
// helper (special characters, low control bytes, plain ASCII), every branch of
// the file-local escape_html_string helper, and the request_id / detail /
// message empty-vs-non-empty branches of build_json_error and build_html_error.
// Also exercises parse_error::to_http_error and http_error convenience queries
// through the public surface.
//
// Part of epic #953 (expand unit test coverage from 40% to 80%). Single-file
// sub-issue #1023.

#include "internal/http/http_error.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

using namespace kcenon::network::internal;

namespace {

auto contains(const std::string& haystack, const std::string& needle) -> bool
{
	return haystack.find(needle) != std::string::npos;
}

}  // namespace

// ============================================================================
// get_error_status_text: default / unknown branch
// ============================================================================

TEST(HttpErrorCoverageTest, GetErrorStatusTextUnknownCodeFallsThroughDefault)
{
	// Cast a value that is not present in the switch to force the default arm.
	auto unknown = static_cast<http_error_code>(999);
	EXPECT_EQ(get_error_status_text(unknown), "Unknown Error");
}

TEST(HttpErrorCoverageTest, GetErrorStatusTextZeroIsUnknown)
{
	auto zero = static_cast<http_error_code>(0);
	EXPECT_EQ(get_error_status_text(zero), "Unknown Error");
}

// ============================================================================
// http_error convenience queries
// ============================================================================

TEST(HttpErrorCoverageTest, HttpErrorStatusCodeReturnsIntegerValue)
{
	http_error err;
	err.code = http_error_code::not_found;
	EXPECT_EQ(err.status_code(), 404);
}

TEST(HttpErrorCoverageTest, HttpErrorIsClientErrorForClientCodes)
{
	http_error err;
	err.code = http_error_code::bad_request;
	EXPECT_TRUE(err.is_client_error());
	EXPECT_FALSE(err.is_server_error());
}

TEST(HttpErrorCoverageTest, HttpErrorIsServerErrorForServerCodes)
{
	http_error err;
	err.code = http_error_code::internal_server_error;
	EXPECT_FALSE(err.is_client_error());
	EXPECT_TRUE(err.is_server_error());
}

TEST(HttpErrorCoverageTest, HttpErrorOutOfRangeCodeIsNeitherClientNorServer)
{
	http_error err;
	err.code = static_cast<http_error_code>(300);  // Redirection range
	EXPECT_FALSE(err.is_client_error());
	EXPECT_FALSE(err.is_server_error());
}

TEST(HttpErrorCoverageTest, HttpErrorAtBoundaryCodes)
{
	http_error err;
	// 399 is not mapped and lies just below the client-error boundary.
	err.code = static_cast<http_error_code>(399);
	EXPECT_FALSE(err.is_client_error());
	EXPECT_FALSE(err.is_server_error());

	// 400 is the inclusive start of client errors.
	err.code = static_cast<http_error_code>(400);
	EXPECT_TRUE(err.is_client_error());

	// 499 is still client.
	err.code = static_cast<http_error_code>(499);
	EXPECT_TRUE(err.is_client_error());

	// 500 is the inclusive start of server errors.
	err.code = static_cast<http_error_code>(500);
	EXPECT_TRUE(err.is_server_error());

	// 599 is still server.
	err.code = static_cast<http_error_code>(599);
	EXPECT_TRUE(err.is_server_error());

	// 600 is outside both ranges.
	err.code = static_cast<http_error_code>(600);
	EXPECT_FALSE(err.is_client_error());
	EXPECT_FALSE(err.is_server_error());
}

// ============================================================================
// parse_error::to_http_error
// ============================================================================

TEST(HttpErrorCoverageTest, ParseErrorToHttpErrorWithEmptyContext)
{
	parse_error pe;
	pe.error_type = parse_error_type::invalid_method;
	pe.line_number = 1;
	pe.column_number = 5;
	pe.message = "method is empty";
	// context intentionally left empty

	auto err = pe.to_http_error();
	EXPECT_EQ(err.code, http_error_code::bad_request);
	EXPECT_EQ(err.message, "Bad Request");
	EXPECT_EQ(err.detail, "method is empty");
	EXPECT_FALSE(contains(err.detail, "near:"));
}

TEST(HttpErrorCoverageTest, ParseErrorToHttpErrorWithContextAppendsNearClause)
{
	parse_error pe;
	pe.error_type = parse_error_type::malformed_request;
	pe.message = "unexpected token";
	pe.context = "GET /\\x00";

	auto err = pe.to_http_error();
	EXPECT_EQ(err.code, http_error_code::bad_request);
	EXPECT_TRUE(contains(err.detail, "unexpected token"));
	EXPECT_TRUE(contains(err.detail, "near: GET /\\x00"));
}

// ============================================================================
// http_error_response::make_error: detail / request_id branches
// ============================================================================

TEST(HttpErrorCoverageTest, MakeErrorDefaultsHaveEmptyDetailAndRequestId)
{
	auto err = http_error_response::make_error(http_error_code::not_found);
	EXPECT_EQ(err.code, http_error_code::not_found);
	EXPECT_EQ(err.message, "Not Found");
	EXPECT_TRUE(err.detail.empty());
	EXPECT_TRUE(err.request_id.empty());
	// Timestamp is set to now() inside the implementation; verify it is not
	// the default-constructed epoch.
	EXPECT_GT(err.timestamp.time_since_epoch().count(), 0);
}

TEST(HttpErrorCoverageTest, MakeErrorWithDetailOnly)
{
	auto err = http_error_response::make_error(http_error_code::conflict, "duplicate key");
	EXPECT_EQ(err.detail, "duplicate key");
	EXPECT_TRUE(err.request_id.empty());
}

TEST(HttpErrorCoverageTest, MakeErrorWithDetailAndRequestId)
{
	auto err = http_error_response::make_error(http_error_code::forbidden, "no access",
											   "req-abc-123");
	EXPECT_EQ(err.detail, "no access");
	EXPECT_EQ(err.request_id, "req-abc-123");
}

TEST(HttpErrorCoverageTest, MakeErrorUsesUnknownTextForUnmappedCode)
{
	// Covers the default branch of get_error_status_text via make_error's
	// message assignment.
	auto err = http_error_response::make_error(static_cast<http_error_code>(777));
	EXPECT_EQ(err.message, "Unknown Error");
}

// ============================================================================
// build_json_error: detail fallback, request_id, timestamp, escape_json_string
// ============================================================================

TEST(HttpErrorCoverageTest, BuildJsonErrorUsesDetailWhenPresent)
{
	auto err = http_error_response::make_error(http_error_code::bad_request, "boom");
	auto response = http_error_response::build_json_error(err);
	auto body = response.get_body_string();

	EXPECT_EQ(response.status_code, 400);
	EXPECT_EQ(response.status_message, "Bad Request");
	EXPECT_TRUE(contains(body, "\"detail\": \"boom\""));

	auto ct = response.get_header("Content-Type");
	ASSERT_TRUE(ct.has_value());
	EXPECT_EQ(*ct, "application/problem+json; charset=utf-8");
}

TEST(HttpErrorCoverageTest, BuildJsonErrorFallsBackToMessageWhenDetailIsEmpty)
{
	// make_error populates message from the status text and leaves detail empty
	// by default; this exercises the ternary branch
	// (error.detail.empty() ? error.message : error.detail).
	auto err = http_error_response::make_error(http_error_code::not_found);
	auto body = http_error_response::build_json_error(err).get_body_string();

	EXPECT_TRUE(contains(body, "\"detail\": \"Not Found\""));
}

TEST(HttpErrorCoverageTest, BuildJsonErrorOmitsInstanceWhenRequestIdEmpty)
{
	auto err = http_error_response::make_error(http_error_code::not_found, "missing");
	auto body = http_error_response::build_json_error(err).get_body_string();

	EXPECT_FALSE(contains(body, "\"instance\""));
}

TEST(HttpErrorCoverageTest, BuildJsonErrorIncludesInstanceWhenRequestIdPresent)
{
	auto err = http_error_response::make_error(http_error_code::not_found, "missing",
											   "trace-42");
	auto body = http_error_response::build_json_error(err).get_body_string();

	EXPECT_TRUE(contains(body, "\"instance\": \"trace-42\""));
}

TEST(HttpErrorCoverageTest, BuildJsonErrorContainsIso8601Timestamp)
{
	auto err = http_error_response::make_error(http_error_code::service_unavailable);
	// Force a deterministic timestamp to validate the strftime path.
	err.timestamp = std::chrono::system_clock::from_time_t(0);
	auto body = http_error_response::build_json_error(err).get_body_string();

	// Zero-time in UTC is 1970-01-01T00:00:00Z.
	EXPECT_TRUE(contains(body, "\"timestamp\": \"1970-01-01T00:00:00Z\""));
}

// escape_json_string: every branch of the switch statement.

TEST(HttpErrorCoverageTest, BuildJsonErrorEscapesDoubleQuote)
{
	auto err = http_error_response::make_error(http_error_code::bad_request,
											   "a \"quoted\" value");
	auto body = http_error_response::build_json_error(err).get_body_string();
	EXPECT_TRUE(contains(body, "a \\\"quoted\\\" value"));
}

TEST(HttpErrorCoverageTest, BuildJsonErrorEscapesBackslash)
{
	auto err = http_error_response::make_error(http_error_code::bad_request, "a\\b");
	auto body = http_error_response::build_json_error(err).get_body_string();
	EXPECT_TRUE(contains(body, "a\\\\b"));
}

TEST(HttpErrorCoverageTest, BuildJsonErrorEscapesBackspaceFormfeedNewlineCrTab)
{
	std::string detail;
	detail.push_back('\b');
	detail.push_back('\f');
	detail.push_back('\n');
	detail.push_back('\r');
	detail.push_back('\t');
	auto err = http_error_response::make_error(http_error_code::bad_request, detail);
	auto body = http_error_response::build_json_error(err).get_body_string();

	EXPECT_TRUE(contains(body, "\\b"));
	EXPECT_TRUE(contains(body, "\\f"));
	EXPECT_TRUE(contains(body, "\\n"));
	EXPECT_TRUE(contains(body, "\\r"));
	EXPECT_TRUE(contains(body, "\\t"));
}

TEST(HttpErrorCoverageTest, BuildJsonErrorEscapesLowControlBytesAsUnicodeEscape)
{
	std::string detail;
	detail.push_back(static_cast<char>(0x01));  // SOH
	detail.push_back(static_cast<char>(0x1F));  // US, boundary of low-control range
	auto err = http_error_response::make_error(http_error_code::bad_request, detail);
	auto body = http_error_response::build_json_error(err).get_body_string();

	EXPECT_TRUE(contains(body, "\\u0001"));
	EXPECT_TRUE(contains(body, "\\u001f"));
}

TEST(HttpErrorCoverageTest, BuildJsonErrorPassesPlainAsciiThrough)
{
	auto err = http_error_response::make_error(http_error_code::bad_request,
											   "plain-ASCII_text 123");
	auto body = http_error_response::build_json_error(err).get_body_string();
	EXPECT_TRUE(contains(body, "plain-ASCII_text 123"));
}

TEST(HttpErrorCoverageTest, BuildJsonErrorPassesHighBitBytesThrough)
{
	// Bytes >= 0x20 take the default "no-escape" branch in escape_json_string,
	// including high-bit UTF-8 continuation bytes.
	std::string detail;
	detail.push_back(static_cast<char>(0xC3));  // LATIN CAPITAL A WITH TILDE (lead)
	detail.push_back(static_cast<char>(0x83));  // trail byte
	auto err = http_error_response::make_error(http_error_code::bad_request, detail);
	auto body = http_error_response::build_json_error(err).get_body_string();

	EXPECT_TRUE(contains(body, detail));
}

// ============================================================================
// build_html_error: message / detail / request_id branches, escape_html_string
// ============================================================================

TEST(HttpErrorCoverageTest, BuildHtmlErrorMinimalFieldsProducesValidShell)
{
	// All three optional string fields empty: message, detail, request_id.
	// This exercises the "skip" arms of the three if-branches plus the
	// default (no-escape) arm of escape_html_string.
	http_error err;
	err.code = http_error_code::not_found;
	err.message.clear();
	err.detail.clear();
	err.request_id.clear();
	auto response = http_error_response::build_html_error(err);
	auto body = response.get_body_string();

	EXPECT_EQ(response.status_code, 404);
	EXPECT_EQ(response.status_message, "Not Found");
	EXPECT_TRUE(contains(body, "<!DOCTYPE html>"));
	EXPECT_TRUE(contains(body, "<title>404 Not Found</title>"));
	EXPECT_TRUE(contains(body, "<h1>404 Not Found</h1>"));

	// Optional sections are omitted.
	EXPECT_FALSE(contains(body, "class=\"detail\""));
	EXPECT_FALSE(contains(body, "Request ID:"));

	auto ct = response.get_header("Content-Type");
	ASSERT_TRUE(ct.has_value());
	EXPECT_EQ(*ct, "text/html; charset=utf-8");
}

TEST(HttpErrorCoverageTest, BuildHtmlErrorIncludesMessageParagraphWhenPresent)
{
	auto err = http_error_response::make_error(http_error_code::bad_request);
	// make_error populates message from the status text; explicitly set to
	// ensure non-empty.
	err.message = "Something went wrong";
	err.detail.clear();
	err.request_id.clear();
	auto body = http_error_response::build_html_error(err).get_body_string();

	EXPECT_TRUE(contains(body, "<p>Something went wrong</p>"));
	EXPECT_FALSE(contains(body, "class=\"detail\""));
	EXPECT_FALSE(contains(body, "Request ID:"));
}

TEST(HttpErrorCoverageTest, BuildHtmlErrorIncludesDetailBlockWhenPresent)
{
	auto err = http_error_response::make_error(http_error_code::conflict, "duplicate id");
	err.request_id.clear();
	auto body = http_error_response::build_html_error(err).get_body_string();

	EXPECT_TRUE(contains(body, "class=\"detail\""));
	EXPECT_TRUE(contains(body, "<strong>Details:</strong> duplicate id"));
	EXPECT_FALSE(contains(body, "Request ID:"));
}

TEST(HttpErrorCoverageTest, BuildHtmlErrorIncludesRequestIdWhenPresent)
{
	auto err = http_error_response::make_error(http_error_code::internal_server_error,
											   "fail", "req-7");
	auto body = http_error_response::build_html_error(err).get_body_string();

	EXPECT_TRUE(contains(body, "Request ID: req-7"));
}

TEST(HttpErrorCoverageTest, BuildHtmlErrorEscapesAllSpecialCharacters)
{
	// escape_html_string handles: & < > " ' and default (unchanged).
	auto err = http_error_response::make_error(http_error_code::bad_request,
											   "<a href=\"x\" title='y'>t&e</a>");
	err.message = "plain <ok>";
	err.request_id = "id&42";
	auto body = http_error_response::build_html_error(err).get_body_string();

	// Detail text gets entity-escaped.
	EXPECT_TRUE(contains(body, "&lt;a href=&quot;x&quot; title=&#39;y&#39;&gt;t&amp;e&lt;/a&gt;"));
	// Message text also escaped.
	EXPECT_TRUE(contains(body, "<p>plain &lt;ok&gt;</p>"));
	// Request-id also escaped.
	EXPECT_TRUE(contains(body, "Request ID: id&amp;42"));
}

TEST(HttpErrorCoverageTest, BuildHtmlErrorTitleReflectsStatusMessage)
{
	auto err = http_error_response::make_error(http_error_code::im_a_teapot);
	auto response = http_error_response::build_html_error(err);
	auto body = response.get_body_string();

	EXPECT_EQ(response.status_code, 418);
	// I'm a teapot contains a single-quote that must be HTML-escaped via the
	// escape_html_string path.
	EXPECT_TRUE(contains(body, "I&#39;m a teapot"));
}

// ============================================================================
// Sanity: at least one check of JSON detail escaping on an empty-detail error.
// ============================================================================

TEST(HttpErrorCoverageTest, BuildJsonErrorEmptyMessageAndDetailStillProducesBody)
{
	http_error err;
	err.code = http_error_code::internal_server_error;
	err.message.clear();
	err.detail.clear();
	err.request_id.clear();
	auto body = http_error_response::build_json_error(err).get_body_string();

	// Detail falls back to (empty) message.
	EXPECT_TRUE(contains(body, "\"detail\": \"\""));
	EXPECT_TRUE(contains(body, "\"status\": 500"));
}
