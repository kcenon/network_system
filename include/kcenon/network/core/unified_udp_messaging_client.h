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

#include <atomic>
#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/core/callback_indices.h"
#include "kcenon/network/interfaces/i_udp_client.h"
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
 * \class unified_udp_messaging_client
 * \brief Unified UDP client template parameterized by TLS policy.
 *
 * This template consolidates plain UDP and secure UDP (DTLS) client variants
 * into a single implementation. The TLS policy determines at compile-time
 * whether secure communication is used.
 *
 * ### Template Parameters
 * - \c TlsPolicy: TLS policy type (no_tls or tls_enabled)
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 * - Socket access is protected by socket_mutex_.
 * - Atomic flags prevent race conditions.
 * - send_packet() can be called from any thread safely.
 *
 * ### Key Characteristics
 * - Connectionless: No persistent connection, each send is independent (plain UDP).
 * - DTLS handshake: For secure mode, performs DTLS handshake before data transfer.
 * - Target endpoint: Configured at start, can be changed via set_target().
 * - Bidirectional: Can both send and receive datagrams.
 *
 * ### Usage Example
 * \code
 * // Plain UDP client
 * auto plain_client = std::make_shared<unified_udp_messaging_client<no_tls>>("client1");
 * plain_client->start_client("localhost", 5555);
 *
 * // Secure UDP client (DTLS)
 * policy::tls_enabled tls_config{.verify_peer = false};
 * auto secure_client = std::make_shared<unified_udp_messaging_client<tls_enabled>>(
 *     "client2", tls_config);
 * secure_client->start_client("localhost", 5556);
 * \endcode
 *
 * \tparam TlsPolicy The TLS policy type (must satisfy policy::TlsPolicy concept)
 */
template <policy::TlsPolicy TlsPolicy = policy::no_tls>
class unified_udp_messaging_client
    : public std::enable_shared_from_this<unified_udp_messaging_client<TlsPolicy>>
    , public interfaces::i_udp_client
{
public:
    //! \brief Indicates whether TLS (DTLS) is enabled for this client
    static constexpr bool is_secure = policy::is_tls_enabled_v<TlsPolicy>;

    //! \brief Callback type for received datagrams with sender endpoint
    using receive_callback_t =
        std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)>;
    //! \brief Callback type for connection established (DTLS handshake complete)
    using connected_callback_t = std::function<void()>;
    //! \brief Callback type for disconnection
    using disconnected_callback_t = std::function<void()>;
    //! \brief Callback type for errors
    using error_callback_t = std::function<void(std::error_code)>;

    /*!
     * \brief Constructs a plain UDP client with a given identifier.
     * \param client_id A string identifier for this client instance.
     *
     * \note This constructor is only available when TlsPolicy is no_tls.
     */
    explicit unified_udp_messaging_client(std::string_view client_id)
        requires(!policy::is_tls_enabled_v<TlsPolicy>);

    /*!
     * \brief Constructs a secure UDP client (DTLS) with TLS configuration.
     * \param client_id A string identifier for this client instance.
     * \param tls_config TLS configuration (cert paths, verification settings).
     *
     * \note This constructor is only available when TlsPolicy is tls_enabled.
     */
    unified_udp_messaging_client(std::string_view client_id, const TlsPolicy& tls_config)
        requires policy::is_tls_enabled_v<TlsPolicy>;

    /*!
     * \brief Destructor; automatically calls stop_client() if still running.
     */
    ~unified_udp_messaging_client() noexcept override;

    // Non-copyable, non-movable
    unified_udp_messaging_client(const unified_udp_messaging_client&) = delete;
    unified_udp_messaging_client& operator=(const unified_udp_messaging_client&) = delete;
    unified_udp_messaging_client(unified_udp_messaging_client&&) = delete;
    unified_udp_messaging_client& operator=(unified_udp_messaging_client&&) = delete;

    // =====================================================================
    // Lifecycle Management
    // =====================================================================

    /*!
     * \brief Starts the client by resolving target host and port.
     * \param host The target hostname or IP address.
     * \param port The target port number.
     * \return Result<void> - Success if client started, or error with code:
     *         - error_codes::common_errors::already_exists if already running
     *         - error_codes::common_errors::invalid_argument if empty host
     *         - error_codes::common_errors::internal_error for other failures
     */
    [[nodiscard]] auto start_client(std::string_view host, uint16_t port) -> VoidResult;

    /*!
     * \brief Stops the client and releases all resources.
     * \return Result<void> - Success if client stopped, or error with code:
     *         - error_codes::common_errors::internal_error for failures
     */
    [[nodiscard]] auto stop_client() -> VoidResult;

    /*!
     * \brief Returns the client identifier.
     * \return The client_id string.
     */
    [[nodiscard]] auto client_id() const -> const std::string&;

    // =====================================================================
    // i_network_component interface implementation
    // =====================================================================

    /*!
     * \brief Checks if the client is currently running.
     * \return true if running, false otherwise.
     */
    [[nodiscard]] auto is_running() const -> bool override;

    /*!
     * \brief Blocks until stop_client() is called.
     */
    auto wait_for_stop() -> void override;

    // =====================================================================
    // i_udp_client interface implementation
    // =====================================================================

    [[nodiscard]] auto start(std::string_view host, uint16_t port) -> VoidResult override;
    [[nodiscard]] auto stop() -> VoidResult override;
    [[nodiscard]] auto send(std::vector<uint8_t>&& data,
                            interfaces::i_udp_client::send_callback_t handler = nullptr)
        -> VoidResult override;
    [[nodiscard]] auto set_target(std::string_view host, uint16_t port) -> VoidResult override;
    auto set_receive_callback(interfaces::i_udp_client::receive_callback_t callback)
        -> void override;
    auto set_error_callback(error_callback_t callback) -> void override;

    // =====================================================================
    // Extended API (not in interface)
    // =====================================================================

    /*!
     * \brief Checks if the client is connected (DTLS handshake complete).
     * \return true if connected, false otherwise.
     *
     * For plain UDP, returns true if running.
     */
    [[nodiscard]] auto is_connected() const noexcept -> bool;

    /*!
     * \brief Sends a datagram to the configured target endpoint.
     * \param data The data to send (moved for efficiency).
     * \return Result<void> - Success if send initiated, or error.
     */
    [[nodiscard]] auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

    /*!
     * \brief Sets the callback for received datagrams (legacy version).
     * \param callback The callback function with asio endpoint.
     */
    auto set_receive_callback(receive_callback_t callback) -> void;

    /*!
     * \brief Sets the callback for connection established (DTLS handshake complete).
     * \param callback Function called when connection is established.
     */
    auto set_connected_callback(connected_callback_t callback) -> void;

    /*!
     * \brief Sets the callback for disconnection.
     * \param callback Function called when disconnected.
     */
    auto set_disconnected_callback(disconnected_callback_t callback) -> void;

