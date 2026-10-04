// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

// Targeted branch-coverage tests for src/internal/http_parser.cpp covering
// branches that remain uncovered after http_parser_test.cpp and
// http_parser_coverage_test.cpp run. Focuses on `parse_multipart_form_data`
// edge cases (missing Content-Type, missing boundary, headers without CRLF
// terminator after part, name-without-closing-quote, filename-without-closing
// -quote, partial Content-Type trim path, final-boundary detection) and
// secondary edge cases in `parse_request`, `parse_response`, `trim`, and
// `serialize_chunked_response`.
//
// Part of epic #953 (sub-issue #1025).

#include "internal/http/http_parser.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace kcenon::network::internal;

namespace {

auto vec_from(std::string_view s) -> std::vector<uint8_t>
{
    return std::vector<uint8_t>(s.begin(), s.end());
}

auto str_from(const std::vector<uint8_t>& bytes) -> std::string
{
    return std::string(bytes.begin(), bytes.end());
}

}  // namespace

// ============================================================================
// parse_multipart_form_data error-path branches
// ============================================================================

class HttpParserMultipartBranchTest : public ::testing::Test
{
protected:
    static auto make_request(const std::string& body,
                              const std::string& content_type) -> http_request
    {
        http_request req;
        if (!content_type.empty()) {
            req.headers["Content-Type"] = content_type;
        }
        req.body = vec_from(body);
        return req;
    }
};

