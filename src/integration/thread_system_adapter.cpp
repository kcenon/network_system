#include "network_system/integration/thread_system_adapter.h"

#if defined(BUILD_WITH_THREAD_SYSTEM)

#include <thread>
#include <stdexcept>

namespace network_system::integration {

thread_system_pool_adapter::thread_system_pool_adapter(
    std::shared_ptr<kcenon::thread::thread_pool> pool)
    : pool_(std::move(pool)) {
    if (!pool_) {
        throw std::invalid_argument("thread_system_pool_adapter: pool is null");
    }
}

std::future<void> thread_system_pool_adapter::submit(std::function<void()> task) {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    bool ok = pool_->submit_task([task = std::move(task), promise]() mutable {
        try {
            if (task) task();
            promise->set_value();
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    });

    if (!ok) {
        promise->set_exception(std::make_exception_ptr(
            std::runtime_error("thread_system_pool_adapter: submit failed")));
    }
    return future;
}

std::future<void> thread_system_pool_adapter::submit_delayed(
    std::function<void()> task,
    std::chrono::milliseconds delay
) {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    // Dispatch a timer thread to schedule onto the pool without blocking workers
    std::thread([this, task = std::move(task), delay, promise]() mutable {
        try {
            std::this_thread::sleep_for(delay);
            bool ok = pool_->submit_task([task = std::move(task), promise]() mutable {
                try {
                    if (task) task();
                    promise->set_value();
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
            if (!ok) {
                promise->set_exception(std::make_exception_ptr(
                    std::runtime_error("thread_system_pool_adapter: delayed submit failed")));
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();

    return future;
}

size_t thread_system_pool_adapter::worker_count() const {
    return pool_->get_thread_count();
}

bool thread_system_pool_adapter::is_running() const {
    return pool_->is_running();
}

size_t thread_system_pool_adapter::pending_tasks() const {
    return pool_->get_pending_task_count();
}

std::shared_ptr<thread_system_pool_adapter> thread_system_pool_adapter::create_default(
    const std::string& pool_name
) {
    kcenon::thread::thread_context ctx; // default resolves logger/monitoring if registered
    auto pool = std::make_shared<kcenon::thread::thread_pool>(pool_name, ctx);
    (void)pool->start(); // best-effort start; ignore error to keep adapter usable
    return std::make_shared<thread_system_pool_adapter>(std::move(pool));
}

std::shared_ptr<thread_system_pool_adapter> thread_system_pool_adapter::from_service_or_default(
    const std::string& pool_name
) {
    try {
        auto& sc = kcenon::thread::service_container::global();
        if (auto existing = sc.resolve<kcenon::thread::thread_pool>()) {
            return std::make_shared<thread_system_pool_adapter>(std::move(existing));
        }
    } catch (...) {
        // ignore and fallback
    }
    return create_default(pool_name);
}

bool bind_thread_system_pool_into_manager(const std::string& pool_name) {
    try {
        auto adapter = thread_system_pool_adapter::from_service_or_default(pool_name);
        if (!adapter) return false;
        thread_integration_manager::instance().set_thread_pool(adapter);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace network_system::integration

#endif // BUILD_WITH_THREAD_SYSTEM

