# Network System - 미구현 기능 목록

> **Language:** [English](TODO.md) | **한국어**

## 개요

이 문서는 network system에서 계획되었지만 아직 구현되지 않은 기능들을 추적합니다. 기능은 우선순위와 예상 구현 소요 시간에 따라 구성됩니다.

**최종 업데이트:** 2025-01-26
**버전:** 1.4.0

---

## 높은 우선순위 기능 (P2)

### ~~1. 클라이언트 재연결 로직~~ ✅ 완료

**상태:** v1.5.0에서 구현됨
**우선순위:** P2
**실제 소요:** 2일
**완료일:** 2025-01-26

**설명:**
네트워크 중단을 우아하게 처리하기 위해 `messaging_client` 및 `secure_messaging_client`에 대한 지수 백오프를 사용한 자동 재연결 로직을 추가합니다.

**제안된 구현:**
```cpp
class resilient_client {
public:
    resilient_client(const std::string& host, unsigned short port,
                    size_t max_retries = 3,
                    std::chrono::milliseconds initial_backoff = std::chrono::seconds(1));

    auto send_with_retry(std::vector<uint8_t>&& data) -> VoidResult;
    auto reconnect() -> VoidResult;
    auto set_reconnect_callback(std::function<void(size_t attempt)> callback) -> void;

private:
    std::unique_ptr<messaging_client> client_;
    size_t max_retries_;
    std::chrono::milliseconds initial_backoff_;
    std::function<void(size_t)> reconnect_callback_;
};
```

**이점:**
- 네트워크 장애로부터 자동 복구
- 지수 백오프로 연결 폭주 방지
- 구성 가능한 재시도 동작
- 재연결 이벤트에 대한 콜백 지원

**관련 파일:**
- `include/network_system/core/messaging_client.h`
- `include/network_system/core/secure_messaging_client.h`

---

### 2. HTTP/2 클라이언트 지원

**상태:** 미구현
**우선순위:** P2
**예상 소요:** 10-14일
**목표 버전:** v2.0.0

**설명:**
멀티플렉싱, 서버 푸시, 헤더 압축을 포함한 현대적인 웹 서비스 통신을 위한 HTTP/2 프로토콜 지원을 구현합니다.

**주요 기능:**
- 완전한 HTTP/2 프로토콜 구현 (RFC 7540)
- 단일 연결을 통한 스트림 멀티플렉싱
- 서버 푸시 지원
- HPACK 헤더 압축
- HTTP/1.1에서 연결 업그레이드
- TLS/SSL 통합 (ALPN 협상)

**제안된 API:**
```cpp
class http2_client {
public:
    http2_client(const std::string& host, unsigned short port, bool use_tls = true);

    auto connect() -> VoidResult;
    auto disconnect() -> VoidResult;

    auto get(const std::string& path,
            const http_headers& headers = {}) -> Result<http_response>;

    auto post(const std::string& path,
             const std::vector<uint8_t>& body,
             const http_headers& headers = {}) -> Result<http_response>;

    auto send_request(const http_request& request) -> Result<http_response>;

    // 스트리밍 지원
    auto create_stream() -> Result<std::shared_ptr<http2_stream>>;
};
```

**의존성:**
- nghttp2 라이브러리 (선택 사항, 처음부터 구현 가능)
- TLS/SSL 지원 (이미 구현됨)

**관련 파일:**
- 신규: `include/network_system/protocols/http2_client.h`
- 신규: `src/protocols/http2_client.cpp`

---

### 3. gRPC 통합

**상태:** 미구현
**우선순위:** P2
**예상 소요:** 7-10일
**목표 버전:** v2.0.0

**설명:**
Protocol Buffers를 사용한 고성능 RPC 통신을 위한 gRPC 프로토콜 지원을 추가합니다.

**주요 기능:**
- HTTP/2 전송을 통한 gRPC
- 단일 RPC 호출
- 서버 스트리밍
- 클라이언트 스트리밍
- 양방향 스트리밍
- 데드라인/타임아웃 지원
- 메타데이터 처리

