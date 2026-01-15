/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "kcenon/network/protocols/quic/connection.h"
#include "kcenon/network/protocols/quic/varint.h"

#include "kcenon/network/tracing/span.h"
#include "kcenon/network/tracing/trace_context.h"
#include "kcenon/network/tracing/tracing_config.h"

#include <algorithm>

namespace kcenon::network::protocols::quic
{

// ============================================================================
// String Conversion Functions
// ============================================================================

auto connection_state_to_string(connection_state state) -> const char*
{
    switch (state)
    {
    case connection_state::idle:
        return "idle";
    case connection_state::handshaking:
        return "handshaking";
    case connection_state::connected:
        return "connected";
    case connection_state::closing:
        return "closing";
    case connection_state::draining:
        return "draining";
    case connection_state::closed:
        return "closed";
    default:
        return "unknown";
    }
}

auto handshake_state_to_string(handshake_state state) -> const char*
{
    switch (state)
    {
    case handshake_state::initial:
        return "initial";
    case handshake_state::waiting_server_hello:
        return "waiting_server_hello";
    case handshake_state::waiting_finished:
        return "waiting_finished";
    case handshake_state::complete:
        return "complete";
    default:
        return "unknown";
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

connection::connection(bool is_server, const connection_id& initial_dcid)
    : is_server_(is_server)
    , initial_dcid_(initial_dcid)
    , peer_cid_manager_(8)  // Default active_connection_id_limit
    , stream_mgr_(is_server)
    , flow_ctrl_(1048576)  // 1MB default
    , loss_detector_(rtt_estimator_)
{
    // Generate local connection ID
    local_cid_ = connection_id::generate();
    local_cids_.emplace_back(0, local_cid_);

    // For client: remote CID is the initial DCID
    // For server: remote CID will be set when we receive client's SCID
    if (!is_server)
    {
        remote_cid_ = initial_dcid;
        peer_cid_manager_.set_initial_peer_cid(initial_dcid);
    }

    // Initialize default transport parameters
    if (is_server)
    {
        local_params_ = make_default_server_params();
    }
    else
    {
        local_params_ = make_default_client_params();
    }

    // Set initial source connection ID
    local_params_.initial_source_connection_id = local_cid_;

    // Initialize idle timer
    reset_idle_timer();
}

connection::~connection() = default;

// Move operations are deleted because some members are not movable

// ============================================================================
// Connection IDs
// ============================================================================

auto connection::add_local_cid(const connection_id& cid, uint64_t sequence)
    -> VoidResult
{
    // Check for duplicates
    for (const auto& [seq, existing_cid] : local_cids_)
    {
        if (seq == sequence)
        {
            return error_void(connection_error::protocol_violation,
                              "Duplicate connection ID sequence",
                              "connection");
        }
    }

    local_cids_.emplace_back(sequence, cid);
    return ok();
}

auto connection::retire_cid(uint64_t sequence) -> VoidResult
{
    auto it = std::find_if(local_cids_.begin(), local_cids_.end(),
                           [sequence](const auto& p) { return p.first == sequence; });

    if (it == local_cids_.end())
    {
        return error_void(connection_error::protocol_violation,
                          "Connection ID not found",
                          "connection");
    }

    // Don't retire the only remaining CID
    if (local_cids_.size() <= 1)
    {
        return error_void(connection_error::protocol_violation,
                          "Cannot retire last connection ID",
                          "connection");
    }

    local_cids_.erase(it);
    return ok();
}

// ============================================================================
// Transport Parameters
// ============================================================================

void connection::set_local_params(const transport_parameters& params)
{
    local_params_ = params;
    local_params_.initial_source_connection_id = local_cid_;

    // Apply to subsystems
    stream_mgr_.set_local_max_streams_bidi(params.initial_max_streams_bidi);
    stream_mgr_.set_local_max_streams_uni(params.initial_max_streams_uni);
}

void connection::set_remote_params(const transport_parameters& params)
{
    remote_params_ = params;
    apply_remote_params();
}

void connection::apply_remote_params()
{
    // Apply to flow control
    flow_ctrl_.update_send_limit(remote_params_.initial_max_data);

    // Apply to stream manager
    stream_mgr_.set_peer_max_streams_bidi(remote_params_.initial_max_streams_bidi);
    stream_mgr_.set_peer_max_streams_uni(remote_params_.initial_max_streams_uni);

    // Apply to peer CID manager
    peer_cid_manager_.set_active_cid_limit(remote_params_.active_connection_id_limit);

    // Update idle timeout
    if (remote_params_.max_idle_timeout > 0)
    {
        auto timeout = std::min(local_params_.max_idle_timeout,
                                remote_params_.max_idle_timeout);
        if (timeout > 0)
        {
            idle_deadline_ = std::chrono::steady_clock::now() +
                             std::chrono::milliseconds(timeout);
        }
    }
}

// ============================================================================
// Handshake
// ============================================================================

auto connection::start_handshake(const std::string& server_name)
    -> Result<std::vector<uint8_t>>
{
    auto span = tracing::is_tracing_enabled()
        ? std::make_optional(tracing::trace_context::create_span("quic.connection.start_handshake"))
        : std::nullopt;
    if (span)
    {
        span->set_attribute("quic.role", "client")
             .set_attribute("quic.server_name", server_name)
             .set_attribute("net.transport", "udp")
             .set_attribute("quic.version", "1");
    }

    if (is_server_)
    {
        if (span)
        {
            span->set_error("Server cannot start handshake");
        }
        return error<std::vector<uint8_t>>(connection_error::invalid_state,
                                            "Server cannot start handshake",
                                            "connection");
    }

    if (state_ != connection_state::idle)
    {
        if (span)
        {
            span->set_error("Connection already started");
        }
        return error<std::vector<uint8_t>>(connection_error::invalid_state,
                                            "Connection already started",
                                            "connection");
    }

    // Initialize crypto for client
    auto init_result = crypto_.init_client(server_name);
    if (init_result.is_err())
    {
        if (span)
        {
            span->set_error(init_result.error().message);
        }
        return error<std::vector<uint8_t>>(connection_error::handshake_failed,
                                            "Failed to initialize crypto",
                                            "connection",
                                            init_result.error().message);
    }

    // Derive initial secrets
    auto derive_result = crypto_.derive_initial_secrets(initial_dcid_);
    if (derive_result.is_err())
    {
        if (span)
        {
            span->set_error(derive_result.error().message);
        }
        return error<std::vector<uint8_t>>(connection_error::handshake_failed,
                                            "Failed to derive initial secrets",
                                            "connection",
                                            derive_result.error().message);
    }

    // Start TLS handshake to get ClientHello
    auto hs_result = crypto_.start_handshake();
    if (hs_result.is_err())
    {
        if (span)
        {
            span->set_error(hs_result.error().message);
        }
        return error<std::vector<uint8_t>>(connection_error::handshake_failed,
                                            "Failed to start TLS handshake",
                                            "connection",
                                            hs_result.error().message);
    }

    // Store crypto data for Initial packet
    if (!hs_result.value().empty())
    {
        pending_crypto_initial_.push_back(std::move(hs_result.value()));
    }

    // Update state
    state_ = connection_state::handshaking;
    hs_state_ = handshake_state::waiting_server_hello;

    if (span)
    {
        span->set_attribute("quic.state", "handshaking");
    }

    return ok(std::vector<uint8_t>{});
}

auto connection::init_server_handshake(const std::string& cert_file,
                                         const std::string& key_file) -> VoidResult
{
    auto span = tracing::is_tracing_enabled()
        ? std::make_optional(tracing::trace_context::create_span("quic.connection.init_server_handshake"))
        : std::nullopt;
    if (span)
    {
        span->set_attribute("quic.role", "server")
             .set_attribute("net.transport", "udp")
             .set_attribute("quic.version", "1");
    }

    if (!is_server_)
    {
        if (span)
        {
            span->set_error("Not a server connection");
        }
        return error_void(connection_error::invalid_state,
                          "Not a server connection",
                          "connection");
    }

    auto result = crypto_.init_server(cert_file, key_file);
    if (result.is_err())
    {
        if (span)
        {
            span->set_error(result.error().message);
        }
        return error_void(connection_error::handshake_failed,
                          "Failed to initialize server crypto",
                          "connection",
                          result.error().message);
    }

    // Derive initial secrets using the client's DCID
    auto derive_result = crypto_.derive_initial_secrets(initial_dcid_);
    if (derive_result.is_err())
    {
        if (span)
        {
            span->set_error(derive_result.error().message);
        }
        return error_void(connection_error::handshake_failed,
                          "Failed to derive initial secrets",
                          "connection",
                          derive_result.error().message);
    }

    state_ = connection_state::handshaking;
    if (span)
    {
        span->set_attribute("quic.state", "handshaking");
    }
    return ok();
}

// ============================================================================
// Packet Processing
// ============================================================================

auto connection::receive_packet(std::span<const uint8_t> data) -> VoidResult
{
    auto span = tracing::is_tracing_enabled()
        ? std::make_optional(tracing::trace_context::create_span("quic.connection.receive_packet"))
        : std::nullopt;
    if (span)
    {
        span->set_attribute("quic.packet.size", static_cast<int64_t>(data.size()))
             .set_attribute("quic.role", is_server_ ? "server" : "client")
             .set_attribute("quic.state", connection_state_to_string(state_));
    }

    if (data.empty())
    {
        if (span)
        {
            span->set_error("Empty packet");
        }
        return error_void(connection_error::protocol_violation,
                          "Empty packet",
                          "connection");
    }

    if (is_draining() || is_closed())
    {
        // In draining/closed state, just count the packet
        packets_received_++;
        bytes_received_ += data.size();
        if (span)
        {
            span->set_attribute("quic.packet.ignored", true);
        }
        return ok();
    }

    bytes_received_ += data.size();
    packets_received_++;
    reset_idle_timer();

    // Check header form
    if (packet_parser::is_long_header(data[0]))
    {
        auto parse_result = packet_parser::parse_long_header(data);
        if (parse_result.is_err())
        {
            return error_void(connection_error::protocol_violation,
                              "Failed to parse long header",
                              "connection",
                              parse_result.error().message);
        }

        auto [header, header_len] = parse_result.value();
        return process_long_header_packet(header, data.subspan(header_len));
    }
    else
    {
        auto parse_result = packet_parser::parse_short_header(data, local_cid_.length());
        if (parse_result.is_err())
        {
            return error_void(connection_error::protocol_violation,
                              "Failed to parse short header",
                              "connection",
                              parse_result.error().message);
        }

        auto [header, header_len] = parse_result.value();
        return process_short_header_packet(header, data.subspan(header_len));
    }
}

auto connection::process_long_header_packet(const long_header& hdr,
                                             std::span<const uint8_t> payload)
    -> VoidResult
{
    encryption_level level;
    switch (hdr.type())
    {
    case packet_type::initial:
        level = encryption_level::initial;
        pending_ack_initial_ = true;
        break;
    case packet_type::handshake:
        level = encryption_level::handshake;
        pending_ack_handshake_ = true;
        break;
    case packet_type::zero_rtt:
        level = encryption_level::zero_rtt;
        break;
    case packet_type::retry:
        // Handle Retry packet specially
        return ok();
    default:
        return error_void(connection_error::protocol_violation,
                          "Unknown packet type",
                          "connection");
    }

    // Update remote CID if this is first packet from peer
    if (remote_cid_.length() == 0)
    {
        remote_cid_ = hdr.src_conn_id;
        peer_cid_manager_.set_initial_peer_cid(hdr.src_conn_id);
    }

    // Server: set original DCID for transport parameters
    if (is_server_ && !local_params_.original_destination_connection_id)
    {
        local_params_.original_destination_connection_id = hdr.dest_conn_id;
    }

    // Get keys for this level
    auto keys_result = crypto_.get_read_keys(level);
    if (keys_result.is_err())
    {
        // Keys not yet available - this might happen during handshake
        return ok();
    }

    // Track largest packet number
    auto& space = get_pn_space(level);
    if (hdr.packet_number > space.largest_received)
    {
        space.largest_received = hdr.packet_number;
        space.largest_received_time = std::chrono::steady_clock::now();
    }
    space.ack_needed = true;

    return process_frames(payload, level);
}

auto connection::process_short_header_packet(const short_header& hdr,
                                              std::span<const uint8_t> payload)
    -> VoidResult
{
    if (state_ != connection_state::connected)
    {
        // Short header packets require completed handshake
        return error_void(connection_error::not_established,
                          "Received 1-RTT packet before handshake complete",
                          "connection");
    }

    pending_ack_app_ = true;

    // Get 1-RTT keys
    auto keys_result = crypto_.get_read_keys(encryption_level::application);
    if (keys_result.is_err())
    {
        return error_void(connection_error::handshake_failed,
                          "1-RTT keys not available",
                          "connection");
    }

    // Track largest packet number
    auto& space = app_space_;
    if (hdr.packet_number > space.largest_received)
    {
        space.largest_received = hdr.packet_number;
        space.largest_received_time = std::chrono::steady_clock::now();
    }
    space.ack_needed = true;

    return process_frames(payload, encryption_level::application);
}

auto connection::process_frames(std::span<const uint8_t> payload,
                                 encryption_level level) -> VoidResult
{
    // Parse all frames in the payload
    auto frames_result = frame_parser::parse_all(payload);
    if (frames_result.is_err())
    {
        return error_void(connection_error::protocol_violation,
                          "Failed to parse frames",
                          "connection",
                          frames_result.error().message);
    }

    // Process each frame
    for (const auto& f : frames_result.value())
    {
        auto result = handle_frame(f, level);
        if (result.is_err())
        {
            return result;
        }
    }

    update_state();
    return ok();
}

auto connection::handle_frame(const frame& frm, encryption_level level)
    -> VoidResult
{
    return std::visit(
        [this, level](const auto& f) -> VoidResult
        {
            using T = std::decay_t<decltype(f)>;

            if constexpr (std::is_same_v<T, padding_frame>)
            {
                // Padding is ignored
                return ok();
            }
            else if constexpr (std::is_same_v<T, ping_frame>)
            {
                // Ping just elicits an ACK (already handled)
                return ok();
            }
            else if constexpr (std::is_same_v<T, ack_frame>)
            {
                // Process ACK using loss detector (RFC 9002)
                auto recv_time = std::chrono::steady_clock::now();
                auto result = loss_detector_.on_ack_received(f, level, recv_time);

                // Handle loss detection result
                handle_loss_detection_result(result);

                // Reset PTO count on receiving acknowledgment
                loss_detector_.reset_pto_count();

                // Update handshake confirmation for loss detector
                if (crypto_.is_handshake_complete())
                {
                    loss_detector_.set_handshake_confirmed(true);
                }

                // Also update our local packet number space tracking
                auto& space = get_pn_space(level);
                if (f.largest_acknowledged > space.largest_acked)
                {
                    space.largest_acked = f.largest_acknowledged;
                }

                // Remove acknowledged packets from local tracking
                for (auto it = space.sent_packets.begin();
                     it != space.sent_packets.end();)
                {
                    if (it->first <= f.largest_acknowledged)
                    {
                        it = space.sent_packets.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                return ok();
            }
            else if constexpr (std::is_same_v<T, crypto_frame>)
            {
                // Process CRYPTO frame
                auto result = crypto_.process_crypto_data(level,
                    std::span<const uint8_t>(f.data.data(), f.data.size()));
                if (result.is_err())
                {
                    return error_void(connection_error::handshake_failed,
                                      "Failed to process crypto data",
                                      "connection",
                                      result.error().message);
                }

                // Store response crypto data
                if (!result.value().empty())
                {
                    switch (level)
                    {
                    case encryption_level::initial:
                        pending_crypto_initial_.push_back(std::move(result.value()));
                        break;
                    case encryption_level::handshake:
                        pending_crypto_handshake_.push_back(std::move(result.value()));
                        break;
                    default:
                        pending_crypto_app_.push_back(std::move(result.value()));
                        break;
                    }
                }

                return ok();
            }
            else if constexpr (std::is_same_v<T, stream_frame>)
            {
                // Get or create stream
                auto stream_result = stream_mgr_.get_or_create_stream(f.stream_id);
                if (stream_result.is_err())
                {
                    return error_void(connection_error::protocol_violation,
                                      "Failed to get stream",
                                      "connection",
                                      stream_result.error().message);
                }

                // Deliver data
                auto recv_result = stream_result.value()->receive_data(
                    f.offset,
                    std::span<const uint8_t>(f.data.data(), f.data.size()),
                    f.fin);
                if (recv_result.is_err())
                {
                    return recv_result;
                }

                // Update connection-level flow control
                auto recv_flow = flow_ctrl_.record_received(f.data.size());
                if (recv_flow.is_err())
                {
                    return recv_flow;
                }
                return ok();
            }
            else if constexpr (std::is_same_v<T, max_data_frame>)
            {
                flow_ctrl_.update_send_limit(f.maximum_data);
                return ok();
            }
            else if constexpr (std::is_same_v<T, max_stream_data_frame>)
            {
                auto* s = stream_mgr_.get_stream(f.stream_id);
                if (s)
                {
                    s->set_max_send_data(f.maximum_stream_data);
                }
                return ok();
            }
            else if constexpr (std::is_same_v<T, max_streams_frame>)
            {
                if (f.bidirectional)
                {
                    stream_mgr_.set_peer_max_streams_bidi(f.maximum_streams);
                }
                else
                {
                    stream_mgr_.set_peer_max_streams_uni(f.maximum_streams);
                }
                return ok();
            }
            else if constexpr (std::is_same_v<T, connection_close_frame>)
            {
                close_error_code_ = f.error_code;
                close_reason_ = f.reason_phrase;
                close_received_ = true;
                application_close_ = f.is_application_error;
                enter_draining();
                return ok();
            }
            else if constexpr (std::is_same_v<T, handshake_done_frame>)
            {
                if (!is_server_)
                {
                    hs_state_ = handshake_state::complete;
                    state_ = connection_state::connected;
                }
                return ok();
            }
            else if constexpr (std::is_same_v<T, reset_stream_frame>)
            {
                auto* s = stream_mgr_.get_stream(f.stream_id);
                if (s)
                {
                    return s->receive_reset(f.application_error_code, f.final_size);
                }
                return ok();
            }
            else if constexpr (std::is_same_v<T, stop_sending_frame>)
            {
                auto* s = stream_mgr_.get_stream(f.stream_id);
                if (s)
                {
                    return s->receive_stop_sending(f.application_error_code);
                }
                return ok();
            }
            else if constexpr (std::is_same_v<T, new_connection_id_frame>)
            {
                // RFC 9000 Section 19.15: Process NEW_CONNECTION_ID frame
                // Store the new peer connection ID with its metadata
                connection_id cid(std::span<const uint8_t>(
                    f.connection_id.data(), f.connection_id.size()));

                auto result = peer_cid_manager_.add_peer_cid(
                    cid,
                    f.sequence_number,
                    f.retire_prior_to,
                    f.stateless_reset_token);

                if (result.is_err())
                {
                    return error_void(connection_error::protocol_violation,
                                      "Failed to process NEW_CONNECTION_ID",
                                      "connection",
                                      result.error().message);
                }
                return ok();
            }
            else if constexpr (std::is_same_v<T, retire_connection_id_frame>)
            {
                return retire_cid(f.sequence_number);
            }
            else if constexpr (std::is_same_v<T, data_blocked_frame> ||
                              std::is_same_v<T, stream_data_blocked_frame> ||
                              std::is_same_v<T, streams_blocked_frame> ||
                              std::is_same_v<T, path_challenge_frame> ||
                              std::is_same_v<T, path_response_frame> ||
                              std::is_same_v<T, new_token_frame>)
            {
                // These frames are acknowledged but may not require action
                return ok();
            }
            else
            {
                return ok();
            }
        },
        frm);
}

// ============================================================================
// Packet Generation
// ============================================================================

auto connection::generate_packets() -> std::vector<std::vector<uint8_t>>
{
    std::vector<std::vector<uint8_t>> packets;

    if (is_closed())
    {
        return packets;
    }

    // Generate close packet if closing
    if (close_sent_ && !close_received_)
    {
        auto pkt = build_packet(encryption_level::application);
        if (!pkt.empty())
        {
            packets.push_back(std::move(pkt));
        }
        return packets;
    }

    // Generate Initial packets if needed
    if (!pending_crypto_initial_.empty() || pending_ack_initial_)
    {
        auto pkt = build_packet(encryption_level::initial);
        if (!pkt.empty())
        {
            packets.push_back(std::move(pkt));
            pending_ack_initial_ = false;
        }
    }

    // Generate Handshake packets if needed
    if (!pending_crypto_handshake_.empty() || pending_ack_handshake_)
    {
        auto pkt = build_packet(encryption_level::handshake);
        if (!pkt.empty())
        {
            packets.push_back(std::move(pkt));
            pending_ack_handshake_ = false;
        }
    }

    // Generate 1-RTT packets if connected
    if (state_ == connection_state::connected)
    {
        // Send HANDSHAKE_DONE (server only, once)
        if (is_server_ && hs_state_ == handshake_state::complete)
        {
            // Will be included in next packet
        }

        // Generate application data packets
        if (has_pending_data() || pending_ack_app_)
        {
            auto pkt = build_packet(encryption_level::application);
            if (!pkt.empty())
            {
                packets.push_back(std::move(pkt));
                pending_ack_app_ = false;
            }
        }
    }

    return packets;
}

auto connection::build_packet(encryption_level level) -> std::vector<uint8_t>
{
    std::vector<uint8_t> packet;

    // Get keys for this level
    auto keys_result = crypto_.get_write_keys(level);
    if (keys_result.is_err())
    {
        return packet;
    }

    auto& space = get_pn_space(level);
    uint64_t pn = space.next_pn++;

    // Build header
    std::vector<uint8_t> header;
    switch (level)
    {
    case encryption_level::initial:
        header = packet_builder::build_initial(
            remote_cid_, local_cid_, {}, pn);
        break;
    case encryption_level::handshake:
        header = packet_builder::build_handshake(
            remote_cid_, local_cid_, pn);
        break;
    case encryption_level::application:
        header = packet_builder::build_short(
            remote_cid_, pn, crypto_.key_phase());
        break;
    default:
        return packet;
    }

    // Build payload with frames
    std::vector<uint8_t> payload;

    // Add ACK frame if needed
    if (space.ack_needed)
    {
        auto ack = generate_ack_frame(space);
        if (ack)
        {
            auto encoded = frame_builder::build_ack(*ack);
            payload.insert(payload.end(), encoded.begin(), encoded.end());
            space.ack_needed = false;
        }
    }

    // Add CRYPTO frames
    std::deque<std::vector<uint8_t>>* crypto_queue = nullptr;
    switch (level)
    {
    case encryption_level::initial:
        crypto_queue = &pending_crypto_initial_;
        break;
    case encryption_level::handshake:
        crypto_queue = &pending_crypto_handshake_;
        break;
    default:
        crypto_queue = &pending_crypto_app_;
        break;
    }

    uint64_t crypto_offset = 0;
    while (!crypto_queue->empty())
    {
        auto& data = crypto_queue->front();
        crypto_frame cf;
        cf.offset = crypto_offset;
        cf.data = std::move(data);
        crypto_offset += cf.data.size();

        auto encoded = frame_builder::build_crypto(cf);
        payload.insert(payload.end(), encoded.begin(), encoded.end());
        crypto_queue->pop_front();
    }

    // Add HANDSHAKE_DONE for server
    if (level == encryption_level::application && is_server_ &&
        hs_state_ == handshake_state::complete && state_ == connection_state::connected)
    {
        handshake_done_frame hd;
        auto encoded = frame_builder::build(hd);
        payload.insert(payload.end(), encoded.begin(), encoded.end());
    }

    // Add CONNECTION_CLOSE if closing
    if (close_sent_)
    {
        connection_close_frame ccf;
        ccf.error_code = close_error_code_.value_or(0);
        ccf.reason_phrase = close_reason_;
        ccf.is_application_error = application_close_;
        auto encoded = frame_builder::build(ccf);
        payload.insert(payload.end(), encoded.begin(), encoded.end());
    }

    // Add RETIRE_CONNECTION_ID frames for application level
    if (level == encryption_level::application && !close_sent_)
    {
        auto retire_frames = peer_cid_manager_.get_pending_retire_frames();
        for (const auto& rf : retire_frames)
        {
            auto encoded = frame_builder::build(rf);
            payload.insert(payload.end(), encoded.begin(), encoded.end());
        }
        if (!retire_frames.empty())
        {
            peer_cid_manager_.clear_pending_retire_frames();
        }
    }

    // Add pending frames (e.g., PING from PTO probing)
    if (!close_sent_)
    {
        while (!pending_frames_.empty())
        {
            const auto& f = pending_frames_.front();
            auto encoded = frame_builder::build(f);
            payload.insert(payload.end(), encoded.begin(), encoded.end());
            pending_frames_.pop_front();
        }
    }

    // Add stream data frames for application level
    if (level == encryption_level::application && !close_sent_)
    {
        auto streams = stream_mgr_.streams_with_pending_data();
        for (auto* s : streams)
        {
            if (auto sf = s->next_stream_frame(1200))
            {
                auto encoded = frame_builder::build_stream(*sf);
                payload.insert(payload.end(), encoded.begin(), encoded.end());
            }
        }
    }

    if (payload.empty())
    {
        return packet;
    }

    // Add padding for Initial packets to meet minimum size
    if (level == encryption_level::initial)
    {
        size_t min_size = 1200;
        size_t current_size = header.size() + payload.size() + 16; // +16 for AEAD tag
        if (current_size < min_size)
        {
            auto encoded = frame_builder::build_padding(min_size - current_size);
            payload.insert(payload.end(), encoded.begin(), encoded.end());
        }
    }

    // Encrypt and protect the packet
    auto protect_result = packet_protection::protect(
        keys_result.value(),
        std::span<const uint8_t>(header.data(), header.size()),
        std::span<const uint8_t>(payload.data(), payload.size()),
        pn);

    if (protect_result.is_err())
    {
        return {};
    }

    packet = std::move(protect_result.value());

    // Track sent packet for local state
    sent_packet_info info;
    info.packet_number = pn;
    info.sent_time = std::chrono::steady_clock::now();
    info.sent_bytes = packet.size();
    info.ack_eliciting = !payload.empty();
    info.in_flight = true;
    info.level = level;
    // Note: frames are not stored here as the current implementation
    // doesn't track individual frames in build_packet.
    // For full retransmission support, frames would need to be collected
    // during packet building and stored here.

    // Notify loss detector about sent packet (RFC 9002)
    sent_packet sent_pkt;
    sent_pkt.packet_number = info.packet_number;
    sent_pkt.sent_time = info.sent_time;
    sent_pkt.sent_bytes = info.sent_bytes;
    sent_pkt.ack_eliciting = info.ack_eliciting;
    sent_pkt.in_flight = info.in_flight;
    sent_pkt.level = info.level;
    loss_detector_.on_packet_sent(sent_pkt);

    // Notify congestion controller about sent packet
    if (info.in_flight)
    {
        congestion_controller_.on_packet_sent(info.sent_bytes);
    }

    space.sent_packets[pn] = std::move(info);

    bytes_sent_ += packet.size();
    packets_sent_++;

    return packet;
}

auto connection::has_pending_data() const -> bool
{
    if (!pending_crypto_initial_.empty() ||
        !pending_crypto_handshake_.empty() ||
        !pending_crypto_app_.empty())
    {
        return true;
    }

    if (pending_ack_initial_ || pending_ack_handshake_ || pending_ack_app_)
    {
        return true;
    }

    if (close_sent_)
    {
        return true;
    }

    // Check for pending frames (e.g., PING from PTO probing)
    if (!pending_frames_.empty())
    {
        return true;
    }

    // Check for stream data
    auto& streams = const_cast<stream_manager&>(stream_mgr_);
    return !streams.streams_with_pending_data().empty();
}

// ============================================================================
// Packet Number Space Management
// ============================================================================

auto connection::get_pn_space(encryption_level level) -> packet_number_space&
{
    switch (level)
    {
    case encryption_level::initial:
    case encryption_level::zero_rtt:
        return initial_space_;
    case encryption_level::handshake:
        return handshake_space_;
    default:
        return app_space_;
    }
}

auto connection::get_pn_space(encryption_level level) const
    -> const packet_number_space&
{
    switch (level)
    {
    case encryption_level::initial:
    case encryption_level::zero_rtt:
        return initial_space_;
    case encryption_level::handshake:
        return handshake_space_;
    default:
        return app_space_;
    }
}

// ============================================================================
// State Management
// ============================================================================

void connection::update_state()
{
    if (crypto_.is_handshake_complete() && hs_state_ != handshake_state::complete)
    {
        hs_state_ = handshake_state::complete;
        if (is_server_)
        {
            state_ = connection_state::connected;
        }
        // Client waits for HANDSHAKE_DONE
    }
}

// ============================================================================
// Connection Close
// ============================================================================

auto connection::close(uint64_t error_code, const std::string& reason) -> VoidResult
{
    auto span = tracing::is_tracing_enabled()
        ? std::make_optional(tracing::trace_context::create_span("quic.connection.close"))
        : std::nullopt;
    if (span)
    {
        span->set_attribute("quic.close.error_code", static_cast<int64_t>(error_code))
             .set_attribute("quic.close.reason", reason)
             .set_attribute("quic.close.type", "transport")
             .set_attribute("quic.role", is_server_ ? "server" : "client");
    }

    // Already closing or closed - don't update error state
    if (is_closed() || is_draining())
    {
        if (span)
        {
            span->set_attribute("quic.close.already_closing", true);
        }
        return ok();
    }

    close_error_code_ = error_code;
    close_reason_ = reason;
    close_sent_ = true;
    application_close_ = false;
    enter_closing();

    if (span)
    {
        span->set_attribute("quic.state", "closing");
    }

    return ok();
}

auto connection::close_application(uint64_t error_code, const std::string& reason)
    -> VoidResult
{
    auto span = tracing::is_tracing_enabled()
        ? std::make_optional(tracing::trace_context::create_span("quic.connection.close_application"))
        : std::nullopt;
    if (span)
    {
        span->set_attribute("quic.close.error_code", static_cast<int64_t>(error_code))
             .set_attribute("quic.close.reason", reason)
             .set_attribute("quic.close.type", "application")
             .set_attribute("quic.role", is_server_ ? "server" : "client");
    }

    // Already closing or closed - don't update error state
    if (is_closed() || is_draining())
    {
        if (span)
        {
            span->set_attribute("quic.close.already_closing", true);
        }
        return ok();
    }

    close_error_code_ = error_code;
    close_reason_ = reason;
    close_sent_ = true;
    application_close_ = true;
    enter_closing();

    if (span)
    {
        span->set_attribute("quic.state", "closing");
    }

    return ok();
}

void connection::enter_closing()
{
    if (state_ == connection_state::closing || state_ == connection_state::draining)
    {
        return;
    }

    state_ = connection_state::closing;

    // Set drain timer (3 * PTO)
    auto pto = std::chrono::milliseconds(100);  // Simplified PTO
    drain_deadline_ = std::chrono::steady_clock::now() + 3 * pto;
}

void connection::enter_draining()
{
    if (state_ == connection_state::draining || state_ == connection_state::closed)
    {
        return;
    }

    state_ = connection_state::draining;

    // Set drain timer (3 * PTO)
    auto pto = std::chrono::milliseconds(100);
    drain_deadline_ = std::chrono::steady_clock::now() + 3 * pto;
}

// ============================================================================
// Timers
// ============================================================================

void connection::reset_idle_timer()
{
    auto timeout = local_params_.max_idle_timeout;
    if (remote_params_.max_idle_timeout > 0)
    {
        timeout = std::min(timeout, remote_params_.max_idle_timeout);
    }

    if (timeout > 0)
    {
        idle_deadline_ = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds(timeout);
    }
}

auto connection::next_timeout() const
    -> std::optional<std::chrono::steady_clock::time_point>
{
    if (is_closed())
    {
        return std::nullopt;
    }

    if (is_draining())
    {
        return drain_deadline_;
    }

    // Return the earliest of idle timeout and loss detection timeout
    auto loss_timeout = loss_detector_.next_timeout();
    if (loss_timeout.has_value())
    {
        return std::min(idle_deadline_, *loss_timeout);
    }

    return idle_deadline_;
}

void connection::on_timeout()
{
    auto now = std::chrono::steady_clock::now();

    // Check drain timeout
    if (is_draining() && now >= drain_deadline_)
    {
        state_ = connection_state::closed;
        return;
    }

    // Check idle timeout
    if (now >= idle_deadline_)
    {
        close_error_code_ = 0;
        close_reason_ = "Idle timeout";
        state_ = connection_state::closed;
        return;
    }

    // Handle PTO timeout for loss detection (RFC 9002 Section 6.2)
    auto loss_timeout = loss_detector_.next_timeout();
    if (loss_timeout.has_value() && now >= *loss_timeout)
    {
        auto result = loss_detector_.on_timeout();
        handle_loss_detection_result(result);
    }
}

// ============================================================================
// ACK Generation
// ============================================================================

auto connection::generate_ack_frame(const packet_number_space& space)
    -> std::optional<ack_frame>
{
    if (space.largest_received == 0 && !space.ack_needed)
    {
        return std::nullopt;
    }

    ack_frame ack;
    ack.largest_acknowledged = space.largest_received;
    ack.ack_delay = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - space.largest_received_time)
                        .count();
    // Simplified: ACK all packets from 0 to largest_received
    // The first ACK range is implicit (largest_acknowledged to largest_acknowledged - first_ack_range_length)
    // For simplicity, we don't add any additional ranges

    return ack;
}

// ============================================================================
// Loss Detection and Congestion Control (RFC 9002)
// ============================================================================

void connection::handle_loss_detection_result(const loss_detection_result& result)
{
    switch (result.event)
    {
    case loss_detection_event::pto_expired:
        // PTO timeout: generate probe packets (RFC 9002 Section 6.2.4)
        generate_probe_packets();
        break;

    case loss_detection_event::packet_lost:
        // Handle lost packets: queue frames for retransmission and update congestion control
        for (const auto& lost : result.lost_packets)
        {
            queue_frames_for_retransmission(lost);
            congestion_controller_.on_packet_lost(lost);
        }
        break;

    case loss_detection_event::none:
        // No action needed
        break;
    }

    // Process acknowledged packets for congestion control
    auto ack_time = std::chrono::steady_clock::now();
    for (const auto& acked : result.acked_packets)
    {
        congestion_controller_.on_packet_acked(acked, ack_time);
    }

    // Handle ECN congestion signal (RFC 9002 Section 7.1)
    if (result.ecn_signal == ecn_result::congestion_signal)
    {
        // ECN-CE marks indicate congestion without packet loss
        // Trigger congestion response using the sent time of the triggering packet
        congestion_controller_.on_ecn_congestion(result.ecn_congestion_sent_time);
    }
    else if (result.ecn_signal == ecn_result::ecn_failure)
    {
        // ECN validation failed - disable ECN for this connection
        // The ecn_tracker in loss_detector already handles this internally
        // No additional action needed here
    }
}

void connection::generate_probe_packets()
{
    // RFC 9002 Section 6.2.4: When PTO expires, send one or two ack-eliciting packets.
    // Probe packets must be ack-eliciting.

    // Determine which encryption level to use for probes
    // Priority: Application > Handshake > Initial (use the highest available)
    encryption_level probe_level = encryption_level::initial;

    if (crypto_.get_write_keys(encryption_level::application).is_ok())
    {
        probe_level = encryption_level::application;
    }
    else if (crypto_.get_write_keys(encryption_level::handshake).is_ok())
    {
        probe_level = encryption_level::handshake;
    }

    // Generate a PING frame as an ack-eliciting probe
    // PING frames are the simplest ack-eliciting frames
    pending_frames_.emplace_back(ping_frame{});

    // Mark that we need to send packets
    switch (probe_level)
    {
    case encryption_level::initial:
        // For initial space, we might need to retransmit crypto data
        if (!pending_crypto_initial_.empty())
        {
            // Crypto data will be sent as probe
            break;
        }
        // Queue a PING frame
        break;

    case encryption_level::handshake:
        if (!pending_crypto_handshake_.empty())
        {
            break;
        }
        break;

    case encryption_level::application:
    case encryption_level::zero_rtt:
        // Application level: PING is sufficient
        break;
    }
}

void connection::queue_frames_for_retransmission(const sent_packet& lost_packet)
{
    // Queue frames from the lost packet for retransmission
    // Note: Some frames should not be retransmitted (ACK, PADDING)
    const auto level = lost_packet.level;

    for (const auto& f : lost_packet.frames)
    {
        std::visit(
            [this, level](const auto& frm)
            {
                using T = std::decay_t<decltype(frm)>;

                // Skip non-retransmittable frames
                if constexpr (std::is_same_v<T, padding_frame> ||
                              std::is_same_v<T, ack_frame>)
                {
                    // These frames are not retransmitted
                }
                else if constexpr (std::is_same_v<T, crypto_frame>)
                {
                    // Crypto frames need to be retransmitted with their data
                    switch (level)
                    {
                    case encryption_level::initial:
                        pending_crypto_initial_.push_back(frm.data);
                        break;
                    case encryption_level::handshake:
                        pending_crypto_handshake_.push_back(frm.data);
                        break;
                    case encryption_level::zero_rtt:
                    case encryption_level::application:
                        pending_crypto_app_.push_back(frm.data);
                        break;
                    }
                }
                else if constexpr (std::is_same_v<T, stream_frame>)
                {
                    // Stream data retransmission:
                    // The stream will naturally resend unacknowledged data
                    // when next_stream_frame() is called, as the data remains
                    // in the send buffer until acknowledged via acknowledge_data().
                    // No explicit action needed here.
                    (void)frm;
                }
                else if constexpr (std::is_same_v<T, ping_frame> ||
                                   std::is_same_v<T, new_token_frame> ||
                                   std::is_same_v<T, handshake_done_frame>)
                {
                    // These frames can be retransmitted
                    pending_frames_.push_back(frm);
                }
                else if constexpr (std::is_same_v<T, max_data_frame> ||
                                   std::is_same_v<T, max_stream_data_frame> ||
                                   std::is_same_v<T, max_streams_frame> ||
                                   std::is_same_v<T, data_blocked_frame> ||
                                   std::is_same_v<T, stream_data_blocked_frame> ||
                                   std::is_same_v<T, streams_blocked_frame>)
                {
                    // Flow control frames: queue for retransmission
                    pending_frames_.push_back(frm);
                }
                else if constexpr (std::is_same_v<T, reset_stream_frame> ||
                                   std::is_same_v<T, stop_sending_frame>)
                {
                    // Stream control frames: queue for retransmission
                    pending_frames_.push_back(frm);
                }
                else if constexpr (std::is_same_v<T, new_connection_id_frame> ||
                                   std::is_same_v<T, retire_connection_id_frame>)
                {
                    // Connection ID management: queue for retransmission
                    pending_frames_.push_back(frm);
                }
                else if constexpr (std::is_same_v<T, path_challenge_frame> ||
                                   std::is_same_v<T, path_response_frame>)
                {
                    // Path validation: may need fresh challenge, skip retransmission
                    (void)frm;
                }
                else if constexpr (std::is_same_v<T, connection_close_frame>)
                {
                    // CONNECTION_CLOSE is handled specially during closing
                    (void)frm;
                }
            },
            f);
    }
}

auto connection::to_sent_packet(const sent_packet_info& info) const -> sent_packet
{
    sent_packet pkt;
    pkt.packet_number = info.packet_number;
    pkt.sent_time = info.sent_time;
    pkt.sent_bytes = info.sent_bytes;
    pkt.ack_eliciting = info.ack_eliciting;
    pkt.in_flight = info.in_flight;
    pkt.level = info.level;
    pkt.frames = info.frames;
    return pkt;
}

// ============================================================================
// Peer Connection ID Management
// ============================================================================

auto connection::active_peer_cid() const -> const connection_id&
{
    // If peer CID manager has CIDs, use the active one
    if (peer_cid_manager_.peer_cid_count() > 0)
    {
        return peer_cid_manager_.get_active_peer_cid();
    }
    // Fall back to remote_cid_ for backward compatibility
    return remote_cid_;
}

auto connection::rotate_peer_cid() -> VoidResult
{
    return peer_cid_manager_.rotate_peer_cid();
}

// ============================================================================
// Path MTU Discovery
// ============================================================================

auto connection::path_mtu() const noexcept -> size_t
{
    return pmtud_controller_.current_mtu();
}

void connection::enable_pmtud()
{
    pmtud_controller_.enable();
    // Update congestion controller with current MTU
    congestion_controller_.set_max_datagram_size(pmtud_controller_.current_mtu());
}

void connection::disable_pmtud()
{
    pmtud_controller_.disable();
    // Reset congestion controller to minimum MTU
    congestion_controller_.set_max_datagram_size(pmtud_controller_.min_mtu());
}

auto connection::pmtud_enabled() const noexcept -> bool
{
    return pmtud_controller_.is_enabled();
}

} // namespace kcenon::network::protocols::quic
