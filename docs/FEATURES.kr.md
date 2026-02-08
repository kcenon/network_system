# Network System - 상세 기능

**언어:** [English](ARCHITECTURE.md) | **한국어**

**최종 업데이트**: 2026-02-08
**버전**: 0.2.0

이 문서는 Network System의 모든 기능에 대한 포괄적인 세부 정보를 제공합니다.

---

## 목차

- [핵심 기능](#핵심-기능)
- [gRPC 프로토콜](#grpc-프로토콜-프로토타입)
- [Facade API](#facade-api)
- [통합 인터페이스 계층](#통합-인터페이스-계층)
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

## gRPC 프로토콜 (프로토타입)

**구현 상태**: HTTP/2 전송 기반의 프로토타입 gRPC 프레임워크로, 4가지 RPC 패턴을 모두 지원합니다.

**참고**: 헤더에 "프로덕션 사용 시 공식 gRPC 라이브러리 래핑을 고려하세요"라고 명시되어 있습니다. `NETWORK_GRPC_OFFICIAL` 컴파일 플래그를 통해 공식 gRPC C++ 라이브러리와의 선택적 통합이 가능합니다.

**기능**:
- **RPC 패턴**: Unary, Server Streaming, Client Streaming, Bidirectional Streaming
- **클라이언트** (`grpc_client`):
  - 동기/비동기 Unary 호출 (`call_raw`, `call_raw_async`)
  - 모든 스트리밍 패턴의 리더/라이터
  - 채널 설정: TLS, 킵얼라이브, 재시도, 최대 메시지 크기
  - 호출별 옵션: 데드라인, 메타데이터, 압축, wait-for-ready
- **서버** (`grpc_server`):
  - 4가지 RPC 타입의 메서드 핸들러 등록
  - `server_context`: 클라이언트 메타데이터, 취소, 데드라인, 피어 정보, 인증 컨텍스트
  - TLS 및 상호 TLS 지원 (`start_tls()`)
  - 동시 스트림, 메시지 크기, 킵얼라이브, 연결 제한, 워커 스레드 설정
- **서비스 레지스트리** (`service_registry`):
  - `generic_service`: protobuf 없이 런타임 메서드 등록
  - `protoc_service_adapter`: protoc 생성 서비스 어댑터 (`NETWORK_GRPC_OFFICIAL` 필요)
  - 서비스 조회 및 전체 경로 기반 메서드 라우팅
  - 리플렉션 지원 (grpcurl 등 디버깅 도구용)
  - 헬스 체크 서비스 (`health_service`): 표준 gRPC 헬스 체크 프로토콜 구현

**주요 클래스**:

| 클래스 | 설명 |
|--------|------|
| `grpc_client` | 동기/비동기 Unary 및 스트리밍 호출 |
| `grpc_server` | 핸들러 등록 및 TLS 지원 서버 |
| `service_registry` | 서비스 관리 및 라우팅 중앙 레지스트리 |
| `generic_service` | 런타임 메서드 등록을 위한 동적 서비스 |
| `protoc_service_adapter` | protoc 생성 서비스 어댑터 |
| `health_service` | 표준 gRPC 헬스 체크 구현 |

**사용 예**:
```cpp
using namespace kcenon::network::protocols::grpc;

// 서버 설정
grpc_server server({.max_concurrent_streams = 100, .num_threads = 4});

server.register_unary_method("/mypackage.MyService/Echo",
    [](server_context& ctx, const std::vector<uint8_t>& request)
        -> std::pair<grpc_status, std::vector<uint8_t>> {
        return {grpc_status::ok_status(), request};  // Echo back
    });

server.start(50051);

// 클라이언트 설정
grpc_client client("localhost:50051", {.use_tls = false});
client.connect();

auto result = client.call_raw("/mypackage.MyService/Echo", request_data);
```

---

## Facade API

Facade API는 프로토콜 클라이언트 및 서버 생성을 위한 단순화된 통합 인터페이스를 제공합니다. 지원되는 각 프로토콜(TCP, UDP, HTTP, WebSocket, QUIC)에는 동일한 설계 패턴을 따르는 전용 Facade 클래스가 있습니다.

**설계 목표**:

| 목표 | 설명 | 이점 |
|------|------|------|
| **단순성** | 템플릿 매개변수나 프로토콜 태그 불필요 | 학습 및 사용 용이 |
| **일관성** | 모든 프로토콜에서 동일한 `create_client`/`create_server` 패턴 | 인지 부하 감소 |
| **타입 안전성** | 표준 `i_protocol_client`/`i_protocol_server` 인터페이스 반환 | 프로토콜 독립 코드 |
| **제로 비용** | 직접 인스턴스화 대비 성능 오버헤드 없음 | 프로덕션 준비 완료 |

### 사용 가능한 Facade

| Facade | 헤더 | 프로토콜 | SSL/TLS | 연결 풀 |
|--------|------|----------|---------|---------|
| `tcp_facade` | `<kcenon/network/facade/tcp_facade.h>` | TCP | 지원 | 지원 |
| `udp_facade` | `<kcenon/network/facade/udp_facade.h>` | UDP | 미지원 | 미지원 |
| `http_facade` | `<kcenon/network/facade/http_facade.h>` | HTTP/1.1 | 지원 | 미지원 |
| `websocket_facade` | `<kcenon/network/facade/websocket_facade.h>` | WebSocket | 미지원 | 미지원 |
| `quic_facade` | `<kcenon/network/facade/quic_facade.h>` | QUIC | 내장 (TLS 1.3) | 미지원 |

### 빠른 예제

```cpp
#include <kcenon/network/facade/tcp_facade.h>
using namespace kcenon::network::facade;

// TCP 클라이언트 생성 (일반 또는 보안)
tcp_facade tcp;
auto client = tcp.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .client_id = "my-client",
    .timeout = std::chrono::seconds(30),
    .use_ssl = false
});

// TCP 서버 생성
auto server = tcp.create_server({
    .port = 8080,
    .server_id = "my-server"
});

// i_protocol_client를 통한 프로토콜 독립 사용
client->set_receive_callback([](const std::vector<uint8_t>& data) {
    std::cout << "Received " << data.size() << " bytes\n";
});
client->start("127.0.0.1", 8080);
```

### 직접 클래스 사용 시점

Facade는 일반적인 사용 사례를 다룹니다. 프로토콜 고유 기능이 필요한 경우 직접 클래스를 사용합니다:

- **TCP**: 고급 TLS 설정, 사용자 정의 암호 모음, 직접 세션 제어
- **WebSocket**: 텍스트 프레임 처리, 프로토콜 확장, 프래그멘테이션 제어
- **HTTP**: 라우팅, 쿠키, 멀티파트 폼, 사용자 정의 헤더
- **QUIC**: 멀티 스트림, 스트림 우선순위, 0-RTT 재개, 연결 마이그레이션

자세한 Facade 문서는 [Facade API 레퍼런스](facades/README.md)를 참조하세요.
마이그레이션 가이드는 [Facade 마이그레이션 가이드](facades/migration-guide.md)를 참조하세요.

---

## 통합 인터페이스 계층

통합 인터페이스 계층은 네트워크 전송, 연결 및 리스너에 대한 프로토콜 독립 추상화를 제공합니다. `kcenon/network/detail/unified/`에 위치한 이 인터페이스들은 프로토콜별 세부 사항에 의존하지 않고 어떤 네트워크 프로토콜과도 작동하는 코드를 작성할 수 있게 합니다.

### 아키텍처

계층은 상속 계층을 형성하는 3개의 핵심 인터페이스로 구성됩니다:

```
i_transport           (기본: 데이터 전송, 상태 조회, 엔드포인트 정보)
    |
    v
i_connection          (i_transport 확장: 연결, 종료, 콜백, 옵션)

i_listener            (독립: 수신, 수락, 브로드캐스트, 연결 관리)
```

### `i_transport` -- 데이터 전송 추상화

**헤더**: `<kcenon/network/detail/unified/i_transport.h>`

모든 데이터 전송의 기본 인터페이스입니다. 모든 프로토콜 구현이 공유하는 최소한의 연산 집합을 제공합니다.

**주요 연산**:
- `send(std::span<const std::byte>)` -- 원격 엔드포인트에 원시 데이터 전송
- `is_connected()` -- 연결 상태 확인
- `id()` -- 고유 전송 식별자 반환
- `remote_endpoint()` / `local_endpoint()` -- 엔드포인트 정보 반환

### `i_connection` -- 활성 연결 인터페이스

**헤더**: `<kcenon/network/detail/unified/i_connection.h>`

`i_transport`를 연결 수명 주기 연산으로 확장합니다. 클라이언트 시작 연결과 서버 수락 연결 모두를 나타냅니다.

**주요 연산**:
- `connect(endpoint_info)` / `connect(url)` -- 원격 엔드포인트에 연결
- `close()` -- 정상 종료
- `set_callbacks(connection_callbacks)` -- 이벤트 핸들러 등록 (on_connected, on_data, on_disconnected, on_error)
- `set_options(connection_options)` -- 타임아웃, 킵얼라이브, no-delay 설정
- `is_connecting()` / `wait_for_stop()` -- 상태 조회

### `i_listener` -- 서버 측 리스너 인터페이스

**헤더**: `<kcenon/network/detail/unified/i_listener.h>`

수신 연결을 대기하는 서버 측 컴포넌트를 나타냅니다.

**주요 연산**:
- `start(endpoint_info)` / `start(port)` -- 바인드 및 수신 대기
- `stop()` -- 수신 중단 및 모든 활성 연결 종료
- `set_callbacks(listener_callbacks)` -- 이벤트 핸들러 등록 (on_accept, on_data, on_disconnect, on_error)
- `send_to(connection_id, data)` -- 특정 연결에 전송
- `broadcast(data)` -- 모든 연결된 클라이언트에 전송
- `connection_count()` -- 활성 연결 수 반환

### 지원 타입

**헤더**: `<kcenon/network/detail/unified/types.h>`

| 타입 | 용도 |
|------|------|
| `endpoint_info` | 네트워크 엔드포인트 (호스트/포트 또는 URL) |
| `connection_callbacks` | 연결 이벤트 콜백 구조체 |
| `listener_callbacks` | 리스너/서버 이벤트 콜백 구조체 |
| `connection_options` | 설정: 타임아웃, 킵얼라이브, no-delay |

### 프로토콜 독립 예제

```cpp
#include <kcenon/network/detail/unified/i_connection.h>

using namespace kcenon::network::unified;

// 어떤 프로토콜 구현과도 작동
void send_message(i_transport& transport, std::span<const std::byte> data) {
    if (!transport.is_connected()) {
        return;
    }
    auto result = transport.send(data);
    if (!result) {
        std::cerr << "Send failed\n";
    }
}
```

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

**최종 업데이트**: 2026-02-08
**버전**: 0.2.0

---

Made with ❤️ by 🍀☀🌕🌥 🌊
