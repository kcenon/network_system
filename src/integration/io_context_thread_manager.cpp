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
 * @file io_context_thread_manager.cpp
 * @brief Implementation of io_context thread management
 *
 * @author kcenon
 * @date 2025-01-06
 */

#include "kcenon/network/integration/io_context_thread_manager.h"
#include "kcenon/network/integration/logger_integration.h"
// Removed: #include "kcenon/network/core/network_context.h"
// This removes the circular dependency that caused static destruction order issues

#include <atomic>
#include <unordered_map>
#include <vector>

namespace kcenon::network::integration {

class io_context_thread_manager::impl {
public:
    struct context_entry {
        std::weak_ptr<asio::io_context> context;
        std::string component_name;
    };

    impl() : total_started_(0), total_completed_(0), owns_thread_pool_(false), is_destroying_(false) {}

    ~impl() {
        // Mark as destroying to prevent logging during destruction
        // (logger singleton may already be destroyed)
        is_destroying_ = true;

        // Safe cleanup to avoid static destruction order issues
        try {
            stop_all_internal();
            wait_all();

            // Only reset the thread pool if we own it
            // IMPORTANT: Do NOT call explicit stop() here!
            // During static destruction, calling stop() on thread_pool can trigger
            // thread_system's thread_logger access, which may cause crashes if
            // the logger singleton is already destroyed (Static Destruction Order Fiasco).
            // Instead, delegate cleanup to shared_ptr destruction which handles it safely.
            // Related: thread_system#293 (thread_logger now uses Intentional Leak pattern)
            if (owns_thread_pool_ && thread_pool_) {
                thread_pool_.reset();
            }
        } catch (...) {
            // Swallow exceptions in destructor to prevent termination
        }
    }

    std::shared_ptr<thread_pool_interface> get_thread_pool() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!thread_pool_) {
            // IMPORTANT: Create our own thread pool instead of using network_context
            // This avoids static destruction order issues where network_context's
            // thread pool might be destroyed before io_context_thread_manager
            // Tests like ConnectionScaling need 20+ clients + server
            // Use larger size to avoid thread exhaustion
            auto pool_size = std::max(32u, std::thread::hardware_concurrency() * 4);
            thread_pool_ = std::make_shared<basic_thread_pool>(pool_size);
            owns_thread_pool_ = true;
        }
        return thread_pool_;
    }

    std::future<void> run_io_context(
        std::shared_ptr<asio::io_context> io_context,
        const std::string& component_name
    ) {
        auto pool = get_thread_pool();
        if (!pool) {
            auto promise = std::make_shared<std::promise<void>>();
            promise->set_exception(
                std::make_exception_ptr(
                    std::runtime_error("Thread pool not available")
                )
            );
            return promise->get_future();
        }

        // Submit io_context::run to thread pool
        auto ctx_weak = std::weak_ptr<asio::io_context>(io_context);
        auto name = component_name.empty() ? "unnamed" : component_name;

        // Create a shared promise for the caller's future
        auto caller_promise = std::make_shared<std::promise<void>>();
        auto caller_future = caller_promise->get_future();

        // Track the entry for metrics and stop_all functionality
        {
            std::lock_guard<std::mutex> lock(mutex_);
            context_entry entry;
            entry.context = io_context;
            entry.component_name = name;
            entries_.push_back(std::move(entry));
            total_started_.fetch_add(1, std::memory_order_relaxed);
        }

        // Submit the io_context::run task - this is the ONLY task we submit
        // No separate monitoring task to avoid thread pool exhaustion
        pool->submit([ctx_weak, name, caller_promise, this]() {
            auto ctx = ctx_weak.lock();
            if (!ctx) {
                NETWORK_LOG_WARN("[io_context_thread_manager] io_context expired before run: " + name);
                caller_promise->set_value();
                return;
            }

            NETWORK_LOG_INFO("[io_context_thread_manager] Starting io_context: " + name);
            try {
                ctx->run();
                NETWORK_LOG_INFO("[io_context_thread_manager] io_context completed: " + name);
                caller_promise->set_value();
            } catch (const std::exception& e) {
                NETWORK_LOG_ERROR("[io_context_thread_manager] Exception in io_context " + name + ": " + e.what());
                caller_promise->set_exception(std::current_exception());
            } catch (...) {
                NETWORK_LOG_ERROR("[io_context_thread_manager] Unknown exception in io_context: " + name);
                caller_promise->set_exception(std::current_exception());
            }

            total_completed_.fetch_add(1, std::memory_order_relaxed);
        });

        // Clean up expired entries periodically
        cleanup_expired();

        return caller_future;
    }

    void stop_io_context(std::shared_ptr<asio::io_context> io_context) {
        if (io_context) {
            NETWORK_LOG_DEBUG("[io_context_thread_manager] Stopping io_context");
            io_context->stop();
        }
    }

    void stop_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : entries_) {
            auto ctx = entry.context.lock();
            if (ctx) {
                if (!is_destroying_) {
                    NETWORK_LOG_DEBUG("[io_context_thread_manager] Stopping: " + entry.component_name);
                }
                ctx->stop();
            }
        }
    }

    // Internal version called from destructor - no logging
    void stop_all_internal() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : entries_) {
            auto ctx = entry.context.lock();
            if (ctx) {
                ctx->stop();
            }
        }
    }

    void wait_all() {
        // Since futures are returned directly to callers, wait_all just
        // waits for all tracked io_contexts to be stopped.
        // The callers should wait on their own futures.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                bool all_stopped = true;
                for (const auto& entry : entries_) {
                    auto ctx = entry.context.lock();
                    if (ctx && !ctx->stopped()) {
                        all_stopped = false;
                        break;
                    }
                }
                if (all_stopped) {
                    entries_.clear();
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Clear any remaining expired entries
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    size_t active_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& entry : entries_) {
            auto ctx = entry.context.lock();
            if (ctx && !ctx->stopped()) {
                ++count;
            }
        }
        return count;
    }

    bool is_active(std::shared_ptr<asio::io_context> io_context) const {
        if (!io_context) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& entry : entries_) {
            auto ctx = entry.context.lock();
            if (ctx && ctx.get() == io_context.get() && !ctx->stopped()) {
                return true;
            }
        }
        return false;
    }

    void set_thread_pool(std::shared_ptr<thread_pool_interface> pool) {
        std::lock_guard<std::mutex> lock(mutex_);
        thread_pool_ = pool;
    }

    io_context_thread_manager::metrics get_metrics() const {
        io_context_thread_manager::metrics m;
        m.active_contexts = active_count();
        m.total_started = total_started_.load(std::memory_order_relaxed);
        m.total_completed = total_completed_.load(std::memory_order_relaxed);
        return m;
    }

