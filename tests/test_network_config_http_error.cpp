/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, network_system contributors
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

#include <gtest/gtest.h>

#include "kcenon/network/detail/config/network_config.h"
#include "kcenon/network/detail/config/network_system_config.h"
#include "internal/http/http_error.h"

#include <string>
#include <string_view>

using namespace kcenon::network;

// ============================================================================
// ThreadPoolConfigTest
// ============================================================================

class ThreadPoolConfigTest : public ::testing::Test
{
};

TEST_F(ThreadPoolConfigTest, DefaultValues)
{
	config::thread_pool_config cfg;
	EXPECT_EQ(cfg.worker_count, 0u);
	EXPECT_EQ(cfg.queue_capacity, 10000u);
	EXPECT_EQ(cfg.pool_name, "network_pool");
}

// ============================================================================
// LoggerConfigTest
// ============================================================================

class LoggerConfigTest : public ::testing::Test
{
};

TEST_F(LoggerConfigTest, DefaultValues)
{
	config::logger_config cfg;
	EXPECT_EQ(cfg.min_level, integration::log_level::info);
	EXPECT_TRUE(cfg.async_logging);
	EXPECT_EQ(cfg.buffer_size, 8192u);
	EXPECT_TRUE(cfg.log_file_path.empty());
}

// ============================================================================
// MonitoringConfigTest
// ============================================================================

class MonitoringConfigTest : public ::testing::Test
{
};

TEST_F(MonitoringConfigTest, DefaultValues)
{
	config::monitoring_config cfg;
	EXPECT_TRUE(cfg.enabled);
	EXPECT_EQ(cfg.metrics_interval, std::chrono::seconds(5));
	EXPECT_EQ(cfg.service_name, "network_system");
}

// ============================================================================
// NetworkConfigPresetTest
// ============================================================================

class NetworkConfigPresetTest : public ::testing::Test
{
};

TEST_F(NetworkConfigPresetTest, DefaultConstruction)
{
	config::network_config cfg;
	EXPECT_EQ(cfg.thread_pool.worker_count, 0u);
	EXPECT_EQ(cfg.logger.min_level, integration::log_level::info);
	EXPECT_TRUE(cfg.monitoring.enabled);
}

TEST_F(NetworkConfigPresetTest, DevelopmentPreset)
{
	auto cfg = config::network_config::development();
	EXPECT_EQ(cfg.logger.min_level, integration::log_level::debug);
	EXPECT_FALSE(cfg.logger.async_logging);
	EXPECT_TRUE(cfg.monitoring.enabled);
	EXPECT_EQ(cfg.thread_pool.worker_count, 2u);
}

TEST_F(NetworkConfigPresetTest, ProductionPreset)
{
	auto cfg = config::network_config::production();
	EXPECT_EQ(cfg.logger.min_level, integration::log_level::info);
	EXPECT_TRUE(cfg.logger.async_logging);
	EXPECT_TRUE(cfg.monitoring.enabled);
	EXPECT_EQ(cfg.thread_pool.worker_count, 0u);
}

TEST_F(NetworkConfigPresetTest, TestingPreset)
{
	auto cfg = config::network_config::testing();
	EXPECT_EQ(cfg.logger.min_level, integration::log_level::warn);
	EXPECT_FALSE(cfg.logger.async_logging);
	EXPECT_FALSE(cfg.monitoring.enabled);
	EXPECT_EQ(cfg.thread_pool.worker_count, 1u);
}

TEST_F(NetworkConfigPresetTest, PresetsAreDifferent)
{
	auto dev = config::network_config::development();
	auto prod = config::network_config::production();
	auto test = config::network_config::testing();

	// Each preset should have distinct log levels
	EXPECT_NE(dev.logger.min_level, prod.logger.min_level);
	EXPECT_NE(prod.logger.min_level, test.logger.min_level);
	EXPECT_NE(dev.logger.min_level, test.logger.min_level);
}

// ============================================================================
// NetworkSystemConfigTest
// ============================================================================

class NetworkSystemConfigTest : public ::testing::Test
{
};

TEST_F(NetworkSystemConfigTest, DefaultValues)
{
	config::network_system_config cfg;
	// Default runtime is production
	EXPECT_EQ(cfg.runtime.logger.min_level, integration::log_level::info);
	EXPECT_TRUE(cfg.runtime.logger.async_logging);
	// External components default to nullptr
	EXPECT_EQ(cfg.executor, nullptr);
	EXPECT_EQ(cfg.logger, nullptr);
	EXPECT_EQ(cfg.monitor, nullptr);
}

// ============================================================================
// HttpErrorCodeTest
// ============================================================================

