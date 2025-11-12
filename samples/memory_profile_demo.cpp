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

