// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file rate_limiter.h
 * @brief Rate limiting and connection limiting for DoS prevention
 *
 * @details Provides token bucket rate limiting and connection count limiting
 * to prevent denial-of-service attacks.
 *
 * Features:
 * - Token bucket rate limiting per client
 * - Connection count limiting
 * - Thread-safe implementation
 * - Configurable limits
 * - Automatic cleanup of stale entries
 *
 * @example
 * @code
 * rate_limiter limiter({
 *     .max_requests_per_second = 100,
 *     .burst_size = 20
 * });
 *
 * if (!limiter.allow("192.168.1.1")) {
 *     return error::rate_limited;
 * }
 * @endcode
 */

#pragma once

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <string_view>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace kcenon::network {

/**
 * @brief Configuration for rate limiter
 */
struct rate_limiter_config {
    /// Maximum requests per second
    size_t max_requests_per_second = 100;

    /// Maximum burst size (token bucket capacity)
    size_t burst_size = 20;

    /// Time window for rate calculation
    std::chrono::seconds window = std::chrono::seconds(1);

    /// Enable automatic cleanup of stale entries
    bool auto_cleanup = true;

    /// Stale entry expiration time
    std::chrono::seconds stale_timeout = std::chrono::seconds(300);
};

/**
 * @brief Token bucket rate limiter
 *
 * @details Implements a token bucket algorithm for rate limiting.
 * Each client is identified by a string key (typically IP address).
 *
 * ### Thread Safety
 * This class is thread-safe. All public methods can be called
 * from multiple threads concurrently.
 *
 * ### Algorithm
 * - Each client has a bucket with capacity = burst_size
 * - Tokens are added at rate = max_requests_per_second
 * - Each request consumes one token
 * - If no tokens available, request is denied
 */
class rate_limiter {
public:
    /**
     * @brief Construct rate limiter with configuration
     * @param config Rate limiter configuration
     */
    explicit rate_limiter(rate_limiter_config config = {})
        : config_(std::move(config))
        , last_cleanup_(std::chrono::steady_clock::now()) {}

    /**
     * @brief Check if request should be allowed
     *
     * @param client_id Client identifier (e.g., IP address)
     * @return true if request is allowed, false if rate limited
     */
    [[nodiscard]] bool allow(std::string_view client_id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();

        // Periodic cleanup
        if (config_.auto_cleanup) {
            maybe_cleanup(now);
        }

        std::string key(client_id);
        auto& bucket = buckets_[key];

        // Initialize new bucket
        if (bucket.last_refill.time_since_epoch().count() == 0) {
            bucket.tokens = static_cast<double>(config_.burst_size);
            bucket.last_refill = now;
        }

        // Refill tokens based on elapsed time
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - bucket.last_refill);

        double tokens_to_add = elapsed.count() *
            (static_cast<double>(config_.max_requests_per_second) / 1000.0);

        bucket.tokens = std::min(
            static_cast<double>(config_.burst_size),
            bucket.tokens + tokens_to_add);
        bucket.last_refill = now;

        // Try to consume a token
        if (bucket.tokens >= 1.0) {
            bucket.tokens -= 1.0;
            return true;
        }

        return false;
    }

    /**
     * @brief Check if request would be allowed (without consuming token)
     *
     * @param client_id Client identifier
     * @return true if a request would be allowed
     */
    [[nodiscard]] bool would_allow(std::string_view client_id) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = buckets_.find(std::string(client_id));
        if (it == buckets_.end()) {
            return true; // New client would have full bucket
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.last_refill);

        double tokens_to_add = elapsed.count() *
            (static_cast<double>(config_.max_requests_per_second) / 1000.0);

        double available = std::min(
            static_cast<double>(config_.burst_size),
            it->second.tokens + tokens_to_add);

        return available >= 1.0;
    }

    /**
     * @brief Get remaining tokens for a client
     *
     * @param client_id Client identifier
     * @return Number of remaining tokens
     */
    [[nodiscard]] double remaining_tokens(std::string_view client_id) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = buckets_.find(std::string(client_id));
        if (it == buckets_.end()) {
            return static_cast<double>(config_.burst_size);
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.last_refill);

        double tokens_to_add = elapsed.count() *
            (static_cast<double>(config_.max_requests_per_second) / 1000.0);

        return std::min(
            static_cast<double>(config_.burst_size),
            it->second.tokens + tokens_to_add);
    }

    /**
     * @brief Reset rate limit for a specific client
     * @param client_id Client identifier
     */
    void reset(std::string_view client_id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        buckets_.erase(std::string(client_id));
    }

    /**
     * @brief Reset all rate limits
     */
    void reset_all() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        buckets_.clear();
    }

    /**
     * @brief Get number of tracked clients
     * @return Number of clients in rate limiter
     */
    [[nodiscard]] size_t client_count() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return buckets_.size();
    }

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void set_config(rate_limiter_config config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        config_ = std::move(config);
    }

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    [[nodiscard]] rate_limiter_config config() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_;
    }

