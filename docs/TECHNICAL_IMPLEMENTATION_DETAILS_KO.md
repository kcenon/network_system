# 기술 구현 세부사항
# Network System 분리 기술 구현 가이드

> **Language:** [English](TECHNICAL_IMPLEMENTATION_DETAILS.md) | **한국어**

**날짜**: 2025-09-19
**버전**: 1.0.0
**소유자**: kcenon

---

## 🏗️ 아키텍처 설계

### 1. 모듈 계층 구조

```
                    ┌─────────────────────────────────────┐
                    │        애플리케이션 계층              │
                    │   (messaging_system, other apps)   │
                    └─────────────────┬───────────────────┘
                                      │
                    ┌─────────────────▼───────────────────┐
                    │     통합 계층                        │
                    │  ┌─────────────┬─────────────────┐  │
                    │  │ messaging   │ container       │  │
                    │  │ _bridge     │ _integration    │  │
                    │  └─────────────┴─────────────────┘  │
                    └─────────────────┬───────────────────┘
                                      │
                    ┌─────────────────▼───────────────────┐
                    │     Network System Core             │
                    │  ┌───────┬─────────┬─────────────┐  │
                    │  │ Core  │ Session │ Internal    │  │
                    │  │ API   │ Mgmt    │ Impl        │  │
                    │  └───────┴─────────┴─────────────┘  │
                    └─────────────────┬───────────────────┘
                                      │
                    ┌─────────────────▼───────────────────┐
                    │     외부 종속성                      │
                    │  ASIO │ fmt │ thread_system │ etc.  │
                    └─────────────────────────────────────┘
```

### 2. 네임스페이스 설계

```cpp
namespace network_system {
    // 핵심 API
    namespace core {
        class messaging_client;
        class messaging_server;
        class network_config;
    }

    // 세션 관리
    namespace session {
        class messaging_session;
        class session_manager;
        class connection_pool;
    }

    // 내부 구현 (외부로 노출되지 않음)
    namespace internal {
        class tcp_socket;
        class send_coroutine;
        class message_pipeline;
        class protocol_handler;
    }

    // 시스템 통합
    namespace integration {
        class messaging_bridge;
        class container_integration;
        class thread_integration;

        // 호환성 네임스페이스 (기존 코드용)
        namespace compat {
            namespace network_module = network_system;
        }
    }

    // 유틸리티
    namespace utils {
        class network_utils;
        class address_resolver;
        class performance_monitor;
    }
}
```

---

## 🔧 핵심 구성 요소 구현

### 1. messaging_bridge 클래스

