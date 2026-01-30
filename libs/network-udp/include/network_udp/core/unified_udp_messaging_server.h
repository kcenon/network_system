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

#include <array>
#include <atomic>
#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/core/callback_indices.h"
#include "network_udp/interfaces/i_udp_server.h"
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/policy/tls_policy.h"
#include "kcenon/network/protocol/protocol_tags.h"
#include "kcenon/network/utils/callback_manager.h"
#include "kcenon/network/utils/lifecycle_manager.h"
#include "kcenon/network/utils/result_types.h"

#ifdef BUILD_TLS_SUPPORT
#include <openssl/ssl.h>
#endif

namespace kcenon::network::internal
{
class udp_socket;
#ifdef BUILD_TLS_SUPPORT
class dtls_socket;
#endif
}  // namespace kcenon::network::internal

namespace kcenon::network::core
{

/*!
 * \class unified_udp_messaging_server
 * \brief Unified UDP server template parameterized by TLS policy.
 *
 * This template consolidates plain UDP and secure UDP (DTLS) server variants
 * into a single implementation. The TLS policy determines at compile-time
 * whether secure communication is used.
 *
 * ### Template Parameters
 * - \c TlsPolicy: TLS policy type (no_tls or tls_enabled)
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 * - Internal state is protected by appropriate mutexes.
 * - Background thread runs io_context.run() independently.
 * - Callbacks are invoked on ASIO worker thread.
 *
 * ### Key Characteristics
 * - Plain UDP: Connectionless, endpoint-based routing.
 * - DTLS: Session-based with per-client DTLS contexts.
 * - Both variants use the same callback interface for consistency.
 *
 * ### Usage Example
 * \code
 * // Plain UDP server
 * auto plain_server = std::make_shared<unified_udp_messaging_server<no_tls>>("server1");
 * plain_server->start_server(5555);
 *
 * // Secure UDP server (DTLS)
 * policy::tls_enabled tls_config{
 *     .cert_path = "server.crt",
 *     .key_path = "server.key"
 * };
 * auto secure_server = std::make_shared<unified_udp_messaging_server<tls_enabled>>(
 *     "server2", tls_config);
 * secure_server->start_server(5556);
 * \endcode
 *
 * \tparam TlsPolicy The TLS policy type (must satisfy policy::TlsPolicy concept)
 */
template <policy::TlsPolicy TlsPolicy = policy::no_tls>
class unified_udp_messaging_server
    : public std::enable_shared_from_this<unified_udp_messaging_server<TlsPolicy>>
    , public interfaces::i_udp_server
{
public:
    //! \brief Indicates whether TLS (DTLS) is enabled for this server
    static constexpr bool is_secure = policy::is_tls_enabled_v<TlsPolicy>;

    //! \brief Callback type for received datagrams with sender endpoint
    using receive_callback_t =
        std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)>;
    //! \brief Callback type for client connection (DTLS handshake complete)
    using client_connected_callback_t = std::function<void(const asio::ip::udp::endpoint&)>;
    //! \brief Callback type for client disconnection
    using client_disconnected_callback_t = std::function<void(const asio::ip::udp::endpoint&)>;
    //! \brief Callback type for errors
    using error_callback_t = std::function<void(std::error_code)>;

    /*!
     * \brief Constructs a plain UDP server with a given identifier.
     * \param server_id A string identifier for this server instance.
     *
     * \note This constructor is only available when TlsPolicy is no_tls.
     */
    explicit unified_udp_messaging_server(std::string_view server_id)
        requires(!policy::is_tls_enabled_v<TlsPolicy>);

    /*!
     * \brief Constructs a secure UDP server (DTLS) with TLS configuration.
     * \param server_id A string identifier for this server instance.
     * \param tls_config TLS configuration (cert paths, verification settings).
     *
     * \note This constructor is only available when TlsPolicy is tls_enabled.
     *       For DTLS server, cert_path and key_path are required.
     */
    unified_udp_messaging_server(std::string_view server_id, const TlsPolicy& tls_config)
        requires policy::is_tls_enabled_v<TlsPolicy>;

