// BSD 3-Clause License; see LICENSE in the project root.
#include "internal/websocket/websocket_handshake.h"

#include <cstddef>
#include <cstdint>

using namespace kcenon::network::internal;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string input(reinterpret_cast<const char*>(data), size);
    const auto request = websocket_handshake::parse_client_request(input);
    (void)websocket_handshake::validate_server_response(input, "dGhlIHNhbXBsZSBub25jZQ==");
    if (request.success)
    {
        const auto key = request.headers.find("sec-websocket-key");
        if (key != request.headers.end())
            (void)websocket_handshake::create_server_response(key->second);
    }
    return 0;
}