```cpp
// include/network_system/integration/messaging_bridge.h
#pragma once

#include "network_system/core/messaging_client.h"
#include "network_system/core/messaging_server.h"
#include "network_system/utils/performance_monitor.h"

#ifdef BUILD_WITH_CONTAINER_SYSTEM
#include "container_system/value_container.h"
#endif

#ifdef BUILD_WITH_THREAD_SYSTEM
#include "thread_system/thread_pool.h"
#endif

namespace network_system::integration {

class messaging_bridge {
public:
    // 생성자/소멸자
    explicit messaging_bridge(const std::string& bridge_id = "default");
    ~messaging_bridge();

    // 복사 및 이동 생성자 삭제 (싱글톤 패턴)
    messaging_bridge(const messaging_bridge&) = delete;
    messaging_bridge& operator=(const messaging_bridge&) = delete;
    messaging_bridge(messaging_bridge&&) = delete;
    messaging_bridge& operator=(messaging_bridge&&) = delete;

    // 서버/클라이언트 팩토리
    std::shared_ptr<core::messaging_server> create_server(
        const std::string& server_id,
        const core::server_config& config = {}
    );

    std::shared_ptr<core::messaging_client> create_client(
        const std::string& client_id,
        const core::client_config& config = {}
    );

    // 호환성 API (기존 messaging_system 코드용)
    namespace compat {
        using messaging_server = core::messaging_server;
        using messaging_client = core::messaging_client;
        using messaging_session = session::messaging_session;

        // 기존과 동일한 함수 시그니처
        std::shared_ptr<messaging_server> make_messaging_server(
            const std::string& server_id
        );
        std::shared_ptr<messaging_client> make_messaging_client(
            const std::string& client_id
        );
    }

#ifdef BUILD_WITH_CONTAINER_SYSTEM
    // Container System 통합
    void set_container_factory(
        std::shared_ptr<container_system::value_factory> factory
    );

    void enable_container_serialization(bool enable = true);

    template<typename MessageHandler>
    void set_container_message_handler(MessageHandler&& handler) {
        static_assert(std::is_invocable_v<MessageHandler,
            std::shared_ptr<container_system::value_container>>,
            "Handler must accept std::shared_ptr<container_system::value_container>");

        container_message_handler_ = std::forward<MessageHandler>(handler);
    }
#endif

#ifdef BUILD_WITH_THREAD_SYSTEM
    // Thread System 통합
    void set_thread_pool(
        std::shared_ptr<thread_system::thread_pool> pool
    );

    void set_async_mode(bool async = true);
#endif

    // 구성 관리
    struct bridge_config {
        bool enable_performance_monitoring = true;
        bool enable_message_logging = false;
        bool enable_connection_pooling = true;
        size_t max_connections_per_client = 10;
        std::chrono::milliseconds connection_timeout{30000};
        std::chrono::milliseconds message_timeout{5000};
        size_t message_buffer_size = 8192;
        bool enable_compression = false;
        bool enable_encryption = false;
    };

    void set_config(const bridge_config& config);
    bridge_config get_config() const;

    // 성능 모니터링
    struct performance_metrics {
        // 연결 통계
        std::atomic<uint64_t> total_connections{0};
        std::atomic<uint64_t> active_connections{0};
        std::atomic<uint64_t> failed_connections{0};

        // 메시지 통계
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};

        // 성능 통계
        std::atomic<uint64_t> avg_latency_us{0};
        std::atomic<uint64_t> max_latency_us{0};
        std::atomic<uint64_t> min_latency_us{UINT64_MAX};

        // 오류 통계
        std::atomic<uint64_t> send_errors{0};
        std::atomic<uint64_t> receive_errors{0};
        std::atomic<uint64_t> timeout_errors{0};

        // 시간 정보
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update;
    };

    performance_metrics get_metrics() const;
    void reset_metrics();

    // 이벤트 핸들러
    using connection_event_handler = std::function<void(
        const std::string& endpoint, bool connected)>;
    using error_event_handler = std::function<void(
        const std::string& error_msg, int error_code)>;

    void set_connection_event_handler(connection_event_handler handler);
    void set_error_event_handler(error_event_handler handler);

    // 생명주기 관리
    void start();
    void stop();
    bool is_running() const;

    // 디버깅 및 진단
    std::string get_status_report() const;
    void enable_debug_logging(bool enable = true);

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace network_system::integration
```

### 2. 핵심 API 설계

```cpp
// include/network_system/core/messaging_server.h
#pragma once

#include "network_system/session/messaging_session.h"
#include <functional>
#include <memory>
#include <string>

namespace network_system::core {

struct server_config {
    unsigned short port = 8080;
    std::string bind_address = "0.0.0.0";
    size_t max_connections = 1000;
    size_t message_size_limit = 1024 * 1024; // 1MB
    std::chrono::seconds timeout{30};
    bool enable_keep_alive = true;
    bool enable_nodelay = true;
    size_t receive_buffer_size = 8192;
    size_t send_buffer_size = 8192;
};

class messaging_server {
public:
    // 생성자
    explicit messaging_server(const std::string& server_id);
    messaging_server(const std::string& server_id, const server_config& config);

    // 소멸자
    ~messaging_server();

    // 복사/이동 삭제
    messaging_server(const messaging_server&) = delete;
    messaging_server& operator=(const messaging_server&) = delete;
    messaging_server(messaging_server&&) = delete;
    messaging_server& operator=(messaging_server&&) = delete;

    // 서버 생명주기
    bool start_server();
    bool start_server(unsigned short port);
    bool start_server(const std::string& bind_address, unsigned short port);
    void stop_server();
    void wait_for_stop();

    // 상태 쿼리
    bool is_running() const;
    std::string server_id() const;
    unsigned short port() const;
    std::string bind_address() const;
    size_t connection_count() const;

    // 구성 관리
    void set_config(const server_config& config);
    server_config get_config() const;

    // 이벤트 핸들러 설정
    using message_handler = std::function<void(
        std::shared_ptr<session::messaging_session>,
        const std::string&)>;
    using connect_handler = std::function<void(
        std::shared_ptr<session::messaging_session>)>;
    using disconnect_handler = std::function<void(
        std::shared_ptr<session::messaging_session>,
        const std::string&)>;
    using error_handler = std::function<void(
        const std::string&, int)>;

    void set_message_handler(message_handler handler);
    void set_connect_handler(connect_handler handler);
    void set_disconnect_handler(disconnect_handler handler);
    void set_error_handler(error_handler handler);

    // 메시지 브로드캐스트
    void broadcast_message(const std::string& message);
    void broadcast_to_group(const std::string& group_id, const std::string& message);

    // 세션 관리
    std::shared_ptr<session::messaging_session> get_session(
        const std::string& session_id);
    std::vector<std::shared_ptr<session::messaging_session>> get_all_sessions();
    void disconnect_session(const std::string& session_id);
    void disconnect_all_sessions();

    // 그룹 관리
    void add_session_to_group(const std::string& session_id,
                              const std::string& group_id);
    void remove_session_from_group(const std::string& session_id,
                                   const std::string& group_id);
    std::vector<std::string> get_session_groups(const std::string& session_id);

    // 통계
    struct server_statistics {
        uint64_t total_connections = 0;
        uint64_t current_connections = 0;
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_activity;
    };

    server_statistics get_statistics() const;
    void reset_statistics();

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace network_system::core
```