    /*!
     * \brief Destructor; automatically calls stop_server() if still running.
     */
    ~unified_udp_messaging_server() noexcept override;

    // Non-copyable, non-movable
    unified_udp_messaging_server(const unified_udp_messaging_server&) = delete;
    unified_udp_messaging_server& operator=(const unified_udp_messaging_server&) = delete;
    unified_udp_messaging_server(unified_udp_messaging_server&&) = delete;
    unified_udp_messaging_server& operator=(unified_udp_messaging_server&&) = delete;

    // =====================================================================
    // Lifecycle Management
    // =====================================================================

    /*!
     * \brief Starts the server on the specified port.
     * \param port The UDP port to bind and listen on.
     * \return Result<void> - Success if server started, or error with code:
     *         - error_codes::network_system::server_already_running if already running
     *         - error_codes::network_system::bind_failed if port binding failed
     *         - error_codes::common_errors::internal_error for other failures
     */
    [[nodiscard]] auto start_server(uint16_t port) -> VoidResult;

    /*!
     * \brief Stops the server and releases all resources.
     * \return Result<void> - Success if server stopped, or error with code:
     *         - error_codes::common_errors::internal_error for failures
     */
    [[nodiscard]] auto stop_server() -> VoidResult;

    /*!
     * \brief Returns the server identifier.
     * \return The server_id string.
     */
    [[nodiscard]] auto server_id() const -> const std::string&;

    // =====================================================================
    // i_network_component interface implementation
    // =====================================================================

    /*!
     * \brief Checks if the server is currently running.
     * \return true if running, false otherwise.
     */
    [[nodiscard]] auto is_running() const -> bool override;

    /*!
     * \brief Blocks until stop_server() is called.
     */
    auto wait_for_stop() -> void override;

    // =====================================================================
    // i_udp_server interface implementation
    // =====================================================================

    [[nodiscard]] auto start(uint16_t port) -> VoidResult override;
    [[nodiscard]] auto stop() -> VoidResult override;
    [[nodiscard]] auto send_to(const interfaces::i_udp_server::endpoint_info& endpoint,
                               std::vector<uint8_t>&& data,
                               interfaces::i_udp_server::send_callback_t handler = nullptr)
        -> VoidResult override;
    auto set_receive_callback(interfaces::i_udp_server::receive_callback_t callback)
        -> void override;
    auto set_error_callback(error_callback_t callback) -> void override;

    // =====================================================================
    // Extended API (not in interface)
    // =====================================================================

    /*!
     * \brief Sets the callback for received datagrams (legacy version).
     * \param callback The callback function with asio endpoint.
     */
    auto set_receive_callback(receive_callback_t callback) -> void;

    /*!
     * \brief Sets the callback for client connections (DTLS handshake complete).
     * \param callback Function called when a client completes DTLS handshake.
     *
     * \note For plain UDP, this callback is not used.
     */
    auto set_client_connected_callback(client_connected_callback_t callback) -> void;

    /*!
     * \brief Sets the callback for client disconnections.
     * \param callback Function called when a client session ends.
     *
     * \note For plain UDP, this callback is not used.
     */
    auto set_client_disconnected_callback(client_disconnected_callback_t callback) -> void;

    /*!
     * \brief Sends a datagram to the specified endpoint (async version).
     * \param data The data to send (moved for efficiency).
     * \param endpoint The target client endpoint.
     * \param handler Optional completion handler.
     */
    auto async_send_to(std::vector<uint8_t>&& data,
                       const asio::ip::udp::endpoint& endpoint,
                       std::function<void(std::error_code, std::size_t)> handler = nullptr)
        -> void;

private:
#ifdef BUILD_TLS_SUPPORT
    /*!
     * \brief DTLS session for a client.
     */
    struct dtls_session
    {
        std::shared_ptr<internal::dtls_socket> socket;
        bool handshake_complete{false};
    };

