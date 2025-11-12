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

#include "network_system/core/network_context.h"
#include <thread>
#include <mutex>

namespace network_system::core {

class network_context::impl {
public:
    impl() = default;

    std::shared_ptr<integration::thread_pool_interface> thread_pool_;
    std::shared_ptr<integration::logger_interface> logger_;

#ifdef BUILD_WITH_MONITORING_SYSTEM
    std::shared_ptr<integration::monitoring_interface> monitoring_;
#endif

    bool initialized_ = false;
    bool owns_thread_pool_ = false;
    mutable std::mutex mutex_;
};

network_context::network_context() : pimpl_(std::make_unique<impl>()) {}

network_context::~network_context() {
    shutdown();
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

        pimpl_->thread_pool_ = std::make_shared<integration::basic_thread_pool>(thread_count);
        pimpl_->owns_thread_pool_ = true;
    }

    // Initialize logger if not already set
    if (!pimpl_->logger_) {
#ifdef BUILD_WITH_LOGGER_SYSTEM
        auto logger_adapter = std::make_shared<integration::logger_system_adapter>(true, 8192);
        logger_adapter->start();
        pimpl_->logger_ = logger_adapter;
#else
        pimpl_->logger_ = std::make_shared<integration::basic_logger>(
            integration::log_level::info);
#endif
        integration::logger_integration_manager::instance().set_logger(pimpl_->logger_);
    }

#ifdef BUILD_WITH_MONITORING_SYSTEM
    // Initialize monitoring if not already set
    if (!pimpl_->monitoring_) {
        pimpl_->monitoring_ = std::make_shared<integration::monitoring_system_adapter>("network_system");
        pimpl_->monitoring_->start();
    }
#endif

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

#ifdef BUILD_WITH_MONITORING_SYSTEM
    if (pimpl_->monitoring_) {
        pimpl_->monitoring_->stop();
        pimpl_->monitoring_.reset();
    }
#endif

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

#ifdef BUILD_WITH_MONITORING_SYSTEM
void network_context::set_monitoring(std::shared_ptr<integration::monitoring_interface> monitoring) {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    pimpl_->monitoring_ = monitoring;
}

std::shared_ptr<integration::monitoring_interface> network_context::get_monitoring() {
    std::lock_guard<std::mutex> lock(pimpl_->mutex_);
    return pimpl_->monitoring_;
}
#endif

} // namespace network_system::core