using namespace kcenon::network::internal;

class HttpErrorCodeTest : public ::testing::Test
{
};

TEST_F(HttpErrorCodeTest, ClientErrorCodesAreCorrect)
{
	EXPECT_EQ(static_cast<int>(http_error_code::bad_request), 400);
	EXPECT_EQ(static_cast<int>(http_error_code::unauthorized), 401);
	EXPECT_EQ(static_cast<int>(http_error_code::payment_required), 402);
	EXPECT_EQ(static_cast<int>(http_error_code::forbidden), 403);
	EXPECT_EQ(static_cast<int>(http_error_code::not_found), 404);
	EXPECT_EQ(static_cast<int>(http_error_code::method_not_allowed), 405);
	EXPECT_EQ(static_cast<int>(http_error_code::not_acceptable), 406);
	EXPECT_EQ(static_cast<int>(http_error_code::proxy_authentication_required), 407);
	EXPECT_EQ(static_cast<int>(http_error_code::request_timeout), 408);
	EXPECT_EQ(static_cast<int>(http_error_code::conflict), 409);
	EXPECT_EQ(static_cast<int>(http_error_code::gone), 410);
	EXPECT_EQ(static_cast<int>(http_error_code::length_required), 411);
	EXPECT_EQ(static_cast<int>(http_error_code::precondition_failed), 412);
	EXPECT_EQ(static_cast<int>(http_error_code::payload_too_large), 413);
	EXPECT_EQ(static_cast<int>(http_error_code::uri_too_long), 414);
	EXPECT_EQ(static_cast<int>(http_error_code::unsupported_media_type), 415);
	EXPECT_EQ(static_cast<int>(http_error_code::range_not_satisfiable), 416);
	EXPECT_EQ(static_cast<int>(http_error_code::expectation_failed), 417);
	EXPECT_EQ(static_cast<int>(http_error_code::im_a_teapot), 418);
	EXPECT_EQ(static_cast<int>(http_error_code::misdirected_request), 421);
	EXPECT_EQ(static_cast<int>(http_error_code::unprocessable_entity), 422);
	EXPECT_EQ(static_cast<int>(http_error_code::locked), 423);
	EXPECT_EQ(static_cast<int>(http_error_code::failed_dependency), 424);
	EXPECT_EQ(static_cast<int>(http_error_code::too_early), 425);
	EXPECT_EQ(static_cast<int>(http_error_code::upgrade_required), 426);
	EXPECT_EQ(static_cast<int>(http_error_code::precondition_required), 428);
	EXPECT_EQ(static_cast<int>(http_error_code::too_many_requests), 429);
	EXPECT_EQ(static_cast<int>(http_error_code::request_header_fields_too_large), 431);
	EXPECT_EQ(static_cast<int>(http_error_code::unavailable_for_legal_reasons), 451);
}

TEST_F(HttpErrorCodeTest, ServerErrorCodesAreCorrect)
{
	EXPECT_EQ(static_cast<int>(http_error_code::internal_server_error), 500);
	EXPECT_EQ(static_cast<int>(http_error_code::not_implemented), 501);
	EXPECT_EQ(static_cast<int>(http_error_code::bad_gateway), 502);
	EXPECT_EQ(static_cast<int>(http_error_code::service_unavailable), 503);
	EXPECT_EQ(static_cast<int>(http_error_code::gateway_timeout), 504);
	EXPECT_EQ(static_cast<int>(http_error_code::http_version_not_supported), 505);
	EXPECT_EQ(static_cast<int>(http_error_code::variant_also_negotiates), 506);
	EXPECT_EQ(static_cast<int>(http_error_code::insufficient_storage), 507);
	EXPECT_EQ(static_cast<int>(http_error_code::loop_detected), 508);
	EXPECT_EQ(static_cast<int>(http_error_code::not_extended), 510);
	EXPECT_EQ(static_cast<int>(http_error_code::network_authentication_required), 511);
}

// ============================================================================
// HttpErrorStructTest
// ============================================================================

class HttpErrorStructTest : public ::testing::Test
{
};

TEST_F(HttpErrorStructTest, DefaultValues)
{
	http_error err;
	EXPECT_EQ(err.code, http_error_code::internal_server_error);
	EXPECT_TRUE(err.message.empty());
	EXPECT_TRUE(err.detail.empty());
	EXPECT_TRUE(err.request_id.empty());
	EXPECT_EQ(err.status_code(), 500);
}

