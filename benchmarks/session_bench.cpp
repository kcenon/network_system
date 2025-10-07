/**
 * @file session_bench.cpp
 * @brief Session management benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>
#include <mutex>

// Mock session class
class mock_session {
public:
    explicit mock_session(int id) : id_(id), created_at_(std::chrono::steady_clock::now()) {}

    int get_id() const { return id_; }

    void set_data(const std::string& key, const std::string& value) {
        data_[key] = value;
    }

    std::string get_data(const std::string& key) const {
        auto it = data_.find(key);
        return it != data_.end() ? it->second : "";
    }

    void update_activity() {
        last_activity_ = std::chrono::steady_clock::now();
    }

    bool is_expired(std::chrono::seconds timeout) const {
        auto now = std::chrono::steady_clock::now();
        return (now - last_activity_) > timeout;
    }

private:
    int id_;
    std::chrono::steady_clock::time_point created_at_;
    std::chrono::steady_clock::time_point last_activity_;
    std::unordered_map<std::string, std::string> data_;
};

// Session manager for benchmarking
class session_manager {
public:
    int create_session() {
        int id = next_id_++;
        sessions_[id] = std::make_unique<mock_session>(id);
        return id;
    }

    void destroy_session(int id) {
        sessions_.erase(id);
    }

    mock_session* get_session(int id) {
        auto it = sessions_.find(id);
        return it != sessions_.end() ? it->second.get() : nullptr;
    }

    size_t get_session_count() const {
        return sessions_.size();
    }

    void cleanup_expired(std::chrono::seconds timeout) {
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            if (it->second->is_expired(timeout)) {
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<int, std::unique_ptr<mock_session>> sessions_;
    int next_id_ = 1;
};

// Benchmark session creation
static void BM_Session_Create(benchmark::State& state) {
    for (auto _ : state) {
        mock_session session(1);
        benchmark::DoNotOptimize(session);
    }
}
BENCHMARK(BM_Session_Create);

// Benchmark session manager - create session
static void BM_SessionManager_CreateSession(benchmark::State& state) {
    session_manager manager;

    for (auto _ : state) {
        int id = manager.create_session();
        benchmark::DoNotOptimize(id);
    }
}
BENCHMARK(BM_SessionManager_CreateSession);

// Benchmark session manager - create and destroy
static void BM_SessionManager_CreateDestroy(benchmark::State& state) {
    session_manager manager;

    for (auto _ : state) {
        int id = manager.create_session();
        manager.destroy_session(id);
    }
}
BENCHMARK(BM_SessionManager_CreateDestroy);

// Benchmark session data storage
static void BM_Session_SetData(benchmark::State& state) {
    mock_session session(1);

    for (auto _ : state) {
        session.set_data("key", "value");
    }
}
BENCHMARK(BM_Session_SetData);

// Benchmark session data retrieval
static void BM_Session_GetData(benchmark::State& state) {
    mock_session session(1);
    session.set_data("key", "value");

    for (auto _ : state) {
        auto value = session.get_data("key");
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_Session_GetData);

// Benchmark session lookup
static void BM_SessionManager_Lookup(benchmark::State& state) {
    session_manager manager;
    int id = manager.create_session();

    for (auto _ : state) {
        auto* session = manager.get_session(id);
        benchmark::DoNotOptimize(session);
    }
}
BENCHMARK(BM_SessionManager_Lookup);

// Benchmark session manager with many sessions
static void BM_SessionManager_ManySession(benchmark::State& state) {
    int num_sessions = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        session_manager manager;
        state.ResumeTiming();

        for (int i = 0; i < num_sessions; ++i) {
            manager.create_session();
        }

        benchmark::DoNotOptimize(manager);
    }

    state.SetItemsProcessed(state.iterations() * num_sessions);
}
BENCHMARK(BM_SessionManager_ManySession)->Arg(10)->Arg(100)->Arg(1000);

// Benchmark session lookup with many sessions
static void BM_SessionManager_LookupMany(benchmark::State& state) {
    int num_sessions = state.range(0);
    session_manager manager;

    std::vector<int> ids;
    for (int i = 0; i < num_sessions; ++i) {
        ids.push_back(manager.create_session());
    }

    for (auto _ : state) {
        int target_id = ids[state.iterations() % ids.size()];
        auto* session = manager.get_session(target_id);
        benchmark::DoNotOptimize(session);
    }
}
BENCHMARK(BM_SessionManager_LookupMany)->Arg(10)->Arg(100)->Arg(1000);

// Benchmark session cleanup
static void BM_SessionManager_Cleanup(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        session_manager manager;
        for (int i = 0; i < 100; ++i) {
            manager.create_session();
        }
        state.ResumeTiming();

        manager.cleanup_expired(std::chrono::seconds(0)); // All sessions expired
        benchmark::DoNotOptimize(manager);
    }
}
BENCHMARK(BM_SessionManager_Cleanup);

// Benchmark session activity update
static void BM_Session_UpdateActivity(benchmark::State& state) {
    mock_session session(1);

    for (auto _ : state) {
        session.update_activity();
    }
}
BENCHMARK(BM_Session_UpdateActivity);

// Benchmark concurrent session access
static void BM_SessionManager_Concurrent(benchmark::State& state) {
    static session_manager manager;
    static std::once_flag init_flag;

    std::call_once(init_flag, []() {
        for (int i = 0; i < 100; ++i) {
            manager.create_session();
        }
    });

    for (auto _ : state) {
        auto* session = manager.get_session(1 + (state.iterations() % 100));
        if (session) {
            session->update_activity();
        }
        benchmark::DoNotOptimize(session);
    }
}
BENCHMARK(BM_SessionManager_Concurrent)->Threads(4)->Threads(8)->Threads(16);