private:
    struct bucket {
        double tokens = 0.0;
        std::chrono::steady_clock::time_point last_refill{};
    };

    void maybe_cleanup(std::chrono::steady_clock::time_point now) {
        // Cleanup at most once per minute
        auto since_cleanup = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_cleanup_);

        if (since_cleanup < std::chrono::seconds(60)) {
            return;
        }

        last_cleanup_ = now;

        // Remove stale entries
        for (auto it = buckets_.begin(); it != buckets_.end();) {
            auto since_refill = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.last_refill);

            if (since_refill > config_.stale_timeout) {
                it = buckets_.erase(it);
            } else {
                ++it;
            }
        }
    }

    rate_limiter_config config_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, bucket> buckets_;
    std::chrono::steady_clock::time_point last_cleanup_;
};

/**
 * @brief Connection count limiter
 *
 * @details Limits the number of concurrent connections to prevent
 * resource exhaustion attacks.
 *
 * ### Thread Safety
 * This class is thread-safe. All public methods can be called
 * from multiple threads concurrently.
 */
class connection_limiter {
public:
    /**
     * @brief Construct connection limiter
     * @param max_connections Maximum allowed concurrent connections
     */
    explicit connection_limiter(size_t max_connections = 1000)
        : max_connections_(max_connections)
        , current_connections_(0) {}

    /**
     * @brief Check if a new connection can be accepted
     * @return true if connection can be accepted
     */
    [[nodiscard]] bool can_accept() const noexcept {
        return current_connections_.load(std::memory_order_acquire) < max_connections_;
    }

