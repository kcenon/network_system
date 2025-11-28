# Network System - 상세 기능

**언어:** [English](ARCHITECTURE.md) | **한국어**

**최종 업데이트**: 2025-11-28
**버전**: 1.0

이 문서는 Network System의 모든 기능에 대한 포괄적인 세부 정보를 제공합니다.

---

## 목차

- [핵심 기능](#핵심-기능)
- [서버 구현](#서버-구현)
- [클라이언트 구현](#클라이언트-구현)
- [세션 관리](#세션-관리)
- [메시지 처리](#메시지-처리)
- [성능 최적화](#성능-최적화)
- [보안 기능](#보안-기능)
- [통합 기능](#통합-기능)

---

## 핵심 기능

### 설계 목표

Network System은 다음 목표를 달성하도록 설계되었습니다:

| 목표 | 설명 | 상태 |
|------|------|------|
| **고성능** | 서브마이크로초 지연시간, 300K+ msg/s | ✅ |
| **제로 카피 I/O** | 네트워크 작업을 위한 직접 메모리 매핑 | ✅ |
| **모듈성** | messaging_system과 독립적 | ✅ |
| **비동기 우선** | 코루틴 기반 비동기 작업 (C++20) | ✅ |
| **통합 친화적** | 스레드, 로거, 컨테이너 시스템과 연결 가능 | ✅ |
| **연결 풀링** | 효율적인 연결 재사용 | 🔄 |
| **TLS/SSL 지원** | 보안 통신 | ✅ |

### 성능 특성

- **305K+ msg/s**: 초당 30만 메시지 처리
- **서브마이크로초 지연시간**: 평균 0.8μs
- **제로 카피**: 불필요한 메모리 복사 제거
- **효율적인 버퍼 관리**: 메모리 풀링 활용

---

## 서버 구현

### MessagingServer

다중 클라이언트를 지원하는 고성능 TCP 서버:

```cpp
#include <kcenon/network/messaging_server.h>

// 서버 생성
MessagingServer server("0.0.0.0", 8080);

// 콜백 등록
server.on_client_connected([](session_id id) {
    std::cout << "클라이언트 연결됨: " << id << std::endl;
});

server.on_client_disconnected([](session_id id) {
    std::cout << "클라이언트 연결 해제됨: " << id << std::endl;
});

server.on_message_received([](session_id id, const message& msg) {
    std::cout << "메시지 수신: " << msg.to_string() << std::endl;
    // 응답 처리
});

// 서버 시작
server.start();

// 특정 클라이언트에 메시지 전송
server.send(client_id, response_message);

// 모든 클라이언트에 브로드캐스트
server.broadcast(broadcast_message);

// 서버 종료
server.stop();
```

### 서버 옵션

```cpp
// 상세 옵션으로 서버 생성
MessagingServer server("0.0.0.0", 8080, {
    .io_threads = 4,                      // I/O 스레드 수
    .max_connections = 10000,             // 최대 동시 연결
    .connection_timeout = std::chrono::seconds(30),
    .keep_alive_interval = std::chrono::seconds(10),
    .receive_buffer_size = 64 * 1024,     // 64KB
    .send_buffer_size = 64 * 1024,
    .enable_tcp_nodelay = true,           // Nagle 알고리즘 비활성화
    .enable_keep_alive = true
});
```

---

## 클라이언트 구현

### MessagingClient

서버에 연결하는 TCP 클라이언트:

```cpp
#include <kcenon/network/messaging_client.h>

// 클라이언트 생성
MessagingClient client("server.example.com", 8080);

// 콜백 등록
client.on_connected([]() {
    std::cout << "서버에 연결됨" << std::endl;
});

client.on_disconnected([]() {
    std::cout << "서버와 연결 해제됨" << std::endl;
});

client.on_message_received([](const message& msg) {
    std::cout << "메시지 수신: " << msg.to_string() << std::endl;
});

// 연결
auto result = client.connect();
if (!result) {
    std::cerr << "연결 실패: " << result.error().message << std::endl;
    return;
}

// 메시지 전송
client.send(request_message);

// 동기 요청-응답
auto response = client.request(request_message, std::chrono::seconds(5));
if (response) {
    std::cout << "응답: " << response->to_string() << std::endl;
}

// 연결 해제
client.disconnect();
```

### 재연결 기능

```cpp
// 자동 재연결 활성화
MessagingClient client("server.example.com", 8080, {
    .auto_reconnect = true,
    .reconnect_interval = std::chrono::seconds(5),
    .max_reconnect_attempts = 10,
    .reconnect_backoff_multiplier = 2.0  // 지수 백오프
});

// 수동 재연결
if (!client.is_connected()) {
    client.reconnect();
}
```

---

## 세션 관리

### 세션 수명주기

```
연결 요청 → 세션 생성 → 활성 상태 → 연결 해제 → 세션 정리
    │           │           │           │           │
    ├── 실패 ──►│           │           │           │
    │           ├── 타임아웃 ──────────►│           │
    │           │           ├── 클라이언트 종료 ──►│
    │           │           ├── 서버 종료 ────────►│
    │           │           └── 오류 ────────────►│
```

### 세션 정보 접근

```cpp
server.on_client_connected([&server](session_id id) {
    // 세션 정보 조회
    auto session_info = server.get_session_info(id);
    if (session_info) {
        std::cout << "원격 주소: " << session_info->remote_address << std::endl;
        std::cout << "연결 시간: " << session_info->connected_at << std::endl;
    }
});

// 모든 활성 세션 조회
auto sessions = server.get_active_sessions();
for (const auto& session : sessions) {
    std::cout << "세션 " << session.id << ": " << session.remote_address << std::endl;
}
```

### 세션 관리 작업

```cpp
// 특정 세션 연결 해제
server.disconnect_session(session_id);

// 조건부 연결 해제
server.disconnect_if([](const session_info& info) {
    return info.idle_time() > std::chrono::minutes(30);
});

// 세션 속성 설정
server.set_session_attribute(session_id, "user_id", user_id);
auto user_id = server.get_session_attribute<std::string>(session_id, "user_id");
```

---

## 메시지 처리

### 메시지 구조

```cpp
#include <kcenon/network/message.h>

// 메시지 생성
message msg;
msg.set_type(message_type::request);
msg.set_id(generate_message_id());
msg.set_payload(container_data);

// 메시지 직렬화
auto bytes = msg.serialize();

// 메시지 역직렬화
auto parsed = message::deserialize(bytes);
if (parsed) {
    auto& msg = parsed.value();
    std::cout << "타입: " << static_cast<int>(msg.type()) << std::endl;
    std::cout << "페이로드: " << msg.payload().to_string() << std::endl;
}
```

### 메시지 타입

```cpp
enum class message_type : uint8_t {
    request = 0,      // 요청 메시지
    response = 1,     // 응답 메시지
    notification = 2, // 단방향 알림
    heartbeat = 3,    // 연결 유지 신호
    error = 4         // 오류 메시지
};
```

### 프레이밍 프로토콜

```
┌─────────────────────────────────────────────────────┐
│                   메시지 프레임                      │
├──────────┬──────────┬──────────┬───────────────────┤
│ 길이 (4) │ 타입 (1) │ ID (8)   │ 페이로드 (가변)   │
└──────────┴──────────┴──────────┴───────────────────┘
```

---

## 성능 최적화

### 제로 카피 파이프라인

```cpp
#include <kcenon/network/pipeline.h>

// 제로 카피 버퍼로 수신
server.on_message_received([](session_id id, span<const uint8_t> buffer) {
    // 버퍼는 복사 없이 직접 참조
    process_data(buffer);
});

// 제로 카피로 전송
auto buffer = acquire_send_buffer();
fill_buffer(buffer);
server.send_zero_copy(client_id, std::move(buffer));
```

### 버퍼 풀링

```cpp
#include <kcenon/network/buffer_pool.h>

// 버퍼 풀 생성
buffer_pool pool(1024, 1000);  // 1KB 버퍼 1000개

// 버퍼 획득
auto buffer = pool.acquire();
// 사용...
pool.release(std::move(buffer));

// RAII 스타일
{
    auto scoped_buffer = pool.acquire_scoped();
    // 스코프 종료 시 자동 반환
}
```

### I/O 멀티플렉싱

```cpp
// 최적의 I/O 모델 자동 선택
// - Linux: epoll
// - macOS: kqueue
// - Windows: IOCP

MessagingServer server("0.0.0.0", 8080, {
    .io_model = io_model::automatic,  // 자동 선택
    .io_threads = std::thread::hardware_concurrency()
});
```

---

## 보안 기능

### TLS/SSL 지원

```cpp
#include <kcenon/network/tls_config.h>

// TLS 서버 구성
tls_config server_tls {
    .certificate_file = "/path/to/server.crt",
    .private_key_file = "/path/to/server.key",
    .ca_file = "/path/to/ca.crt",
    .verify_mode = tls_verify_mode::peer,
    .min_protocol_version = tls_version::tls_1_2
};

MessagingServer secure_server("0.0.0.0", 8443, {
    .tls = server_tls
});

// TLS 클라이언트 구성
tls_config client_tls {
    .ca_file = "/path/to/ca.crt",
    .verify_mode = tls_verify_mode::peer,
    .verify_hostname = true
};

MessagingClient secure_client("server.example.com", 8443, {
    .tls = client_tls
});
```

### 인증

```cpp
// 커스텀 인증 핸들러
server.set_auth_handler([](const auth_request& req) -> auth_result {
    // API 키 검증
    if (validate_api_key(req.api_key)) {
        return auth_result::success(user_info);
    }
    return auth_result::failure("잘못된 API 키");
});

// 클라이언트 인증
client.authenticate({
    .api_key = "my-api-key",
    .metadata = {{"client_version", "1.0.0"}}
});
```

### 속도 제한

```cpp
#include <kcenon/network/rate_limiter.h>

// 속도 제한 설정
server.set_rate_limiter(create_rate_limiter({
    .requests_per_second = 1000,
    .burst_size = 100,
    .per_client = true
}));
```

---

## 통합 기능

### thread_system 통합

```cpp
#include <kcenon/network/integration/thread_integration.h>

// 스레드 풀 공유
auto pool = create_thread_pool(8);
MessagingServer server("0.0.0.0", 8080, {
    .thread_pool = pool
});

// 콜백이 스레드 풀에서 실행됨
server.on_message_received([](session_id id, const message& msg) {
    // 자동으로 스레드 풀에서 실행
    process_message(msg);
});
```

### container_system 통합

```cpp
#include <kcenon/network/integration/container_integration.h>

// 컨테이너를 메시지로 직접 전송
container data;
data.set("action", "update");
data.set("value", 42);

server.send_container(client_id, data);

// 컨테이너로 수신
server.on_container_received([](session_id id, const container& data) {
    auto action = data.get<std::string>("action");
    auto value = data.get<int>("value");
});
```

### logger_system 통합

```cpp
#include <kcenon/network/integration/logger_integration.h>

// 로거 연결
auto logger = create_logger("network");
server.set_logger(logger);

// 자동 로깅
// - 연결/연결 해제 이벤트
// - 오류 및 경고
// - 성능 메트릭 (옵션)
```

### monitoring_system 통합

```cpp
#include <kcenon/network/integration/monitoring_integration.h>

// 메트릭 수집기 연결
auto metrics = create_metrics_collector();
server.set_metrics_collector(metrics);

// 수집되는 메트릭:
// - network_connections_active
// - network_bytes_received_total
// - network_bytes_sent_total
// - network_messages_received_total
// - network_messages_sent_total
// - network_request_duration_seconds
```

---

## 벤치마크 결과

### 처리량 테스트

| 시나리오 | 처리량 | 지연시간 (p50) | 지연시간 (p99) |
|----------|--------|----------------|----------------|
| 작은 메시지 (64B) | 500K msg/s | 0.5μs | 2μs |
| 중간 메시지 (1KB) | 305K msg/s | 0.8μs | 3μs |
| 큰 메시지 (64KB) | 50K msg/s | 10μs | 50μs |

### 연결 테스트

| 메트릭 | 값 |
|--------|-----|
| 최대 동시 연결 | 100K+ |
| 연결 설정 시간 | < 1ms |
| 연결 해제 시간 | < 500μs |

### 리소스 사용

| 연결 수 | CPU 사용 | 메모리 사용 |
|---------|----------|------------|
| 1,000 | 5% | 50MB |
| 10,000 | 15% | 200MB |
| 100,000 | 40% | 1.5GB |

---

## 참고사항

### 스레드 안전성

- **MessagingServer**: 스레드 안전 (콜백은 스레드 풀에서 실행)
- **MessagingClient**: 스레드 안전 (내부 동기화)
- **message**: 불변 (공유 안전)

### 베스트 프랙티스

1. **연결 재사용**: 빈번한 연결/해제 피하기
2. **배치 처리**: 작은 메시지는 배치로 전송
3. **버퍼 크기 조정**: 워크로드에 맞게 버퍼 크기 조정
4. **TCP_NODELAY**: 저지연이 필요한 경우 활성화

---

**최종 업데이트**: 2025-11-28
**버전**: 1.0

---

Made with ❤️ by 🍀☀🌕🌥 🌊