**제안된 API:**
```cpp
class grpc_client {
public:
    grpc_client(const std::string& host, unsigned short port,
               const grpc_channel_config& config = {});

    template<typename Request, typename Response>
    auto call(const std::string& method,
             const Request& request) -> Result<Response>;

    template<typename Request, typename Response>
    auto call_async(const std::string& method,
                   const Request& request,
                   std::function<void(Result<Response>)> callback) -> void;
};
```

**의존성:**
- HTTP/2 지원 (먼저 구현 필요)
- Protocol Buffers 라이브러리 (protobuf)
- gRPC 라이브러리 (선택 사항, HTTP/2 + protobuf를 직접 사용 가능)

**관련 파일:**
- 신규: `include/network_system/protocols/grpc_client.h`
- 신규: `src/protocols/grpc_client.cpp`

---

## 중간 우선순위 기능 (P3)

### ~~4. Zero-Copy 파이프라인~~ ✅ 완료

**상태:** v1.6.0에서 구현됨
**우선순위:** P3
**실제 소요:** 3일
**완료일:** 2025-01-26

**설명:**
송수신 작업 중 불필요한 메모리 복사를 피하도록 데이터 파이프라인을 최적화합니다.

**구현된 기능:**
- 메모리 재사용을 위한 버퍼 풀 (할당 감소)
- 파이프라인 전반에 이동 의미론 적용 (복사 제거)
- 가능한 경우 제자리 변환

**제안된 개선사항:**
```cpp
class zero_copy_pipeline {
public:
    // 복사 대신 버퍼 참조 사용
    auto transform(std::span<const uint8_t> input) -> std::vector<asio::const_buffer>;

    // 벡터화된 I/O 지원
    auto async_send_vectored(
        std::vector<asio::const_buffer> buffers,
        std::function<void(std::error_code, size_t)> handler) -> void;

    // 버퍼 재사용을 위한 메모리 풀
    auto get_buffer(size_t size) -> std::shared_ptr<std::vector<uint8_t>>;
    auto release_buffer(std::shared_ptr<std::vector<uint8_t>> buffer) -> void;
};
```

**이점:**
- 메모리 할당 감소
- 낮은 CPU 사용량
- 대용량 메시지에 대한 높은 처리량
- 더 나은 캐시 지역성

**관련 파일:**
- `include/network_system/internal/pipeline.h`
- `src/internal/pipeline.cpp`
- `include/network_system/internal/tcp_socket.h`
- `include/network_system/internal/secure_tcp_socket.h`

---

### ~~5. 연결 상태 모니터링~~ ✅ 완료

**상태:** v1.5.0에서 구현됨
**우선순위:** P3
**실제 소요:** 3일
**완료일:** 2025-01-26

**설명:**
죽은 연결을 조기에 감지하기 위한 하트비트/킵얼라이브 메커니즘을 사용한 연결 상태 모니터링을 추가합니다.

**주요 기능:**
- 구성 가능한 하트비트 간격
- 자동 죽은 연결 감지
- 연결 품질 메트릭 (지연시간, 패킷 손실)
- 상태 콜백

**제안된 API:**
```cpp
struct connection_health {
    bool is_alive;
    std::chrono::milliseconds last_response_time;
    size_t missed_heartbeats;
    double packet_loss_rate;
};

class health_monitor {
public:
    health_monitor(std::chrono::seconds heartbeat_interval = std::chrono::seconds(30));

    auto start_monitoring(std::shared_ptr<messaging_client> client) -> void;
    auto stop_monitoring() -> void;

    auto get_health() const -> connection_health;
    auto set_health_callback(std::function<void(connection_health)> callback) -> void;
};
```

**이점:**
- 죽은 연결의 조기 감지
- 죽은 연결에 대한 자원 낭비 감소
- 연결 품질에 대한 더 나은 가시성

**관련 파일:**
- 신규: `include/network_system/utils/health_monitor.h`
- 신규: `src/utils/health_monitor.cpp`

---

### ~~6. 메시지 압축 지원~~ ✅ 완료

**상태:** v1.6.0에서 구현됨
**우선순위:** P3
**실제 소요:** 2일
**완료일:** 2025-01-26

**설명:**
파이프라인에서 LZ4 알고리즘을 사용한 실제 압축을 구현합니다.

