#pragma once
#include <chrono>
#include <atomic>

namespace network_system {

class session_timeout_manager {
public:
    explicit session_timeout_manager(std::chrono::seconds timeout = std::chrono::seconds(300))
        : timeout_(timeout) {}

    void update_activity() {
        last_activity_ = std::chrono::steady_clock::now();
    }

    bool is_timed_out() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_activity_.load());
        return elapsed > timeout_;
    }

    auto get_idle_time() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now - last_activity_.load());
    }

private:
    std::chrono::seconds timeout_;
    std::atomic<std::chrono::steady_clock::time_point> last_activity_{
        std::chrono::steady_clock::now()};
};

} // namespace
