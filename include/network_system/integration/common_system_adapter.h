// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
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

#pragma once

/**
 * @file common_system_adapter.h
 * @brief Adapter for common_system integration
 */

#ifdef BUILD_WITH_COMMON_SYSTEM
#include <kcenon/common/interfaces/executor_interface.h>
#include <kcenon/common/patterns/result.h>
#endif

#include <memory>
#include <future>
#include <functional>

namespace network_system {
namespace integration {

#ifdef BUILD_WITH_COMMON_SYSTEM

/**
 * @brief Adapter to use common_system IExecutor for async operations
 */
class common_executor_adapter {
public:
    explicit common_executor_adapter(
        std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor)
        : executor_(executor) {}

    /**
     * @brief Submit a task for execution
     */
    template<typename Func>
    auto submit(Func&& func) -> std::future<void> {
        if (!executor_) {
            std::promise<void> promise;
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Executor not initialized")));
            return promise.get_future();
        }
        return executor_->submit(std::forward<Func>(func));
    }

    /**
     * @brief Check if executor is running
     */
    bool is_running() const {
        return executor_ && executor_->is_running();
    }

    /**
     * @brief Shutdown the executor
     */
    void shutdown(bool wait_for_completion = true) {
        if (executor_) {
            executor_->shutdown(wait_for_completion);
        }
    }

private:
    std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor_;
};

/**
 * @brief Convert network errors to common Result
 */
template<typename T>
inline ::common::Result<T> to_common_result(T&& value) {
    return ::common::Result<T>(std::forward<T>(value));
}

inline ::common::VoidResult to_common_result_void() {
    return ::common::VoidResult(std::monostate{});
}

inline ::common::VoidResult to_common_error(int code, const std::string& msg) {
    return ::common::VoidResult(::common::error_info(code, msg, "network_system"));
}

#endif // BUILD_WITH_COMMON_SYSTEM

} // namespace integration
} // namespace network_system