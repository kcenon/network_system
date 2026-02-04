/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
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

/**
 * @file network_context.cpp
 * @brief Implementation of network context
 *
 * @author kcenon
 * @date 2025-01-13
 */

#include "internal/core/network_context.h"
#include "kcenon/network/detail/config/feature_flags.h"

#include <thread>
#include <mutex>

#if KCENON_WITH_THREAD_SYSTEM
#include "internal/integration/thread_system_adapter.h"
#endif

namespace kcenon::network::core {

// Use namespace alias for integration types
namespace integration = kcenon::network::integration;

class network_context::impl {
public:
    impl() = default;

    std::shared_ptr<integration::thread_pool_interface> thread_pool_;
    std::shared_ptr<integration::logger_interface> logger_;
    std::shared_ptr<integration::monitoring_interface> monitoring_;

    bool initialized_ = false;
    bool owns_thread_pool_ = false;
    mutable std::mutex mutex_;
};

network_context::network_context()
    // Intentional Leak pattern: Use no-op deleter to prevent destruction
    // during static destruction phase. This avoids heap corruption when
    // io_context_thread_manager tasks still reference the thread pool.
    // Memory impact: ~few KB (reclaimed by OS on process termination)
    : pimpl_(new impl(), [](impl*) { /* no-op deleter - intentional leak */ }) {
}

network_context::~network_context() {
    // Note: With Intentional Leak pattern, shutdown() is never called
    // during static destruction. The OS reclaims all resources on exit.
    // This prevents heap corruption from thread pool destruction order issues.
}

network_context& network_context::instance() {
    static network_context ctx;
    return ctx;
}

void network_context::initialize(size_t thread_count) {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);

    if (pimpl_->initialized_) {
        return;
    }

    // Initialize thread pool if not already set
    if (!pimpl_->thread_pool_) {
        if (thread_count == 0) {
            thread_count = std::thread::hardware_concurrency();
            if (thread_count == 0) {
                thread_count = 2; // Fallback
            }
        }

#if KCENON_WITH_THREAD_SYSTEM
        // Use thread_system's thread_pool via adapter
        try {
            auto adapter = integration::thread_system_pool_adapter::from_service_or_default("network_pool");
            if (adapter && adapter->is_running()) {
                pimpl_->thread_pool_ = adapter;
                pimpl_->owns_thread_pool_ = false; // Managed by thread_system
            } else {
                // Fallback to basic pool if thread_system pool is not available
                pimpl_->thread_pool_ = std::make_shared<integration::basic_thread_pool>(thread_count);
                pimpl_->owns_thread_pool_ = true;
            }
        } catch (...) {
            // If thread_system integration fails, use basic pool
            pimpl_->thread_pool_ = std::make_shared<integration::basic_thread_pool>(thread_count);
            pimpl_->owns_thread_pool_ = true;
        }
#else
        // Use basic thread pool when thread_system is not available
        pimpl_->thread_pool_ = std::make_shared<integration::basic_thread_pool>(thread_count);
        pimpl_->owns_thread_pool_ = true;
#endif
    }

    // Initialize logger if not already set
    // Issue #285: Uses common_system_logger_adapter when available, basic_logger otherwise
    if (!pimpl_->logger_) {
#if KCENON_WITH_COMMON_SYSTEM
        pimpl_->logger_ = std::make_shared<integration::common_system_logger_adapter>();
#else
        pimpl_->logger_ = std::make_shared<integration::basic_logger>(
            integration::log_level::info);
#endif
        integration::logger_integration_manager::instance().set_logger(pimpl_->logger_);
    }

    // Initialize monitoring if not already set
    // Note: Uses basic_monitoring by default. Metrics are also published via EventBus.
    if (!pimpl_->monitoring_) {
        pimpl_->monitoring_ = integration::monitoring_integration_manager::instance().get_monitoring();
    }

    pimpl_->initialized_ = true;

    if (pimpl_->logger_) {
        pimpl_->logger_->log(
            integration::log_level::info,
            "network_context initialized with " + std::to_string(thread_count) + " threads"
        );
    }
}

void network_context::shutdown() {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);

    if (!pimpl_->initialized_) {
        return;
    }

    if (pimpl_->logger_) {
        pimpl_->logger_->log(integration::log_level::info, "network_context shutting down");
    }

    if (pimpl_->monitoring_) {
        pimpl_->monitoring_.reset();
    }

    // Only stop thread pool if we own it
    if (pimpl_->owns_thread_pool_ && pimpl_->thread_pool_) {
        // Try to stop the thread pool if it's a basic_thread_pool
        if (auto* basic_pool = dynamic_cast<integration::basic_thread_pool*>(pimpl_->thread_pool_.get())) {
            basic_pool->stop(true);
        }
        pimpl_->thread_pool_.reset();
    }

    // Don't reset logger as it may be used during shutdown logging
    // Just mark as not initialized
    pimpl_->initialized_ = false;
}

bool network_context::is_initialized() const {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    return pimpl_->initialized_;
}

void network_context::set_thread_pool(std::shared_ptr<integration::thread_pool_interface> pool) {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    pimpl_->thread_pool_ = pool;
    pimpl_->owns_thread_pool_ = false;
}

std::shared_ptr<integration::thread_pool_interface> network_context::get_thread_pool() {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    return pimpl_->thread_pool_;
}

void network_context::set_logger(std::shared_ptr<integration::logger_interface> logger) {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    pimpl_->logger_ = logger;
    integration::logger_integration_manager::instance().set_logger(logger);
}

std::shared_ptr<integration::logger_interface> network_context::get_logger() {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    return pimpl_->logger_;
}

void network_context::set_monitoring(std::shared_ptr<integration::monitoring_interface> monitoring) {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    pimpl_->monitoring_ = monitoring;
    // Also update the integration manager for backward compatibility
    integration::monitoring_integration_manager::instance().set_monitoring(monitoring);
}

std::shared_ptr<integration::monitoring_interface> network_context::get_monitoring() {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    if (!pimpl_->monitoring_) {
        // Return the default from integration manager
        return integration::monitoring_integration_manager::instance().get_monitoring();
    }
    return pimpl_->monitoring_;
}

} // namespace kcenon::network::core