### 3. 세션 관리

```cpp
// include/network_system/session/messaging_session.h
#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace network_system::session {

enum class session_state {
    connecting,
    connected,
    disconnecting,
    disconnected,
    error
};

class messaging_session {
public:
    // 소멸자
    virtual ~messaging_session() = default;

    // 메시지 송수신
    virtual void send_message(const std::string& message) = 0;
    virtual void send_raw_data(const std::vector<uint8_t>& data) = 0;
    virtual void send_binary_message(const void* data, size_t size) = 0;

    // 세션 정보
    virtual std::string session_id() const = 0;
    virtual std::string client_id() const = 0;
    virtual std::string remote_address() const = 0;
    virtual unsigned short remote_port() const = 0;
    virtual std::string local_address() const = 0;
    virtual unsigned short local_port() const = 0;

    // 연결 상태
    virtual session_state state() const = 0;
    virtual bool is_connected() const = 0;
    virtual std::chrono::steady_clock::time_point connect_time() const = 0;
    virtual std::chrono::steady_clock::time_point last_activity() const = 0;

    // 세션 관리
    virtual void disconnect() = 0;
    virtual void disconnect(const std::string& reason) = 0;

    // 세션 데이터 (키-값 저장소)
    virtual void set_session_data(const std::string& key,
                                  const std::string& value) = 0;
    virtual std::string get_session_data(const std::string& key) const = 0;
    virtual bool has_session_data(const std::string& key) const = 0;
    virtual void remove_session_data(const std::string& key) = 0;
    virtual void clear_session_data() = 0;

    // 세션 그룹
    virtual void join_group(const std::string& group_id) = 0;
    virtual void leave_group(const std::string& group_id) = 0;
    virtual std::vector<std::string> get_groups() const = 0;
    virtual bool is_in_group(const std::string& group_id) const = 0;

    // 통계
    struct session_statistics {
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t errors_count = 0;
        std::chrono::steady_clock::time_point last_message_time;
    };

    virtual session_statistics get_statistics() const = 0;
    virtual void reset_statistics() = 0;

    // 이벤트 핸들러
    using message_handler = std::function<void(const std::string&)>;
    using state_change_handler = std::function<void(session_state, session_state)>;
    using error_handler = std::function<void(const std::string&, int)>;

    virtual void set_message_handler(message_handler handler) = 0;
    virtual void set_state_change_handler(state_change_handler handler) = 0;
    virtual void set_error_handler(error_handler handler) = 0;

protected:
    messaging_session() = default;
};

// 팩토리 함수
std::shared_ptr<messaging_session> create_tcp_session(
    const std::string& session_id,
    const std::string& client_id
);

} // namespace network_system::session
```

---

## 🔗 종속성 관리

### 1. CMake 모듈 파일

