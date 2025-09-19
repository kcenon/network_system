/**
 * @file messaging_bridge.cpp
 * @brief Implementation of messaging_bridge for messaging_system compatibility
 *
 * @author kcenon
 * @date 2025-09-19
 * @version 2.0.0
 */

#include "network_system/integration/messaging_bridge.h"
#include <atomic>
#include <mutex>

namespace network_system::integration {

class messaging_bridge::impl {
public:
    impl() : initialized_(true), start_time_(std::chrono::steady_clock::now()) {}

    std::atomic<bool> initialized_{false};
    mutable std::mutex metrics_mutex_;
    performance_metrics metrics_;

#ifdef BUILD_WITH_CONTAINER_SYSTEM
    std::shared_ptr<container_system::factory> container_factory_;
    std::function<void(const container_system::message&)> container_handler_;
#endif

#ifdef BUILD_WITH_THREAD_SYSTEM
    std::shared_ptr<thread_system::thread_pool> thread_pool_;
#endif
};

messaging_bridge::messaging_bridge() : pimpl_(std::make_unique<impl>()) {
    pimpl_->metrics_.start_time = std::chrono::steady_clock::now();
}

messaging_bridge::~messaging_bridge() = default;

std::shared_ptr<core::messaging_server> messaging_bridge::create_server(
    const std::string& server_id
) {
    return std::make_shared<core::messaging_server>(server_id);
}

std::shared_ptr<core::messaging_client> messaging_bridge::create_client(
    const std::string& client_id
) {
    return std::make_shared<core::messaging_client>(client_id);
}

#ifdef BUILD_WITH_CONTAINER_SYSTEM
void messaging_bridge::set_container_factory(
    std::shared_ptr<container_system::factory> factory
) {
    pimpl_->container_factory_ = factory;
}

void messaging_bridge::set_container_message_handler(
    std::function<void(const container_system::message&)> handler
) {
    pimpl_->container_handler_ = handler;
}
#endif

#ifdef BUILD_WITH_THREAD_SYSTEM
void messaging_bridge::set_thread_pool(
    std::shared_ptr<thread_system::thread_pool> pool
) {
    pimpl_->thread_pool_ = pool;
}
#endif

messaging_bridge::performance_metrics messaging_bridge::get_metrics() const {
    std::lock_guard<std::mutex> lock(pimpl_->metrics_mutex_);
    return pimpl_->metrics_;
}

void messaging_bridge::reset_metrics() {
    std::lock_guard<std::mutex> lock(pimpl_->metrics_mutex_);
    pimpl_->metrics_ = performance_metrics{};
    pimpl_->metrics_.start_time = std::chrono::steady_clock::now();
}

bool messaging_bridge::is_initialized() const {
    return pimpl_->initialized_.load();
}

} // namespace network_system::integration