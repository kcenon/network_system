// BSD 3-Clause License; see LICENSE in the project root.
#include "internal/http/http_parser.h"

#include <cstddef>
#include <cstdint>

using namespace kcenon::network::internal;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    auto request = http_parser::parse_request(input);
    if (request.is_ok())
    {
        (void)http_parser::serialize_request(request.value());
        (void)http_parser::parse_multipart_form_data(request.value());
    }
    http_request multipart;
    multipart.headers["Content-Type"] = "multipart/form-data; boundary=fuzz";
    multipart.body.assign(data, data + size);
    (void)http_parser::parse_multipart_form_data(multipart);
    const auto response = http_parser::parse_response(input);
    if (response.is_ok()) (void)http_parser::serialize_response(response.value());
    (void)http_parser::url_decode(std::string(input));
    (void)http_parser::parse_query_string(std::string(input));
    return 0;
}