**구현된 기능:**
- LZ4 고속 압축 알고리즘
- 구성 가능한 압축 임계값 (기본값: 256바이트)
- 작은/비압축 데이터에 대한 자동 폴백
- 크기 검증을 통한 안전한 압축 해제

**제안된 구현:**
```cpp
enum class compression_algorithm {
    none,
    zlib,
    lz4,
    zstd
};

class compression_pipeline {
public:
    compression_pipeline(compression_algorithm algo = compression_algorithm::lz4,
                        int compression_level = 3);

    auto compress(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>;
    auto decompress(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>;

    auto set_compression_threshold(size_t bytes) -> void;  // 작은 메시지는 압축하지 않음
};
```

**의존성:**
- zlib (널리 사용 가능)
- lz4 (선택 사항, 속도용)
- zstd (선택 사항, 더 나은 압축용)

**이점:**
- 대역폭 사용량 감소
- 낮은 네트워크 비용
- 압축 가능한 데이터에 대한 빠른 전송

**관련 파일:**
- `include/network_system/internal/pipeline.h`
- `src/internal/pipeline.cpp`

---

### ~~7. UDP 신뢰성 계층~~ ✅ 완료

**상태:** v1.7.0에서 구현됨
**우선순위:** P3
**실제 소요:** 3일
**완료일:** 2025-01-27

**설명:**
속도와 신뢰성 모두를 필요로 하는 애플리케이션을 위해 UDP 위에 선택적 신뢰성 계층을 추가합니다.

**주요 기능:**
- 선택적 확인응답 (SACK)
- 패킷 재전송
- 순서 보장 배달 옵션
- 혼잡 제어
- 구성 가능한 신뢰성 수준

**제안된 API:**
```cpp
enum class reliability_mode {
    unreliable,          // 순수 UDP
    reliable_ordered,    // TCP와 유사하지만 UDP를 통해
    reliable_unordered,  // 순서 없는 신뢰성
    sequenced           // 오래된 패킷 버림, 재전송 없음
};

class reliable_udp_client {
public:
    reliable_udp_client(const std::string& client_id,
                       reliability_mode mode = reliability_mode::reliable_ordered);

    auto start_client(const std::string& host, unsigned short port) -> VoidResult;
    auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

    auto set_congestion_window(size_t packets) -> void;
    auto set_max_retries(size_t retries) -> void;
};
```

**이점:**
- 양쪽의 장점 (UDP 속도 + TCP 신뢰성)
- 구성 가능한 트레이드오프
- TCP보다 실시간 애플리케이션에 더 적합

**관련 파일:**
- 신규: `include/network_system/core/reliable_udp_client.h`
- 신규: `src/core/reliable_udp_client.cpp`

---

## 낮은 우선순위 / 미래 기능 (P4)

### 8. QUIC 프로토콜 지원

**상태:** 계획되지 않음
**우선순위:** P4
**예상 소요:** 15-20일
**목표 버전:** v2.1.0+

**설명:**
현대적인 저지연 네트워킹을 위한 QUIC (Quick UDP Internet Connections) 프로토콜 지원을 추가합니다.

**주요 기능:**
- UDP 기반
- 0-RTT 연결 수립
- 멀티플렉싱 스트림
- 내장 암호화 (TLS 1.3)
- 연결 마이그레이션

**의존성:**
- 상당한 엔지니어링 노력 필요
- 복잡한 프로토콜 구현
- 타사 라이브러리 필요 가능 (예: quiche, ngtcp2)

---

### ~~9. UDP용 DTLS 지원~~ ✅ 완료

**상태:** v1.8.0에서 구현됨
**우선순위:** P4
**실제 소요:** 1일
**완료일:** 2025-11-26

**설명:**
안전한 UDP 통신을 위한 DTLS (Datagram TLS) 지원을 추가합니다.

**구현된 기능:**
- `dtls_socket` - OpenSSL을 사용한 저수준 DTLS 소켓 래퍼
- `secure_messaging_udp_client` - 안전한 UDP 통신을 위한 DTLS 클라이언트
- `secure_messaging_udp_server` - 세션 관리 기능이 있는 DTLS 서버
- ASIO 통합을 통한 전체 비동기 I/O 지원
- 스레드 안전 구현

