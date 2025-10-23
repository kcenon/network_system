#pragma once

/**
 * @file thread_system_adapter.h
 * @brief Adapter that bridges thread_system::thread_pool to thread_pool_interface
 *
 * This optional adapter lets NetworkSystem use thread_system's thread_pool via the
 * existing thread_integration API, strengthening DI/scheduling consistency.
 * Enabled when BUILD_WITH_THREAD_SYSTEM is defined.
 */

#include "network_system/integration/thread_integration.h"
#include <memory>
#include <future>
#include <string>

#if defined(BUILD_WITH_THREAD_SYSTEM)
// Suppress deprecation warnings from thread_system headers
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
// Include fmt before thread_system headers since thread_system uses fmt formatters
#  if defined(USE_FMT_LIBRARY)
#    include <fmt/format.h>
#  elif defined(USE_STD_FORMAT)
#    include <format>
#  endif
#  include <kcenon/thread/core/thread_pool.h>
#  include <kcenon/thread/interfaces/thread_context.h>
#  include <kcenon/thread/interfaces/service_container.h>
#  pragma clang diagnostic pop
#endif

namespace network_system::integration {

#if defined(BUILD_WITH_THREAD_SYSTEM)

class thread_system_pool_adapter : public thread_pool_interface {
public:
    explicit thread_system_pool_adapter(std::shared_ptr<kcenon::thread::thread_pool> pool);

    // thread_pool_interface
    std::future<void> submit(std::function<void()> task) override;
    std::future<void> submit_delayed(std::function<void()> task,
                                     std::chrono::milliseconds delay) override;
    size_t worker_count() const override;
    bool is_running() const override;
    size_t pending_tasks() const override;

    // Convenience helpers
    static std::shared_ptr<thread_system_pool_adapter> create_default(
        const std::string& pool_name = "network_pool");

    // Try resolving from thread_system's service_container; fallback to default
    static std::shared_ptr<thread_system_pool_adapter> from_service_or_default(
        const std::string& pool_name = "network_pool");

private:
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
};

// Bind a thread_system adapter into the global integration manager.
// Returns true on success.
bool bind_thread_system_pool_into_manager(const std::string& pool_name = "network_pool");

#else // BUILD_WITH_THREAD_SYSTEM

// Placeholder when thread_system is not available, to keep includes harmless
struct thread_system_pool_adapter_unavailable final {
    thread_system_pool_adapter_unavailable() = delete;
};

#endif // BUILD_WITH_THREAD_SYSTEM

} // namespace network_system::integration

