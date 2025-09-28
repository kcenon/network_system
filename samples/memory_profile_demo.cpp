#include <network_system/utils/memory_profiler.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

int main() {
    using network_system::utils::memory_profiler;
    auto& profiler = memory_profiler::instance();
    profiler.start(200ms);

    std::cout << "Memory profiler started (sampling every 200ms)" << std::endl;

    // Allocate some memory to see RSS growth
    std::vector<std::vector<char>> buffers;
    for (int i = 0; i < 10; ++i) {
        buffers.emplace_back(10 * 1024 * 1024, '\0'); // 10MB
        std::this_thread::sleep_for(150ms);
    }

    auto hist = profiler.get_history();
    std::cout << "Snapshots: " << hist.size() << std::endl;
    for (const auto& s : hist) {
        auto t = std::chrono::system_clock::to_time_t(s.timestamp);
        std::cout << std::put_time(std::localtime(&t), "%H:%M:%S")
                  << " RSS=" << (s.resident_bytes / (1024*1024)) << "MB"
                  << " VMS=" << (s.virtual_bytes / (1024*1024)) << "MB" << std::endl;
    }

    profiler.stop();
    std::cout << "Memory profiler stopped" << std::endl;
    return 0;
}