    /*!
     * \brief Hash function for endpoint (for unordered_map).
     */
    struct endpoint_hash
    {
        std::size_t operator()(const asio::ip::udp::endpoint& ep) const
        {
            auto addr_hash = std::hash<std::string>{}(ep.address().to_string());
            auto port_hash = std::hash<unsigned short>{}(ep.port());
            return addr_hash ^ (port_hash << 1);
        }
    };
#endif

    // =====================================================================
    // Internal Implementation Methods
    // =====================================================================

    auto do_start_impl(uint16_t port) -> VoidResult;
    auto do_stop_impl() -> VoidResult;

#ifdef BUILD_TLS_SUPPORT
    auto init_ssl_context() -> VoidResult
        requires is_secure;
    auto do_receive() -> void
        requires is_secure;
    auto process_session_data(const std::vector<uint8_t>& data,
                              const asio::ip::udp::endpoint& sender) -> void
        requires is_secure;
    auto create_session(const asio::ip::udp::endpoint& client_endpoint)
        -> std::shared_ptr<dtls_session>
        requires is_secure;
#endif

    // =====================================================================
    // Internal Callback Helpers
    // =====================================================================

    auto invoke_receive_callback(const std::vector<uint8_t>& data,
                                 const asio::ip::udp::endpoint& endpoint) -> void;
    auto invoke_error_callback(std::error_code ec) -> void;

    [[nodiscard]] auto get_receive_callback() const -> receive_callback_t;
    [[nodiscard]] auto get_error_callback() const -> error_callback_t;

private:
    //! \brief Callback index type alias for clarity
    using callback_index = unified_udp_server_callback;

    //! \brief Callback manager type for this server
    using callbacks_t = utils::callback_manager<receive_callback_t,
                                                client_connected_callback_t,
                                                client_disconnected_callback_t,
                                                error_callback_t>;

    // =====================================================================
    // Member Variables
    // =====================================================================

    std::string server_id_;               /*!< Server identifier. */
    utils::lifecycle_manager lifecycle_;  /*!< Lifecycle state manager. */
    callbacks_t callbacks_;               /*!< Callback manager. */

    std::unique_ptr<asio::io_context> io_context_;  /*!< ASIO I/O context. */

    // Plain UDP mode uses udp_socket
    std::shared_ptr<internal::udp_socket> socket_;  /*!< Plain UDP socket wrapper. */

    std::shared_ptr<integration::thread_pool_interface> thread_pool_;
    std::future<void> io_context_future_;  /*!< Future for io_context run task. */

#ifdef BUILD_TLS_SUPPORT
    // DTLS mode uses raw socket + session management
    std::unique_ptr<asio::ip::udp::socket> dtls_socket_;  /*!< Raw UDP socket for DTLS. */
    SSL_CTX* ssl_ctx_{nullptr};                           /*!< OpenSSL SSL context. */
    [[no_unique_address]] TlsPolicy tls_config_;          /*!< TLS configuration. */

    std::array<uint8_t, 65536> read_buffer_;           /*!< Buffer for receiving datagrams. */
    asio::ip::udp::endpoint sender_endpoint_;          /*!< Current sender endpoint. */
    std::mutex sessions_mutex_;                        /*!< Protects sessions map. */
    std::unordered_map<asio::ip::udp::endpoint,
                       std::shared_ptr<dtls_session>,
                       endpoint_hash>
        sessions_;  /*!< Active DTLS sessions. */
#endif
};

// =====================================================================
// Type Aliases for Convenience
// =====================================================================

/*!
 * \brief Type alias for plain UDP server.
 */
using udp_server = unified_udp_messaging_server<policy::no_tls>;

#ifdef BUILD_TLS_SUPPORT
/*!
 * \brief Type alias for secure UDP server with DTLS.
 */
using secure_udp_server = unified_udp_messaging_server<policy::tls_enabled>;
#endif

}  // namespace kcenon::network::core

// Include template implementation
#include "unified_udp_messaging_server.inl"
