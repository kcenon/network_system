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
#include "kcenon/network/core/network_context.h"

#include <atomic>
#include <unordered_map>
#include <vector>

namespace kcenon::network::integration {

class io_context_thread_manager::impl {
public:
    struct context_entry {
        std::weak_ptr<asio::io_context> context;
        std::string component_name;
        std::future<void> future;
    };

    impl() : total_started_(0), total_completed_(0) {}

    ~impl() {
        stop_all();
        wait_all();
    }

    std::shared_ptr<thread_pool_interface> get_thread_pool() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!thread_pool_) {
            thread_pool_ = network_system::core::network_context::instance().get_thread_pool();
            if (!thread_pool_) {
                // Fallback to basic thread pool
                thread_pool_ = std::make_shared<basic_thread_pool>(2);
            }
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

        auto future = pool->submit([ctx_weak, name, this]() {
            auto ctx = ctx_weak.lock();
            if (!ctx) {
                NETWORK_LOG_WARN("[io_context_thread_manager] io_context expired before run: " + name);
                return;
            }

            NETWORK_LOG_INFO("[io_context_thread_manager] Starting io_context: " + name);
            try {
                ctx->run();
                NETWORK_LOG_INFO("[io_context_thread_manager] io_context completed: " + name);
            } catch (const std::exception& e) {
                NETWORK_LOG_ERROR("[io_context_thread_manager] Exception in io_context " + name + ": " + e.what());
            } catch (...) {
                NETWORK_LOG_ERROR("[io_context_thread_manager] Unknown exception in io_context: " + name);
            }

            total_completed_.fetch_add(1, std::memory_order_relaxed);
        });

        // Track the entry
        {
            std::lock_guard<std::mutex> lock(mutex_);
            context_entry entry;
            entry.context = io_context;
            entry.component_name = name;
            entry.future = std::move(future);
            entries_.push_back(std::move(entry));
            total_started_.fetch_add(1, std::memory_order_relaxed);
        }

        // Clean up expired entries periodically
        cleanup_expired();

        // Return an empty future since we moved it into the entry
        // We need to create a new future for the caller
        auto promise = std::make_shared<std::promise<void>>();

        // Submit a monitoring task that waits for the io_context to complete
        pool->submit([ctx_weak, promise]() {
            auto ctx = ctx_weak.lock();
            if (!ctx) {
                promise->set_value();
                return;
            }

            // Wait for the io_context to stop
            // This task will complete when the io_context is stopped
            while (!ctx->stopped()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // Check if context is still valid
                if (ctx.use_count() <= 1) {
                    break;
                }
            }
            promise->set_value();
        });

        return promise->get_future();
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
                NETWORK_LOG_DEBUG("[io_context_thread_manager] Stopping: " + entry.component_name);
                ctx->stop();
            }
        }
    }

    void wait_all() {
        std::vector<std::future<void>> futures;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& entry : entries_) {
                if (entry.future.valid()) {
                    futures.push_back(std::move(entry.future));
                }
            }
            entries_.clear();
        }

        for (auto& f : futures) {
            if (f.valid()) {
                try {
                    f.wait();
                } catch (...) {
                    // Ignore exceptions during shutdown
                }
            }
        }
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