private:
    // =====================================================================
    // Internal Socket Type Selection
    // =====================================================================

#ifdef BUILD_TLS_SUPPORT
    using socket_type =
        std::conditional_t<is_secure, internal::dtls_socket, internal::udp_socket>;
#else
    using socket_type = internal::udp_socket;
#endif

    // =====================================================================
    // Internal Implementation Methods
    // =====================================================================

    auto do_start_impl(std::string_view host, uint16_t port) -> VoidResult;
    auto do_stop_impl() -> VoidResult;

#ifdef BUILD_TLS_SUPPORT
    auto init_ssl_context() -> VoidResult
        requires is_secure;
    auto do_handshake() -> VoidResult
        requires is_secure;
#endif

    // =====================================================================
    // Internal Callback Helpers
    // =====================================================================

    auto set_connected(bool connected) -> void;
    auto invoke_receive_callback(const std::vector<uint8_t>& data,
                                 const asio::ip::udp::endpoint& endpoint) -> void;
    auto invoke_connected_callback() -> void;
    auto invoke_disconnected_callback() -> void;
    auto invoke_error_callback(std::error_code ec) -> void;

    [[nodiscard]] auto get_receive_callback() const -> receive_callback_t;
    [[nodiscard]] auto get_error_callback() const -> error_callback_t;

private:
    //! \brief Callback index type alias for clarity
    using callback_index = unified_udp_client_callback;

    //! \brief Callback manager type for this client
    using callbacks_t = utils::callback_manager<receive_callback_t,
                                                connected_callback_t,
                                                disconnected_callback_t,
                                                error_callback_t>;

    // =====================================================================
    // Member Variables
    // =====================================================================

    std::string client_id_;               /*!< Client identifier. */
    utils::lifecycle_manager lifecycle_;  /*!< Lifecycle state manager. */
    callbacks_t callbacks_;               /*!< Callback manager. */
    std::atomic<bool> is_connected_{false};  /*!< Connection state (DTLS handshake). */

    std::unique_ptr<asio::io_context> io_context_;  /*!< ASIO I/O context. */
    std::shared_ptr<socket_type> socket_;           /*!< Socket wrapper. */

    std::shared_ptr<integration::thread_pool_interface> thread_pool_;
    std::future<void> io_context_future_;  /*!< Future for io_context run task. */

    std::mutex endpoint_mutex_;              /*!< Protects target_endpoint_. */
    asio::ip::udp::endpoint target_endpoint_;  /*!< Target endpoint for sends. */

    mutable std::mutex socket_mutex_;  /*!< Protects socket access. */

#ifdef BUILD_TLS_SUPPORT
    SSL_CTX* ssl_ctx_{nullptr};                    /*!< OpenSSL SSL context (DTLS). */
    [[no_unique_address]] TlsPolicy tls_config_;  /*!< TLS configuration. */
#endif
};

// =====================================================================
// Type Aliases for Convenience
// =====================================================================

/*!
 * \brief Type alias for plain UDP client.
 */
using udp_client = unified_udp_messaging_client<policy::no_tls>;

#ifdef BUILD_TLS_SUPPORT
/*!
 * \brief Type alias for secure UDP client with DTLS.
 */
using secure_udp_client = unified_udp_messaging_client<policy::tls_enabled>;
#endif

}  // namespace kcenon::network::core

// Include template implementation
#include "unified_udp_messaging_client.inl"
