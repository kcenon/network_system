/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#pragma once

#include "kcenon/network/utils/result_types.h"
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include <memory>
#include <string>

namespace kcenon::network::protocols::http2
{
    /*!
     * \enum frame_type
     * \brief HTTP/2 frame types (RFC 7540 Section 6)
     */
    enum class frame_type : uint8_t
    {
        data          = 0x0,  //!< DATA frame
        headers       = 0x1,  //!< HEADERS frame
        priority      = 0x2,  //!< PRIORITY frame
        rst_stream    = 0x3,  //!< RST_STREAM frame
        settings      = 0x4,  //!< SETTINGS frame
        push_promise  = 0x5,  //!< PUSH_PROMISE frame
        ping          = 0x6,  //!< PING frame
        goaway        = 0x7,  //!< GOAWAY frame
        window_update = 0x8,  //!< WINDOW_UPDATE frame
        continuation  = 0x9   //!< CONTINUATION frame
    };

    /*!
     * \enum frame_flags
     * \brief Common frame flags
     */
    namespace frame_flags
    {
        constexpr uint8_t none         = 0x0;
        constexpr uint8_t end_stream   = 0x1;  // DATA, HEADERS
        constexpr uint8_t ack          = 0x1;  // SETTINGS, PING
        constexpr uint8_t end_headers  = 0x4;  // HEADERS, PUSH_PROMISE, CONTINUATION
        constexpr uint8_t padded       = 0x8;  // DATA, HEADERS, PUSH_PROMISE
        constexpr uint8_t priority     = 0x20; // HEADERS
    }

    /*!
     * \struct frame_header
     * \brief HTTP/2 frame header (9 bytes)
     */
    struct frame_header
    {
        uint32_t length;     //!< Payload length (24 bits)
        frame_type type;     //!< Frame type
        uint8_t flags;       //!< Frame flags
        uint32_t stream_id;  //!< Stream identifier (31 bits, MSB reserved)

        /*!
         * \brief Parse frame header from raw bytes
         * \param data Raw byte data (at least 9 bytes)
         * \return Result containing parsed header or error
         */
        static auto parse(std::span<const uint8_t> data) -> Result<frame_header>;

        /*!
         * \brief Serialize frame header to bytes
         * \return 9-byte vector containing serialized header
         */
        auto serialize() const -> std::vector<uint8_t>;
    };

    /*!
     * \class frame
     * \brief Base class for HTTP/2 frames
     *
     * Represents a generic HTTP/2 frame with header and payload.
     * Specific frame types inherit from this base class.
     */
    class frame
    {
    public:
        /*!
         * \brief Default constructor
         */
        frame() = default;

        /*!
         * \brief Construct frame with header and payload
         * \param hdr Frame header
         * \param payload Frame payload data
         */
        frame(const frame_header& hdr, std::vector<uint8_t> payload);

        /*!
         * \brief Parse frame from raw bytes
         * \param data Raw byte data (header + payload)
         * \return Result containing parsed frame or error
         */
        static auto parse(std::span<const uint8_t> data) -> Result<std::unique_ptr<frame>>;

        /*!
         * \brief Serialize frame to bytes
         * \return Byte vector containing serialized frame (header + payload)
         */
        auto serialize() const -> std::vector<uint8_t>;

        /*!
         * \brief Get frame header
         * \return Const reference to frame header
         */
        auto header() const -> const frame_header&;

        /*!
         * \brief Get frame payload
         * \return Span of payload bytes
         */
        auto payload() const -> std::span<const uint8_t>;

        /*!
         * \brief Virtual destructor
         */
        virtual ~frame() = default;

    protected:
        frame_header header_;           //!< Frame header
        std::vector<uint8_t> payload_;  //!< Frame payload
    };

    /*!
     * \class data_frame
     * \brief DATA frame (RFC 7540 Section 6.1)
     *
     * DATA frames convey arbitrary, variable-length sequences of octets
     * associated with a stream.
     */
    class data_frame : public frame
    {
    public:
        /*!
         * \brief Construct DATA frame
         * \param stream_id Stream identifier
         * \param data Payload data
         * \param end_stream True if this is the last frame in stream
         * \param padded True if frame is padded
         */
        data_frame(uint32_t stream_id, std::vector<uint8_t> data,
                   bool end_stream = false, bool padded = false);

        /*!
         * \brief Parse DATA frame from raw bytes
         * \param hdr Frame header (must be DATA type)
         * \param payload Frame payload
         * \return Result containing parsed DATA frame or error
         */
        static auto parse(const frame_header& hdr, std::span<const uint8_t> payload)
            -> Result<std::unique_ptr<data_frame>>;

        /*!
         * \brief Check if END_STREAM flag is set
         * \return True if this is the last frame in stream
         */
        auto is_end_stream() const -> bool;

        /*!
         * \brief Check if frame is padded
         * \return True if frame has padding
         */
        auto is_padded() const -> bool;

        /*!
         * \brief Get actual data (without padding)
         * \return Span of data bytes
         */
        auto data() const -> std::span<const uint8_t>;