private:
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                [](const context_entry& entry) {
                    return entry.context.expired();
                }),
            entries_.end()
        );
    }

    mutable std::mutex mutex_;
    std::shared_ptr<thread_pool_interface> thread_pool_;
    std::vector<context_entry> entries_;
    std::atomic<size_t> total_started_;
    std::atomic<size_t> total_completed_;
    bool owns_thread_pool_ = false;  // Track if we created the thread pool
    bool is_destroying_ = false;     // True during destructor to avoid logging
};

io_context_thread_manager& io_context_thread_manager::instance() {
    static io_context_thread_manager instance;
    return instance;
}

io_context_thread_manager::io_context_thread_manager()
    : pimpl_(std::make_unique<impl>()) {
}

io_context_thread_manager::~io_context_thread_manager() = default;

std::future<void> io_context_thread_manager::run_io_context(
    std::shared_ptr<asio::io_context> io_context,
    const std::string& component_name
) {
    return pimpl_->run_io_context(io_context, component_name);
}

void io_context_thread_manager::stop_io_context(
    std::shared_ptr<asio::io_context> io_context
) {
    pimpl_->stop_io_context(io_context);
}

void io_context_thread_manager::stop_all() {
    pimpl_->stop_all();
}

void io_context_thread_manager::wait_all() {
    pimpl_->wait_all();
}

size_t io_context_thread_manager::active_count() const {
    return pimpl_->active_count();
}

bool io_context_thread_manager::is_active(
    std::shared_ptr<asio::io_context> io_context
) const {
    return pimpl_->is_active(io_context);
}

void io_context_thread_manager::set_thread_pool(
    std::shared_ptr<thread_pool_interface> pool
) {
    pimpl_->set_thread_pool(pool);
}

io_context_thread_manager::metrics io_context_thread_manager::get_metrics() const {
    return pimpl_->get_metrics();
}

} // namespace kcenon::network::integration