    /**
     * @brief Try to accept a connection
     * @return true if connection was accepted, false if limit reached
     */
    [[nodiscard]] bool try_accept() noexcept {
        size_t current = current_connections_.load(std::memory_order_acquire);
        while (current < max_connections_) {
            if (current_connections_.compare_exchange_weak(
                    current, current + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Register a new connection
     *
     * @note Use try_accept() instead when possible to atomically check and increment.
     */
    void on_connect() noexcept {
        current_connections_.fetch_add(1, std::memory_order_release);
    }

    /**
     * @brief Unregister a connection
     */
    void on_disconnect() noexcept {
        size_t prev = current_connections_.fetch_sub(1, std::memory_order_release);
        // Prevent underflow (shouldn't happen in correct usage)
        if (prev == 0) {
            current_connections_.fetch_add(1, std::memory_order_release);
        }
    }

    /**
     * @brief Get current connection count
     * @return Current number of connections
     */
    [[nodiscard]] size_t current() const noexcept {
        return current_connections_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get maximum connection limit
     * @return Maximum allowed connections
     */
    [[nodiscard]] size_t max() const noexcept {
        return max_connections_;
    }

    /**
     * @brief Set maximum connection limit
     * @param max_connections New maximum
     */
    void set_max(size_t max_connections) noexcept {
        max_connections_ = max_connections;
    }

    /**
     * @brief Get available connection slots
     * @return Number of available slots
     */
    [[nodiscard]] size_t available() const noexcept {
        size_t current = current_connections_.load(std::memory_order_acquire);
        return (current < max_connections_) ? (max_connections_ - current) : 0;
    }

    /**
     * @brief Check if at capacity
     * @return true if at maximum connections
     */
    [[nodiscard]] bool at_capacity() const noexcept {
        return current_connections_.load(std::memory_order_acquire) >= max_connections_;
    }

private:
    size_t max_connections_;
    std::atomic<size_t> current_connections_;
};

/**
 * @brief RAII guard for connection limiting
 *
 * Automatically registers/unregisters connection on construction/destruction.
 */
class connection_guard {
public:
    /**
     * @brief Construct guard and try to accept connection
     * @param limiter Connection limiter
     */
    explicit connection_guard(connection_limiter& limiter)
        : limiter_(&limiter)
        , accepted_(limiter.try_accept()) {}

    /// Non-copyable
    connection_guard(const connection_guard&) = delete;
    connection_guard& operator=(const connection_guard&) = delete;

    /// Movable
    connection_guard(connection_guard&& other) noexcept
        : limiter_(other.limiter_)
        , accepted_(other.accepted_) {
        other.accepted_ = false;
    }

    connection_guard& operator=(connection_guard&& other) noexcept {
        if (this != &other) {
            release();
            limiter_ = other.limiter_;
            accepted_ = other.accepted_;
            other.accepted_ = false;
        }
        return *this;
    }

    ~connection_guard() {
        release();
    }

    /**
     * @brief Check if connection was accepted
     * @return true if connection was successfully accepted
     */
    [[nodiscard]] bool accepted() const noexcept {
        return accepted_;
    }

    /**
     * @brief Explicit bool conversion
     */
    explicit operator bool() const noexcept {
        return accepted_;
    }

    /**
     * @brief Release the connection early
     */
    void release() noexcept {
        if (accepted_ && limiter_) {
            limiter_->on_disconnect();
            accepted_ = false;
        }
    }

private:
    connection_limiter* limiter_;
    bool accepted_;
};

/**
 * @brief Per-client connection limiter
 *
 * Limits connections per client identifier (e.g., IP address).
 */
class per_client_connection_limiter {
public:
    /**
     * @brief Construct limiter
     * @param max_per_client Maximum connections per client
     * @param max_total Maximum total connections
     */
    explicit per_client_connection_limiter(
            size_t max_per_client = 10,
            size_t max_total = 1000)
        : max_per_client_(max_per_client)
        , total_limiter_(max_total) {}

    /**
     * @brief Try to accept a connection from a client
     * @param client_id Client identifier
     * @return true if accepted
     */
    [[nodiscard]] bool try_accept(std::string_view client_id) {
        // First check total limit
        if (!total_limiter_.can_accept()) {
            return false;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        std::string key(client_id);

        auto& count = client_connections_[key];
        if (count >= max_per_client_) {
            return false;
        }

        if (!total_limiter_.try_accept()) {
            return false;
        }

        ++count;
        return true;
    }

    /**
     * @brief Release a connection from a client
     * @param client_id Client identifier
     */
    void release(std::string_view client_id) {
        std::unique_lock<std::mutex> lock(mutex_);
        std::string key(client_id);

        auto it = client_connections_.find(key);
        if (it != client_connections_.end() && it->second > 0) {
            --it->second;
            if (it->second == 0) {
                client_connections_.erase(it);
            }
            total_limiter_.on_disconnect();
        }
    }

    /**
     * @brief Get connection count for a client
     * @param client_id Client identifier
     * @return Number of connections from client
     */
    [[nodiscard]] size_t client_connections(std::string_view client_id) const {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = client_connections_.find(std::string(client_id));
        return (it != client_connections_.end()) ? it->second : 0;
    }

    /**
     * @brief Get total connection count
     * @return Total number of connections
     */
    [[nodiscard]] size_t total_connections() const noexcept {
        return total_limiter_.current();
    }

private:
    size_t max_per_client_;
    connection_limiter total_limiter_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, size_t> client_connections_;
};

} // namespace kcenon::network
