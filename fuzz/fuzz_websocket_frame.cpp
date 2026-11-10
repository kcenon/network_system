// BSD 3-Clause License; see LICENSE in the project root.
#include "internal/websocket/websocket_frame.h"
#include "internal/websocket/websocket_protocol.h"

#include <algorithm>
#include <cstdlib>
#include <span>

using namespace kcenon::network::internal;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::vector<uint8_t> bytes(data, data + size);
    if (const auto header = websocket_frame::decode_header(bytes))
        (void)websocket_frame::decode_payload(*header, bytes);
    ws_header_error error;
    (void)websocket_frame::decode_header_checked(bytes, error, 4096);

    // Exercise both coalesced input and header/body splits using the real assembler.
    for (const size_t stride : {std::max(size, size_t{1}), size_t{7}})
    {
        websocket_protocol protocol(true, 4096);
        protocol.set_message_callback([](const ws_message& message) {
            if (message.data.size() > 4096) std::abort();
        });
        for (size_t offset = 0; offset < size; offset += std::min(stride, size - offset))
            protocol.process_data(std::span(data + offset, std::min(stride, size - offset)));
    }
    return 0;
}