TEST_F(HttpErrorStructTest, IsClientError)
{
	http_error err;

	err.code = http_error_code::bad_request;
	EXPECT_TRUE(err.is_client_error());
	EXPECT_FALSE(err.is_server_error());

	err.code = http_error_code::not_found;
	EXPECT_TRUE(err.is_client_error());
	EXPECT_FALSE(err.is_server_error());

	err.code = http_error_code::im_a_teapot;
	EXPECT_TRUE(err.is_client_error());
	EXPECT_FALSE(err.is_server_error());

	err.code = http_error_code::unavailable_for_legal_reasons;
	EXPECT_TRUE(err.is_client_error());
	EXPECT_FALSE(err.is_server_error());
}

TEST_F(HttpErrorStructTest, IsServerError)
{
	http_error err;

	err.code = http_error_code::internal_server_error;
	EXPECT_TRUE(err.is_server_error());
	EXPECT_FALSE(err.is_client_error());

	err.code = http_error_code::not_implemented;
	EXPECT_TRUE(err.is_server_error());
	EXPECT_FALSE(err.is_client_error());

	err.code = http_error_code::gateway_timeout;
	EXPECT_TRUE(err.is_server_error());
	EXPECT_FALSE(err.is_client_error());

	err.code = http_error_code::network_authentication_required;
	EXPECT_TRUE(err.is_server_error());
	EXPECT_FALSE(err.is_client_error());
}

TEST_F(HttpErrorStructTest, StatusCodeConversion)
{
	http_error err;
	err.code = http_error_code::not_found;
	EXPECT_EQ(err.status_code(), 404);

	err.code = http_error_code::service_unavailable;
	EXPECT_EQ(err.status_code(), 503);
}

// ============================================================================
// GetErrorStatusTextTest
// ============================================================================

class GetErrorStatusTextTest : public ::testing::Test
{
};

TEST_F(GetErrorStatusTextTest, AllClientErrorCodes)
{
	EXPECT_EQ(get_error_status_text(http_error_code::bad_request), "Bad Request");
	EXPECT_EQ(get_error_status_text(http_error_code::unauthorized), "Unauthorized");
	EXPECT_EQ(get_error_status_text(http_error_code::payment_required), "Payment Required");
	EXPECT_EQ(get_error_status_text(http_error_code::forbidden), "Forbidden");
	EXPECT_EQ(get_error_status_text(http_error_code::not_found), "Not Found");
	EXPECT_EQ(get_error_status_text(http_error_code::method_not_allowed), "Method Not Allowed");
	EXPECT_EQ(get_error_status_text(http_error_code::not_acceptable), "Not Acceptable");
	EXPECT_EQ(get_error_status_text(http_error_code::proxy_authentication_required),
			  "Proxy Authentication Required");
	EXPECT_EQ(get_error_status_text(http_error_code::request_timeout), "Request Timeout");
	EXPECT_EQ(get_error_status_text(http_error_code::conflict), "Conflict");
	EXPECT_EQ(get_error_status_text(http_error_code::gone), "Gone");
	EXPECT_EQ(get_error_status_text(http_error_code::length_required), "Length Required");
	EXPECT_EQ(get_error_status_text(http_error_code::precondition_failed), "Precondition Failed");
	EXPECT_EQ(get_error_status_text(http_error_code::payload_too_large), "Payload Too Large");
	EXPECT_EQ(get_error_status_text(http_error_code::uri_too_long), "URI Too Long");
	EXPECT_EQ(get_error_status_text(http_error_code::unsupported_media_type),
			  "Unsupported Media Type");
	EXPECT_EQ(get_error_status_text(http_error_code::range_not_satisfiable),
			  "Range Not Satisfiable");
	EXPECT_EQ(get_error_status_text(http_error_code::expectation_failed), "Expectation Failed");
	EXPECT_EQ(get_error_status_text(http_error_code::im_a_teapot), "I'm a teapot");
	EXPECT_EQ(get_error_status_text(http_error_code::misdirected_request), "Misdirected Request");
	EXPECT_EQ(get_error_status_text(http_error_code::unprocessable_entity),
			  "Unprocessable Entity");
	EXPECT_EQ(get_error_status_text(http_error_code::locked), "Locked");
	EXPECT_EQ(get_error_status_text(http_error_code::failed_dependency), "Failed Dependency");
	EXPECT_EQ(get_error_status_text(http_error_code::too_early), "Too Early");
	EXPECT_EQ(get_error_status_text(http_error_code::upgrade_required), "Upgrade Required");
	EXPECT_EQ(get_error_status_text(http_error_code::precondition_required),
			  "Precondition Required");
	EXPECT_EQ(get_error_status_text(http_error_code::too_many_requests), "Too Many Requests");
	EXPECT_EQ(get_error_status_text(http_error_code::request_header_fields_too_large),
			  "Request Header Fields Too Large");
	EXPECT_EQ(get_error_status_text(http_error_code::unavailable_for_legal_reasons),
			  "Unavailable For Legal Reasons");
}