**이점:**
- 암호화된 UDP 통신
- 실시간 애플리케이션을 위한 보안
- 기존 TLS 인프라와 호환

**관련 파일:**
- `include/kcenon/network/internal/dtls_socket.h`
- `src/internal/dtls_socket.cpp`
- `include/kcenon/network/core/secure_messaging_udp_client.h`
- `src/core/secure_messaging_udp_client.cpp`
- `include/kcenon/network/core/secure_messaging_udp_server.h`
- `src/core/secure_messaging_udp_server.cpp`

---

### 10. 메트릭 및 모니터링 대시보드

**상태:** 계획되지 않음
**우선순위:** P4
**예상 소요:** 10-14일
**목표 버전:** v2.0.0

**설명:**
실시간으로 network system 메트릭을 모니터링하기 위한 내장 웹 대시보드.

**주요 기능:**
- 실시간 연결 통계
- 처리량 그래프
- 오류율 모니터링
- 라이브 업데이트를 위한 WebSocket
- 메트릭 내보내기를 위한 REST API

---

## 요약

### 우선순위별

| 우선순위 | 개수 | 완료 | 남은 | 총 소요 시간 |
|----------|------|------|------|-------------|
| P2       | 3    | 1    | 2    | 19-27일     |
| P3       | 5    | 5    | 0    | 0일         |
| P4       | 3    | 1    | 2    | 25-34일     |
| **총계**  | **11** | **7** | **4** | **44-61일** |

### 목표 버전별

| 버전    | 기능                                    | 상태 | 소요 시간   |
|---------|----------------------------------------|------|-----------|
| v1.5.0  | 재연결 로직, 상태 모니터링              | ✅ 완료 | 5일      |
| v1.6.0  | Zero-Copy 파이프라인, 압축              | ✅ 완료 | 5일     |
| v1.7.0  | UDP 신뢰성 계층                         | ✅ 완료 | 3일     |
| v1.8.0  | DTLS 지원                               | ✅ 완료 | 1일     |
| v2.0.0  | HTTP/2, gRPC, 메트릭 대시보드           | 대기 중 | 27-38일    |
| v2.1.0+ | QUIC 프로토콜                           | 대기 중 | 15-20일    |

### 구현 로드맵

**Phase 1 (v1.5.0):** 안정성 및 신뢰성 ✅ 완료
- ✅ 재연결 로직
- ✅ 상태 모니터링

**Phase 2 (v1.6.0):** 성능 최적화 ✅ 완료
- ✅ Zero-copy 파이프라인 (버퍼 풀 + 이동 의미론)
- ✅ 메시지 압축 (LZ4)

**Phase 3 (v1.7.0):** 프로토콜 향상 ✅ 완료
- ✅ UDP 신뢰성 계층

**Phase 3.5 (v1.8.0):** 보안 강화 ✅ 완료
- ✅ DTLS 지원 (안전한 UDP 통신)

**Phase 4 (v2.0.0):** 현대적인 프로토콜
- HTTP/2 지원
- gRPC 통합
- 모니터링 대시보드

**Phase 5 (v2.1.0+):** 고급 기능
- QUIC 프로토콜
- 필요에 따른 추가 프로토콜

---

## 기여하기

이러한 기능 중 하나를 구현하고 싶으시다면:

1. 완료된 기능에 대해서는 [IMPROVEMENTS.md](IMPROVEMENTS.md)를 확인하세요
2. 구현 접근 방식을 논의하기 위한 이슈를 생성하세요
3. 기존 코드 스타일과 패턴을 따르세요
4. 포괄적인 테스트를 추가하세요
5. 문서를 업데이트하세요

## 참고 사항

- 모든 소요 시간 추정치는 숙련된 개발자 1명을 기준으로 합니다
- 추정치에는 구현, 테스트 및 문서화가 포함됩니다
- 의존성이 구현 순서에 영향을 줄 수 있습니다
- 사용자 피드백에 따라 기능 우선순위가 변경될 수 있습니다