    private:
        std::vector<uint8_t> data_;  //!< Actual data without padding
    };

    /*!
     * \class headers_frame
     * \brief HEADERS frame (RFC 7540 Section 6.2)
     *
     * HEADERS frames are used to open a stream and carry a header block fragment.
     */
    class headers_frame : public frame
    {
    public:
        /*!
         * \brief Construct HEADERS frame
         * \param stream_id Stream identifier
         * \param header_block Header block fragment
         * \param end_stream True if this is the last frame in stream
         * \param end_headers True if this is the last header frame
         * \param priority_data Optional priority information
         */
        headers_frame(uint32_t stream_id, std::vector<uint8_t> header_block,
                      bool end_stream = false, bool end_headers = true);

        /*!
         * \brief Parse HEADERS frame from raw bytes
         * \param hdr Frame header (must be HEADERS type)
         * \param payload Frame payload
         * \return Result containing parsed HEADERS frame or error
         */
        static auto parse(const frame_header& hdr, std::span<const uint8_t> payload)
            -> Result<std::unique_ptr<headers_frame>>;

        /*!
         * \brief Check if END_STREAM flag is set
         * \return True if this is the last frame in stream
         */
        auto is_end_stream() const -> bool;

        /*!
         * \brief Check if END_HEADERS flag is set
         * \return True if this is the last header frame
         */
        auto is_end_headers() const -> bool;

        /*!
         * \brief Get header block fragment
         * \return Span of header block bytes
         */
        auto header_block() const -> std::span<const uint8_t>;

    private:
        std::vector<uint8_t> header_block_;  //!< Header block fragment
    };

    /*!
     * \struct setting_parameter
     * \brief SETTINGS frame parameter
     */
    struct setting_parameter
    {
        uint16_t identifier;  //!< Setting identifier
        uint32_t value;       //!< Setting value
    };

    /*!
     * \enum setting_identifier
     * \brief SETTINGS frame parameter identifiers (RFC 7540 Section 6.5.2)
     */
    enum class setting_identifier : uint16_t
    {
        header_table_size      = 0x1,  //!< SETTINGS_HEADER_TABLE_SIZE
        enable_push            = 0x2,  //!< SETTINGS_ENABLE_PUSH
        max_concurrent_streams = 0x3,  //!< SETTINGS_MAX_CONCURRENT_STREAMS
        initial_window_size    = 0x4,  //!< SETTINGS_INITIAL_WINDOW_SIZE
        max_frame_size         = 0x5,  //!< SETTINGS_MAX_FRAME_SIZE
        max_header_list_size   = 0x6   //!< SETTINGS_MAX_HEADER_LIST_SIZE
    };

    /*!
     * \class settings_frame
     * \brief SETTINGS frame (RFC 7540 Section 6.5)
     *
     * SETTINGS frames convey configuration parameters that affect how
     * endpoints communicate.
     */
    class settings_frame : public frame
    {
    public:
        /*!
         * \brief Construct SETTINGS frame
         * \param settings Vector of settings parameters
         * \param ack True if this is an acknowledgment
         */
        explicit settings_frame(std::vector<setting_parameter> settings = {},
                                bool ack = false);

        /*!
         * \brief Parse SETTINGS frame from raw bytes
         * \param hdr Frame header (must be SETTINGS type)
         * \param payload Frame payload
         * \return Result containing parsed SETTINGS frame or error
         */
        static auto parse(const frame_header& hdr, std::span<const uint8_t> payload)
            -> Result<std::unique_ptr<settings_frame>>;

        /*!
         * \brief Get settings parameters
         * \return Vector of settings
         */
        auto settings() const -> const std::vector<setting_parameter>&;

        /*!
         * \brief Check if this is an ACK frame
         * \return True if ACK flag is set
         */
        auto is_ack() const -> bool;

    private:
        std::vector<setting_parameter> settings_;  //!< Settings parameters
    };

    /*!
     * \class rst_stream_frame
     * \brief RST_STREAM frame (RFC 7540 Section 6.4)
     *
     * RST_STREAM frames allow for immediate termination of a stream.
     */
    class rst_stream_frame : public frame
    {
    public:
        /*!
         * \brief Construct RST_STREAM frame
         * \param stream_id Stream identifier to reset
         * \param error_code Error code
         */
        rst_stream_frame(uint32_t stream_id, uint32_t error_code);

        /*!
         * \brief Parse RST_STREAM frame from raw bytes
         * \param hdr Frame header (must be RST_STREAM type)
         * \param payload Frame payload
         * \return Result containing parsed RST_STREAM frame or error
         */
        static auto parse(const frame_header& hdr, std::span<const uint8_t> payload)
            -> Result<std::unique_ptr<rst_stream_frame>>;

        /*!
         * \brief Get error code
         * \return Error code
         */
        auto error_code() const -> uint32_t;

    private:
        uint32_t error_code_;  //!< Error code
    };