TEST_F(GetErrorStatusTextTest, AllServerErrorCodes)
{
	EXPECT_EQ(get_error_status_text(http_error_code::internal_server_error),
			  "Internal Server Error");
	EXPECT_EQ(get_error_status_text(http_error_code::not_implemented), "Not Implemented");
	EXPECT_EQ(get_error_status_text(http_error_code::bad_gateway), "Bad Gateway");
	EXPECT_EQ(get_error_status_text(http_error_code::service_unavailable), "Service Unavailable");
	EXPECT_EQ(get_error_status_text(http_error_code::gateway_timeout), "Gateway Timeout");
	EXPECT_EQ(get_error_status_text(http_error_code::http_version_not_supported),
			  "HTTP Version Not Supported");
	EXPECT_EQ(get_error_status_text(http_error_code::variant_also_negotiates),
			  "Variant Also Negotiates");
	EXPECT_EQ(get_error_status_text(http_error_code::insufficient_storage),
			  "Insufficient Storage");
	EXPECT_EQ(get_error_status_text(http_error_code::loop_detected), "Loop Detected");
	EXPECT_EQ(get_error_status_text(http_error_code::not_extended), "Not Extended");
	EXPECT_EQ(get_error_status_text(http_error_code::network_authentication_required),
			  "Network Authentication Required");
}

TEST_F(GetErrorStatusTextTest, UnknownErrorCode)
{
	auto text = get_error_status_text(static_cast<http_error_code>(999));
	EXPECT_EQ(text, "Unknown Error");
}

// ============================================================================
// ParseErrorTest
// ============================================================================

class ParseErrorTest : public ::testing::Test
{
};

TEST_F(ParseErrorTest, DefaultValues)
{
	parse_error err;
	EXPECT_EQ(err.error_type, parse_error_type::malformed_request);
	EXPECT_EQ(err.line_number, 0u);
	EXPECT_EQ(err.column_number, 0u);
	EXPECT_TRUE(err.context.empty());
	EXPECT_TRUE(err.message.empty());
}

TEST_F(ParseErrorTest, ToHttpErrorBasic)
{
	parse_error perr;
	perr.message = "Invalid header format";

	auto herr = perr.to_http_error();
	EXPECT_EQ(herr.code, http_error_code::bad_request);
	EXPECT_EQ(herr.message, "Bad Request");
	EXPECT_EQ(herr.detail, "Invalid header format");
}

TEST_F(ParseErrorTest, ToHttpErrorWithContext)
{
	parse_error perr;
	perr.message = "Unexpected token";
	perr.context = "GET /index HTTP/1.x";

	auto herr = perr.to_http_error();
	EXPECT_EQ(herr.code, http_error_code::bad_request);
	EXPECT_EQ(herr.detail, "Unexpected token near: GET /index HTTP/1.x");
}

TEST_F(ParseErrorTest, ToHttpErrorEmptyContext)
{
	parse_error perr;
	perr.message = "Missing Content-Length";
	perr.context = "";

	auto herr = perr.to_http_error();
	EXPECT_EQ(herr.detail, "Missing Content-Length");
}

// ============================================================================
// HttpErrorResponseMakeErrorTest
// ============================================================================

class HttpErrorResponseMakeErrorTest : public ::testing::Test
{
};

TEST_F(HttpErrorResponseMakeErrorTest, BasicMakeError)
{
	auto err = http_error_response::make_error(http_error_code::not_found, "Resource missing");
	EXPECT_EQ(err.code, http_error_code::not_found);
	EXPECT_EQ(err.message, "Not Found");
	EXPECT_EQ(err.detail, "Resource missing");
	EXPECT_TRUE(err.request_id.empty());
}

TEST_F(HttpErrorResponseMakeErrorTest, MakeErrorWithRequestId)
{
	auto err = http_error_response::make_error(
		http_error_code::internal_server_error, "Database timeout", "req-12345");
	EXPECT_EQ(err.code, http_error_code::internal_server_error);
	EXPECT_EQ(err.message, "Internal Server Error");
	EXPECT_EQ(err.detail, "Database timeout");
	EXPECT_EQ(err.request_id, "req-12345");
}

TEST_F(HttpErrorResponseMakeErrorTest, MakeErrorEmptyDetail)
{
	auto err = http_error_response::make_error(http_error_code::bad_gateway);
	EXPECT_EQ(err.code, http_error_code::bad_gateway);
	EXPECT_EQ(err.message, "Bad Gateway");
	EXPECT_TRUE(err.detail.empty());
}