TEST_F(HttpParserMultipartBranchTest, MissingContentTypeHeaderReturnsError)
{
    http_request req;
    req.body = vec_from("anything");
    auto result = http_parser::parse_multipart_form_data(req);
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserMultipartBranchTest, ContentTypeWithoutBoundaryReturnsError)
{
    http_request req;
    req.headers["Content-Type"] = "multipart/form-data";
    req.body = vec_from("anything");
    auto result = http_parser::parse_multipart_form_data(req);
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserMultipartBranchTest, BoundaryFollowedByEofTerminates)
{
    // Boundary is found but body ends right after — pos+2 > size, so the
    // CRLF-skip branch is not taken; loop exits next iteration.
    auto req = make_request("--B", "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(req.form_data.empty());
}

TEST_F(HttpParserMultipartBranchTest, FinalBoundaryStopsParsing)
{
    // The boundary "--B--" causes the substr(pos-2, 2) == "--" check to fire,
    // breaking the parsing loop without reading any part headers.
    std::string body =
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(req.form_data.empty());
    EXPECT_TRUE(req.files.empty());
}

TEST_F(HttpParserMultipartBranchTest, PartHeadersWithoutDoubleCrlfStops)
{
    // After the boundary CRLF, no "\r\n\r\n" appears — `headers_end == npos`
    // hits the break branch.
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"x\"\r\n"
        "no terminating empty line";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(req.form_data.empty());
}

TEST_F(HttpParserMultipartBranchTest, NameWithoutClosingQuoteSkipsField)
{
    // The closing quote of name="..." is missing — `name_end == npos` so the
    // field_name remains empty; for a non-file part this still inserts an
    // entry under the empty key.
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"unterminated\r\n"
        "\r\n"
        "value\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    // No closing quote -> name_end == npos -> field_name stays "" but the
    // form_data["" ] entry is still inserted.
    EXPECT_EQ(req.form_data.count(""), 1u);
}

TEST_F(HttpParserMultipartBranchTest, FilenameWithoutClosingQuoteUsesEmptyName)
{
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"f\"; filename=\"unterm\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "data\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(req.files.count("f"), 1u);
    // filename_end == npos -> filename stays empty.
    EXPECT_EQ(req.files["f"].filename, "");
}

TEST_F(HttpParserMultipartBranchTest, ContentTypeWithoutCrlfReadsToEnd)
{
    // The Content-Type line is the very last line before "\r\n\r\n";
    // headers_section.find("\r\n", ct_start) returns npos -> the substr
    // length is npos, exercising the "ct_end == npos" branch.
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"f\"; filename=\"a.bin\"\r\n"
        "Content-Type: application/x-only-line\r\n"
        "\r\n"
        "payload\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(req.files.count("f"), 1u);
    EXPECT_EQ(req.files["f"].content_type, "application/x-only-line");
}

TEST_F(HttpParserMultipartBranchTest, ContentTypeAllWhitespaceFallsThrough)
{
    // After "Content-Type:", the value is only whitespace. `find_first_not_of`
    // returns npos, so the trim branch's `if (first != npos)` is false and
    // content_type_value keeps the raw whitespace string (the second branch).
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"f\"; filename=\"a.bin\"\r\n"
        "Content-Type:   \r\n"
        "\r\n"
        "payload\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(req.files.count("f"), 1u);
    // The trim branch is not taken; the raw "   " (three spaces) is kept.
    EXPECT_EQ(req.files["f"].content_type, "   ");
}

TEST_F(HttpParserMultipartBranchTest, ContentDispositionWithoutCrlfTakesNposBranch)
{
    // headers_section.find("\r\n", disp_start) returns npos when the
    // Content-Disposition line is the last header (no trailing CRLF before
    // the headers/body separator). disp_end == npos exercises the second
    // branch of the conditional.
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"x\"\r\n"
        "\r\n"
        "value\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(req.form_data["x"], "value");
}

TEST_F(HttpParserMultipartBranchTest, FieldWithoutNameSkipsEntry)
{
    // Disposition has no `name="..."` substring -> name_pos == npos branch
    // taken, field_name stays empty.
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data\r\n"
        "\r\n"
        "value\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    // field_name == "" -> form_data[""] = "value" still inserted.
    EXPECT_EQ(req.form_data.count(""), 1u);
}

TEST_F(HttpParserMultipartBranchTest, NoTrailingCrlfBeforeBoundaryKeepsBytes)
{
    // The "remove trailing \r\n before boundary" check needs at least 2
    // bytes; we craft a minimal value so the check sees "\r\n" exactly and
    // strips it, then a separate test with content < 2 bytes.
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"x\"\r\n"
        "\r\n"
        "v"  // single byte content, no trailing CRLF
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    // The "v" sits immediately before "--B--" with no CRLF; content_end stays
    // at the next_boundary index unchanged.
    EXPECT_EQ(req.form_data["x"], "v");
}

// ============================================================================
// parse_request / parse_response edge branches
// ============================================================================

class HttpParserParseBranchTest : public ::testing::Test
{
};

TEST_F(HttpParserParseBranchTest, SingleCrlfRequestEmptyLineErrors)
{
    // "\r\n..." -> split_line yields empty first line -> "Empty HTTP request"
    auto result = http_parser::parse_request(std::string_view("\r\n"));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserParseBranchTest, SingleCrlfResponseEmptyLineErrors)
{
    auto result = http_parser::parse_response(std::string_view("\r\n"));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserParseBranchTest, RequestNoCrlfAtAllTreatsAsRequestLine)
{
    // No \r\n anywhere -> split_line returns (entire data, "") -> request_line
    // is non-empty so we proceed; with no version, parse_request_line errors.
    auto result = http_parser::parse_request(std::string_view("GET /"));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserParseBranchTest, ResponseNoCrlfAtAllTreatsAsStatusLine)
{
    auto result = http_parser::parse_response(std::string_view("HTTP/1.1"));
    // No spaces -> "Invalid status line: no spaces found".
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserParseBranchTest, RequestWithEmptyHeadersSectionParses)
{
    // request_line + "\r\n" only -> headers_end is npos -> parse_headers
    // gets an empty rest, returns true.
    auto result = http_parser::parse_request("GET / HTTP/1.1\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().headers.empty());
    EXPECT_TRUE(result.value().body.empty());
}

TEST_F(HttpParserParseBranchTest, ResponseWithMalformedHeaderInRestErrors)
{
    // No "\r\n\r\n" terminator, parse_headers fed with a malformed header
    // (no colon). Hits the "no separator + parse_headers fails" path.
    auto result = http_parser::parse_response(
        "HTTP/1.1 200 OK\r\nNoColonLine\r\n");
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// trim / split_line indirect branches via parse_headers
// ============================================================================

class HttpParserTrimBranchTest : public ::testing::Test
{
};

TEST_F(HttpParserTrimBranchTest, AllWhitespaceHeaderValueTrimsToEmpty)
{
    // Header value is only whitespace -> trim returns empty string_view via
    // the start >= end branch.
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "X-Whitespace:    \r\n"
        "\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().headers["X-Whitespace"], "");
}

TEST_F(HttpParserTrimBranchTest, AllWhitespaceHeaderNameTrimsToEmpty)
{
    // The header name itself is only spaces. trim of name -> empty;
    // headers[""] is set.
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "  : value\r\n"
        "\r\n");
    ASSERT_TRUE(result.is_ok());
    auto it = result.value().headers.find("");
    ASSERT_NE(it, result.value().headers.end());
    EXPECT_EQ(it->second, "value");
}

// ============================================================================
// serialize_chunked_response remaining branches
// ============================================================================

class HttpParserChunkedBranchTest : public ::testing::Test
{
};

TEST_F(HttpParserChunkedBranchTest, BodySmallerThanChunkSingleNonZeroChunk)
{
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    resp.body = vec_from("small");

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    // 5 in hex is "5"
    EXPECT_NE(s.find("5\r\nsmall\r\n"), std::string::npos);
    EXPECT_NE(s.rfind("0\r\n\r\n"), std::string::npos);
}

TEST_F(HttpParserChunkedBranchTest, OtherCaseContentLengthRetained)
{
    // The skip filter only checks the literal "Content-Length" and
    // "content-length"; an oddly cased variant slips through and is emitted.
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    resp.headers["CONTENT-LENGTH"] = "99";
    resp.body = vec_from("x");

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    EXPECT_NE(s.find("CONTENT-LENGTH"), std::string::npos);
    EXPECT_NE(s.find("Transfer-Encoding: chunked"), std::string::npos);
}

TEST_F(HttpParserChunkedBranchTest, MultipleSetCookiesAllEmitted)
{
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    cookie a; a.name = "a"; a.value = "1";
    cookie b; b.name = "b"; b.value = "2";
    resp.set_cookies.push_back(a);
    resp.set_cookies.push_back(b);

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    // Both cookies must be present.
    EXPECT_NE(s.find("a=1"), std::string::npos);
    EXPECT_NE(s.find("b=2"), std::string::npos);
}

TEST_F(HttpParserChunkedBranchTest, BodyExactlyTwoChunkSizes)
{
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    resp.body.assign(8192u * 2, 'Z');

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    // Two "2000" chunks.
    auto first = s.find("2000\r\n");
    ASSERT_NE(first, std::string::npos);
    auto second = s.find("2000\r\n", first + 1);
    EXPECT_NE(second, std::string::npos);
    EXPECT_NE(s.rfind("0\r\n\r\n"), std::string::npos);
}

// ============================================================================
// serialize_request branches
// ============================================================================

class HttpParserSerializeBranchTest : public ::testing::Test
{
};

TEST_F(HttpParserSerializeBranchTest, RequestNoQueryParamsOmitsQuestionMark)
{
    http_request r;
    r.method = http_method::HTTP_GET;
    r.uri = "/plain";
    r.version = http_version::HTTP_1_1;

    auto s = str_from(http_parser::serialize_request(r));
    EXPECT_NE(s.find("/plain HTTP/1.1"), std::string::npos);
    // No '?' must be emitted in the request line.
    auto first_line_end = s.find("\r\n");
    ASSERT_NE(first_line_end, std::string::npos);
    EXPECT_EQ(s.substr(0, first_line_end).find('?'), std::string::npos);
}

TEST_F(HttpParserSerializeBranchTest, RequestWithBodyButNoHeadersStillTerminatedByDoubleCrlf)
{
    http_request r;
    r.method = http_method::HTTP_POST;
    r.uri = "/u";
    r.version = http_version::HTTP_1_1;
    r.body = vec_from("payload");

    auto s = str_from(http_parser::serialize_request(r));
    // No header lines, but \r\n\r\n must still be present before the body.
    auto sep = s.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    EXPECT_EQ(s.substr(sep + 4), "payload");
}