    /*!
     * \class ping_frame
     * \brief PING frame (RFC 7540 Section 6.7)
     *
     * PING frames are a mechanism for measuring a minimal round-trip time
     * from the sender and for determining whether an idle connection is
     * still functional.
     */
    class ping_frame : public frame
    {
    public:
        /*!
         * \brief Construct PING frame
         * \param opaque_data 8-byte opaque data
         * \param ack True if this is an acknowledgment
         */
        explicit ping_frame(std::array<uint8_t, 8> opaque_data = {},
                            bool ack = false);

        /*!
         * \brief Parse PING frame from raw bytes
         * \param hdr Frame header (must be PING type)
         * \param payload Frame payload
         * \return Result containing parsed PING frame or error
         */
        static auto parse(const frame_header& hdr, std::span<const uint8_t> payload)
            -> Result<std::unique_ptr<ping_frame>>;

        /*!
         * \brief Get opaque data
         * \return 8-byte opaque data
         */
        auto opaque_data() const -> const std::array<uint8_t, 8>&;

        /*!
         * \brief Check if this is an ACK frame
         * \return True if ACK flag is set
         */
        auto is_ack() const -> bool;

    private:
        std::array<uint8_t, 8> opaque_data_;  //!< 8-byte opaque data
    };

    /*!
     * \class goaway_frame
     * \brief GOAWAY frame (RFC 7540 Section 6.8)
     *
     * GOAWAY frames are used to initiate shutdown of a connection or to
     * signal serious error conditions.
     */
    class goaway_frame : public frame
    {
    public:
        /*!
         * \brief Construct GOAWAY frame
         * \param last_stream_id Last stream ID processed
         * \param error_code Error code
         * \param additional_data Optional additional debug data
         */
        goaway_frame(uint32_t last_stream_id, uint32_t error_code,
                     std::vector<uint8_t> additional_data = {});

        /*!
         * \brief Parse GOAWAY frame from raw bytes
         * \param hdr Frame header (must be GOAWAY type)
         * \param payload Frame payload
         * \return Result containing parsed GOAWAY frame or error
         */
        static auto parse(const frame_header& hdr, std::span<const uint8_t> payload)
            -> Result<std::unique_ptr<goaway_frame>>;

        /*!
         * \brief Get last stream ID
         * \return Last stream ID processed
         */
        auto last_stream_id() const -> uint32_t;

        /*!
         * \brief Get error code
         * \return Error code
         */
        auto error_code() const -> uint32_t;

        /*!
         * \brief Get additional debug data
         * \return Span of additional data
         */
        auto additional_data() const -> std::span<const uint8_t>;

    private:
        uint32_t last_stream_id_;           //!< Last stream ID
        uint32_t error_code_;               //!< Error code
        std::vector<uint8_t> additional_data_;  //!< Debug data
    };

    /*!
     * \class window_update_frame
     * \brief WINDOW_UPDATE frame (RFC 7540 Section 6.9)
     *
     * WINDOW_UPDATE frames are used to implement flow control.
     */
    class window_update_frame : public frame
    {
    public:
        /*!
         * \brief Construct WINDOW_UPDATE frame
         * \param stream_id Stream identifier (0 for connection)
         * \param window_size_increment Window size increment
         */
        window_update_frame(uint32_t stream_id, uint32_t window_size_increment);

        /*!
         * \brief Parse WINDOW_UPDATE frame from raw bytes
         * \param hdr Frame header (must be WINDOW_UPDATE type)
         * \param payload Frame payload
         * \return Result containing parsed WINDOW_UPDATE frame or error
         */
        static auto parse(const frame_header& hdr, std::span<const uint8_t> payload)
            -> Result<std::unique_ptr<window_update_frame>>;

        /*!
         * \brief Get window size increment
         * \return Window size increment value
         */
        auto window_size_increment() const -> uint32_t;

    private:
        uint32_t window_size_increment_;  //!< Window size increment
    };

    /*!
     * \enum error_code
     * \brief HTTP/2 error codes (RFC 7540 Section 7)
     */
    enum class error_code : uint32_t
    {
        no_error            = 0x0,  //!< Graceful shutdown
        protocol_error      = 0x1,  //!< Protocol error detected
        internal_error      = 0x2,  //!< Implementation fault
        flow_control_error  = 0x3,  //!< Flow-control limits exceeded
        settings_timeout    = 0x4,  //!< Settings not acknowledged
        stream_closed       = 0x5,  //!< Frame received for closed stream
        frame_size_error    = 0x6,  //!< Frame size incorrect
        refused_stream      = 0x7,  //!< Stream not processed
        cancel              = 0x8,  //!< Stream cancelled
        compression_error   = 0x9,  //!< Compression state not updated
        connect_error       = 0xa,  //!< TCP connection error for CONNECT
        enhance_your_calm   = 0xb,  //!< Processing capacity exceeded
        inadequate_security = 0xc,  //!< Negotiated TLS parameters not acceptable
        http_1_1_required   = 0xd   //!< Use HTTP/1.1 for the request
    };

} // namespace kcenon::network::protocols::http2