```cmake
# cmake/FindNetworkSystem.cmake
find_package(PkgConfig QUIET)

# ASIO 종속성
find_package(asio CONFIG REQUIRED)
if(NOT asio_FOUND)
    message(FATAL_ERROR "ASIO library is required for NetworkSystem")
endif()

# fmt 종속성
find_package(fmt CONFIG REQUIRED)
if(NOT fmt_FOUND)
    message(FATAL_ERROR "fmt library is required for NetworkSystem")
endif()

# 조건부 종속성 확인
if(BUILD_WITH_CONTAINER_SYSTEM)
    find_package(ContainerSystem CONFIG)
    if(NOT ContainerSystem_FOUND)
        message(WARNING "ContainerSystem not found, container integration disabled")
        set(BUILD_WITH_CONTAINER_SYSTEM OFF)
    endif()
endif()

if(BUILD_WITH_THREAD_SYSTEM)
    find_package(ThreadSystem CONFIG)
    if(NOT ThreadSystem_FOUND)
        message(WARNING "ThreadSystem not found, thread integration disabled")
        set(BUILD_WITH_THREAD_SYSTEM OFF)
    endif()
endif()

# 라이브러리 타겟 정의
if(NOT TARGET NetworkSystem::Core)
    add_library(NetworkSystem::Core INTERFACE)
    target_include_directories(NetworkSystem::Core INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/../include
    )
    target_link_libraries(NetworkSystem::Core INTERFACE
        asio::asio
        fmt::fmt
    )

    if(BUILD_WITH_CONTAINER_SYSTEM)
        target_link_libraries(NetworkSystem::Core INTERFACE
            ContainerSystem::Core
        )
        target_compile_definitions(NetworkSystem::Core INTERFACE
            BUILD_WITH_CONTAINER_SYSTEM
        )
    endif()

    if(BUILD_WITH_THREAD_SYSTEM)
        target_link_libraries(NetworkSystem::Core INTERFACE
            ThreadSystem::Core
        )
        target_compile_definitions(NetworkSystem::Core INTERFACE
            BUILD_WITH_THREAD_SYSTEM
        )
    endif()
endif()

# 변수 설정
set(NetworkSystem_FOUND TRUE)
set(NetworkSystem_VERSION "2.0.0")
set(NetworkSystem_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../include)
```

### 2. pkg-config 파일

```ini
# network-system.pc.in
prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@

Name: NetworkSystem
Description: 고성능 모듈식 네트워크 시스템
Version: @PROJECT_VERSION@
Requires: asio fmt
Requires.private: @PRIVATE_DEPENDENCIES@
Libs: -L${libdir} -lNetworkSystem
Libs.private: @PRIVATE_LIBS@
Cflags: -I${includedir}
```

---

## 🧪 테스트 프레임워크

### 1. 단위 테스트 구조

