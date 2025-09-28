#pragma once

/**
 * @file common_system_adapter.h
 * @brief Adapter for common_system integration
 */

#ifdef USE_COMMON_SYSTEM
#include <kcenon/common/interfaces/executor_interface.h>
#include <kcenon/common/patterns/result.h>
#endif

#include <memory>
#include <future>
#include <functional>

namespace network_system {
namespace integration {

#ifdef USE_COMMON_SYSTEM

/**
 * @brief Adapter to use common_system IExecutor for async operations
 */
class common_executor_adapter {
public:
    explicit common_executor_adapter(
        std::shared_ptr<::common::interfaces::IExecutor> executor)
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
    std::shared_ptr<::common::interfaces::IExecutor> executor_;
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

#endif // USE_COMMON_SYSTEM

} // namespace integration
} // namespace network_system