```cpp
// tests/unit/core/test_messaging_server.cpp
#include <gtest/gtest.h>
#include "network_system/core/messaging_server.h"
#include "network_system/core/messaging_client.h"
#include "test_utils/network_test_utils.h"

namespace network_system::test {

class MessagingServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_config config;
        config.port = test_utils::get_free_port();
        config.max_connections = 10;

        server_ = std::make_shared<core::messaging_server>("test_server", config);

        // 테스트 핸들러 설정
        server_->set_message_handler([this](auto session, const std::string& msg) {
            received_messages_.push_back(msg);
            last_session_ = session;
        });

        server_->set_connect_handler([this](auto session) {
            connected_sessions_.push_back(session->session_id());
        });

        server_->set_disconnect_handler([this](auto session, const std::string& reason) {
            disconnected_sessions_.push_back(session->session_id());
            disconnect_reasons_.push_back(reason);
        });
    }

    void TearDown() override {
        if (server_ && server_->is_running()) {
            server_->stop_server();
        }
        server_.reset();
    }

    std::shared_ptr<core::messaging_server> server_;
    std::vector<std::string> received_messages_;
    std::vector<std::string> connected_sessions_;
    std::vector<std::string> disconnected_sessions_;
    std::vector<std::string> disconnect_reasons_;
    std::shared_ptr<session::messaging_session> last_session_;
};

TEST_F(MessagingServerTest, ServerStartStop) {
    EXPECT_FALSE(server_->is_running());

    EXPECT_TRUE(server_->start_server());
    EXPECT_TRUE(server_->is_running());

    server_->stop_server();
    EXPECT_FALSE(server_->is_running());
}

TEST_F(MessagingServerTest, ClientConnection) {
    ASSERT_TRUE(server_->start_server());

    auto client = std::make_shared<core::messaging_client>("test_client");

    bool connected = false;
    client->set_connect_handler([&connected]() {
        connected = true;
    });

    EXPECT_TRUE(client->start_client("127.0.0.1", server_->port()));

    // 연결 대기
    test_utils::wait_for_condition([&connected]() { return connected; },
                                   std::chrono::seconds(5));

    EXPECT_TRUE(connected);
    EXPECT_EQ(1, connected_sessions_.size());
    EXPECT_EQ(1, server_->connection_count());
}

TEST_F(MessagingServerTest, MessageExchange) {
    ASSERT_TRUE(server_->start_server());

    auto client = std::make_shared<core::messaging_client>("test_client");

    std::string received_message;
    client->set_message_handler([&received_message](const std::string& msg) {
        received_message = msg;
    });

    ASSERT_TRUE(client->start_client("127.0.0.1", server_->port()));

    // 연결 대기
    test_utils::wait_for_condition([this]() {
        return !connected_sessions_.empty();
    }, std::chrono::seconds(5));

    // 클라이언트에서 서버로 메시지 전송
    const std::string test_message = "Hello, Server!";
    client->send_message(test_message);

    // 메시지 수신 대기
    test_utils::wait_for_condition([this]() {
        return !received_messages_.empty();
    }, std::chrono::seconds(5));

    EXPECT_EQ(1, received_messages_.size());
    EXPECT_EQ(test_message, received_messages_[0]);

    // 서버에서 클라이언트로 응답 전송
    const std::string response_message = "Hello, Client!";
    ASSERT_NE(nullptr, last_session_);
    last_session_->send_message(response_message);

    // 응답 수신 대기
    test_utils::wait_for_condition([&received_message, &response_message]() {
        return received_message == response_message;
    }, std::chrono::seconds(5));

    EXPECT_EQ(response_message, received_message);
}

TEST_F(MessagingServerTest, MaxConnectionsLimit) {
    server_config config = server_->get_config();
    config.max_connections = 2;
    server_->set_config(config);

    ASSERT_TRUE(server_->start_server());

    // 최대 연결까지 클라이언트 생성
    std::vector<std::shared_ptr<core::messaging_client>> clients;
    for (int i = 0; i < 3; ++i) {
        auto client = std::make_shared<core::messaging_client>(
            "test_client_" + std::to_string(i));
        clients.push_back(client);

        client->start_client("127.0.0.1", server_->port());
    }

    // 잠시 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 최대 연결이 초과되지 않았는지 확인
    EXPECT_LE(server_->connection_count(), 2);
}

} // namespace network_system::test
```

### 2. 통합 테스트

통합 테스트는 container_system 및 thread_system과의 통합을 검증하는 데 중점을 둡니다.

---

## 📊 성능 모니터링

### 1. 성능 메트릭 수집

```cpp
// include/network_system/utils/performance_monitor.h
#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace network_system::utils {

class performance_monitor {
public:
    struct metrics {
        // 연결 메트릭
        std::atomic<uint64_t> total_connections{0};
        std::atomic<uint64_t> active_connections{0};
        std::atomic<uint64_t> failed_connections{0};
        std::atomic<uint64_t> connections_per_second{0};

        // 메시지 메트릭
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> messages_per_second{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<uint64_t> bytes_per_second{0};

        // 지연시간 메트릭 (마이크로초)
        std::atomic<uint64_t> avg_latency_us{0};
        std::atomic<uint64_t> min_latency_us{UINT64_MAX};
        std::atomic<uint64_t> max_latency_us{0};
        std::atomic<uint64_t> p95_latency_us{0};
        std::atomic<uint64_t> p99_latency_us{0};

        // 오류 메트릭
        std::atomic<uint64_t> send_errors{0};
        std::atomic<uint64_t> receive_errors{0};
        std::atomic<uint64_t> timeout_errors{0};
        std::atomic<uint64_t> connection_errors{0};

        // 메모리 메트릭
        std::atomic<uint64_t> memory_usage_bytes{0};
        std::atomic<uint64_t> buffer_pool_size{0};
        std::atomic<uint64_t> peak_memory_usage{0};

        // 시간 정보
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update;

        // 가동 시간 계산
        std::chrono::milliseconds uptime() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
        }
    };

    static performance_monitor& instance();

    // 메트릭 업데이트
    void record_connection(bool success);
    void record_disconnection();
    void record_message_sent(size_t bytes);
    void record_message_received(size_t bytes);
    void record_latency(std::chrono::microseconds latency);
    void record_error(const std::string& error_type);
    void record_memory_usage(size_t bytes);

    // 메트릭 쿼리
    metrics get_metrics() const;
    void reset_metrics();

    // 실시간 통계 계산
    void update_realtime_stats();

    // JSON으로 메트릭 내보내기
    std::string to_json() const;

    // 성능 보고서 생성
    std::string generate_report() const;

private:
    performance_monitor();
    ~performance_monitor() = default;

    performance_monitor(const performance_monitor&) = delete;
    performance_monitor& operator=(const performance_monitor&) = delete;

    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace network_system::utils
```

### 2. 벤치마크 도구

벤치마크는 실제 성능 테스트를 통해 처리량, 지연시간 및 동시성을 측정합니다.

---

## 🛡️ 리소스 관리 패턴

network_system은 **95% RAII 준수**를 달성하며 모든 리소스(세션, 소켓, ASIO 객체, 스레드)는 RAII 및 스마트 포인터를 통해 관리됩니다.

### 스마트 포인터 전략

**세션 관리**: `std::shared_ptr<messaging_session>`과 `std::enable_shared_from_this` 사용
- 서버가 벡터에서 활성 세션 추적
- 비동기 ASIO 콜백이 세션을 유지
- 마지막 참조 삭제 시 자동 정리

**ASIO 리소스**: 배타적 소유권을 위한 `std::unique_ptr`
- 서버가 io_context와 acceptor를 독점 소유
- 예외 안전 리소스 관리
- 수동 `delete` 불필요

### 비동기 안전 패턴

```cpp
// 좋음: shared_ptr을 캡처하여 세션 유지
void start_session() {
    auto self = shared_from_this();
    socket_->start_read(
        [self](const std::vector<uint8_t>& data) {
            self->on_receive(data);  // 안전: self가 세션을 유지
        }
    );
}
```

### 적용된 모범 사례

✅ 모든 힙 할당에 스마트 포인터 (shared_ptr/unique_ptr)
✅ 모든 리소스에 RAII (생성자에서 획득, 소멸자에서 해제)
✅ 비동기 안전성을 위한 enable_shared_from_this
✅ 원자적 플래그와 뮤텍스를 통한 스레드 안전
✅ 예외 안전 소멸자 (noexcept)

---

## 🚨 오류 처리 전략

### Result<T> 패턴으로의 마이그레이션

**이전**: void 리턴과 콜백 기반 오류 처리
**이후**: 명시적 Result<void> 리턴

```cpp
// 이후
Result<void> start_server(unsigned short port) {
    if (is_running_) {
        return error<std::monostate>(
            codes::network_system::server_already_running,
            "Server already running"
        );
    }

    try {
        // 서버 시작 로직
        return ok();
    } catch (const std::system_error& e) {
        return error<std::monostate>(
            codes::network_system::bind_failed,
            "Failed to bind to port"
        );
    }
}
```

### 오류 코드 시스템

네트워크 시스템 오류 코드 (-600 ~ -699):
- **연결 오류** (-600~-619): connection_failed, connection_refused, connection_timeout
- **세션 오류** (-620~-639): session_not_found, session_expired
- **송수신 오류** (-640~-659): send_failed, receive_failed
- **서버 오류** (-660~-679): server_already_running, bind_failed

### 마이그레이션 이점

**타입 안전성**: 명시적 오류 리턴, 컴파일 타임 검사
**더 나은 컨텍스트**: 상세 메시지, 오류 원점 스택
**일관성**: 모든 시스템에서 동일한 패턴
**성능**: 로컬 작업 <2% 오버헤드

---

이 기술 구현 세부사항 문서는 network_system 분리 작업을 위한 포괄적인 가이드를 제공합니다. 다음 단계는 마이그레이션 스크립트를 생성하는 것입니다.
