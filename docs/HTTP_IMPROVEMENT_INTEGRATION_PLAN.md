# HTTP 개선 사항 통합 계획서

**문서 버전**: 1.0
**작성일**: 2025-11-13
**브랜치**: `feature/http-improvement-integration`
**기준 브랜치**: `main`
**참조 브랜치**: `feature/http-improvement`

---

## 목차

1. [개요](#개요)
2. [분석 요약](#분석-요약)
3. [개선 방향성](#개선-방향성)
4. [단계별 통합 계획](#단계별-통합-계획)
5. [시스템 통합 전략](#시스템-통합-전략)
6. [검증 및 테스트 계획](#검증-및-테스트-계획)
7. [위험 관리](#위험-관리)
8. [일정 및 마일스톤](#일정-및-마일스톤)

---

## 개요

### 목적

`feature/http-improvement` 브랜치의 개선 사항을 `main` 브랜치에 안전하게 통합하여 network_system의 HTTP 기능을 production-ready 수준으로 향상시킵니다.

### 범위

- **버그 수정**: 메모리 누수, 데드락, 스레드 안전성 이슈 해결
- **기능 추가**: Cookie 파싱, Multipart/form-data, Chunked encoding, 자동 압축
- **시스템 통합**: common_system, thread_system, logger_system, monitoring_system 활용 강화
- **테스트 커버리지**: 단위 테스트, 통합 테스트, 성능 테스트 확대

### 주요 이해관계자

- **개발팀**: HTTP 기능 구현 및 유지보수
- **품질팀**: 테스트 및 검증
- **운영팀**: 배포 및 모니터링

---

## 분석 요약

### 현재 상태 (main 브랜치)

#### 강점
- ✅ 모듈화된 아키텍처: 명확한 계층 분리 (core, internal, integration, utils)
- ✅ 기존 시스템 통합: logger, thread, monitoring, common_system 인터페이스 구현
- ✅ 프로토콜 지원: TCP, UDP, WebSocket, HTTP/1.1, TLS/SSL
- ✅ 포괄적인 테스트: 단위, 통합, 스트레스, 벤치마크 테스트
- ✅ C++20 활용: Concepts, Coroutines, Result<T> 패턴

#### 제약사항
- ❌ HTTP 요청 버퍼링 미흡: 청크별 수신 시 불완전한 요청 처리
- ❌ 제한적인 HTTP 기능: Cookie, multipart, chunked encoding, compression 미지원
- ❌ 메모리 누수: messaging_session에서 순환 참조 발생 (~900 bytes/connection)
- ❌ 데드락 위험: messaging_server의 lock-order-inversion
- ❌ 스레드 안전성: http_url::parse()의 데이터 레이스

### 개선 대상 (feature/http-improvement 브랜치)

#### 변경 통계
- **36개 파일 변경**
- **2,972줄 추가**
- **830줄 삭제**
- **18개 커밋**

#### 주요 개선 사항

**1. 버그 수정 (7개)**

| 커밋 | 이슈 | 영향 | 파일 |
|------|------|------|------|
| c2d3031 | 메모리 누수 (순환 참조) | High | `src/session/messaging_session.cpp` |
| 68f9d29 | Lock-order-inversion 데드락 | High | `src/core/messaging_server.cpp` |
| deb86e4 | URL 파싱 데이터 레이스 | Medium | `src/core/http_client.cpp` |
| d48b6db | TCP 우아한 종료 복원 | Medium | `src/internal/tcp_socket.cpp` |
| 369a6ae | Use-after-move 버그 | Low | `tests/integration/...` |
| 3cd60a8 | HTTP 테스트 타임아웃 | Low | `src/core/messaging_client.cpp` |
| adc056a | 세션 콜백 순환 참조 | High | `src/core/messaging_server.cpp` |

**2. 기능 추가 (8개)**

| 기능 | 우선순위 | 복잡도 | 파일 |
|------|----------|--------|------|
| HTTP 요청 버퍼링 | Critical | Medium | `src/core/http_server.cpp` |
| Cookie 파싱 | High | Low | `src/internal/http_parser.cpp`, `include/.../http_types.h` |
| Multipart/form-data | High | High | `src/internal/http_parser.cpp` |
| Chunked encoding | Medium | Medium | `src/core/http_server.cpp` |
| 자동 압축 (gzip/deflate) | Medium | Medium | `src/utils/compression_pipeline.cpp` |
| 동기 응답 전송 | High | Low | `src/core/http_server.cpp` |

**3. 테스트 추가**

- `tests/unit/http_parser_test.cpp`: HTTP 파싱 단위 테스트 (496줄)
- `tests/integration/http_server_client_test.cpp`: HTTP 통합 테스트 (411줄)
- `samples/chunked_encoding_demo.cpp`: Chunked/압축 데모 (143줄)
- `samples/cookie_multipart_demo.cpp`: Cookie/파일 업로드 데모 (240줄)

**4. 의존성 변경**

- **추가**: `zlib` (gzip/deflate 압축용, `vcpkg.json`)
- **빌드 시스템**: `CMakeLists.txt`에 ZLIB 링크 추가

---

## 개선 방향성

### 핵심 원칙

1. **안정성 우선**: 버그 수정을 기능 추가보다 먼저 통합
2. **점진적 통합**: 단계별로 검증하며 통합
3. **시스템 통합 강화**: 기존 logger, thread, monitoring, common_system 활용 극대화
4. **테스트 기반 개발**: 모든 변경 사항은 테스트로 검증
5. **후방 호환성**: 기존 API 유지 또는 deprecation 경로 제공

### 목표 아키텍처

```
network_system (main 구조 유지)
├── core/                       # Public API
│   ├── http_server.h/cpp      # ✨ 요청 버퍼링, 압축, 청킹 추가
│   ├── http_client.h/cpp      # ✨ 스레드 안전성 개선
│   ├── messaging_server.cpp   # 🔧 데드락 수정
│   └── messaging_client.cpp   # 🔧 타임아웃 수정
├── internal/                   # 프로토콜 구현
│   ├── http_parser.h/cpp      # ✨ Cookie, Multipart 파싱 추가
│   ├── http_types.h/cpp       # ✨ Cookie, Multipart 타입 추가
│   └── tcp_socket.cpp         # 🔧 우아한 종료 복원
├── session/
│   └── messaging_session.cpp  # 🔧 메모리 누수 수정
├── integration/                # 🎯 시스템 통합 강화
│   ├── logger_integration.h   # HTTP 메트릭 로깅
│   ├── thread_integration.h   # 비동기 작업 처리
│   ├── monitoring_integration.h  # HTTP 성능 메트릭
│   └── common_system_adapter.h   # Result 타입 활용
└── utils/
    └── compression_pipeline.cpp  # ✨ gzip/deflate 구현
```

### 시스템 통합 전략

#### 1. logger_system 활용

- HTTP 요청/응답 로깅 (요청 라인, 상태 코드, 크기)
- 압축/청킹 작업 로깅
- 에러 상황 상세 로깅 (파싱 실패, 버퍼 오버플로)
- 성능 경고 (느린 응답, 대용량 업로드)

**구현 위치**: `src/core/http_server.cpp`, `src/internal/http_parser.cpp`

```cpp
// 예시
NETWORK_LOG_INFO("HTTP " + method + " " + path + " - " +
                 std::to_string(status_code) + " (" +
                 std::to_string(response_size) + " bytes)");

NETWORK_LOG_DEBUG("Applied gzip compression: " +
                  std::to_string(original_size) + " -> " +
                  std::to_string(compressed_size) + " bytes (" +
                  std::to_string(ratio) + "% reduction)");
```

#### 2. thread_system 활용

- HTTP 요청 처리 작업을 스레드 풀로 분산
- 파일 업로드 처리 비동기화
- 압축 작업 백그라운드 처리

**구현 위치**: `src/core/http_server.cpp`

```cpp
// 예시
auto& thread_mgr = integration::thread_integration_manager::instance();
thread_mgr.submit_task([this, request, response]() {
    // 대용량 multipart 파싱
    http_parser::parse_multipart_form_data(request);
});
```

#### 3. monitoring_system 활용

**추적 메트릭**:

| 메트릭 | 타입 | 설명 |
|--------|------|------|
| `http_requests_total` | Counter | 총 요청 수 (라벨: method, path, status) |
| `http_request_duration_seconds` | Histogram | 요청 처리 시간 |
| `http_request_size_bytes` | Histogram | 요청 크기 분포 |
| `http_response_size_bytes` | Histogram | 응답 크기 분포 |
| `http_compression_ratio` | Gauge | 압축률 (%) |
| `http_chunked_responses_total` | Counter | 청킹된 응답 수 |
| `http_parse_errors_total` | Counter | 파싱 에러 수 (라벨: error_type) |
| `http_multipart_uploads_total` | Counter | 파일 업로드 수 |

**구현 위치**: `src/core/http_server.cpp`, `src/internal/http_parser.cpp`

```cpp
// 예시
auto& monitoring = integration::monitoring_integration_manager::instance();
monitoring.report_counter("http_requests_total", 1, {
    {"method", request.method},
    {"status", std::to_string(status_code)}
});
monitoring.report_histogram("http_request_duration_seconds",
                            duration.count());
```

#### 4. common_system 활용

- `Result<T>` 타입으로 에러 전파
- 통일된 에러 코드 체계 사용
- 에러 체인 구축

**구현 위치**: `src/internal/http_parser.cpp`, `src/core/http_server.cpp`

```cpp
// 예시
Result<http_request> parse_http_request(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return Result<http_request>::error(
            error_codes::common_errors::invalid_argument,
            "Empty request data"
        );
    }
    // 파싱 로직...
    return Result<http_request>::ok(std::move(request));
}
```

---

## 단계별 통합 계획

### Phase 0: 준비 단계 (사전 작업)

**목표**: 통합을 위한 기반 구축

**작업 항목**:

1. **의존성 설치 및 검증**
   - [ ] zlib 설치 확인 (`vcpkg install zlib`)
   - [ ] CMakeLists.txt에 ZLIB 의존성 추가
   - [ ] 빌드 시스템 검증 (clean build)

2. **테스트 환경 구성**
   - [ ] 테스트 데이터셋 준비 (Cookie, multipart 샘플)
   - [ ] Sanitizer 빌드 환경 설정 (AddressSanitizer, ThreadSanitizer)
   - [ ] 성능 베이스라인 측정 (기존 HTTP 서버 벤치마크)

3. **브랜치 전략**
   - [x] `feature/http-improvement-integration` 브랜치 생성
   - [ ] CI/CD 파이프라인 설정

**예상 기간**: 1일

**산출물**:
- 빌드 성공 확인
- 베이스라인 성능 보고서
- CI/CD 설정 완료

---

### Phase 1: 중요 버그 수정 (안정성 확보)

**목표**: 프로덕션 블로커 버그 제거

**우선순위**: Critical 🔴

#### Step 1.1: 메모리 누수 수정

**파일**: `src/session/messaging_session.cpp`

**변경 내역**:
- 콜백 람다에서 `shared_ptr` 대신 `weak_ptr` 사용
- 순환 참조 체인 차단

**구현 방법**:

```cpp
// Before (순환 참조)
socket_->set_receive_callback([self = shared_from_this()](auto data, auto size) {
    self->process_data(data, size);
});

// After (weak_ptr 사용)
socket_->set_receive_callback([weak_self = weak_from_this()](auto data, auto size) {
    if (auto self = weak_self.lock()) {
        self->process_data(data, size);
    }
});
```

**검증 방법**:
- AddressSanitizer 빌드로 누수 확인
- Valgrind/Leak Sanitizer 실행
- 1000개 연결 생성/종료 후 메모리 사용량 측정

**예상 영향**: 연결당 ~900 bytes 누수 제거

**기간**: 2일

**작업 항목**:
- [ ] `messaging_session.cpp` 수정
- [ ] 모든 콜백에 `weak_ptr` 패턴 적용
- [ ] AddressSanitizer 빌드 실행
- [ ] 메모리 프로파일링 (before/after)
- [ ] 단위 테스트 작성 (세션 생명주기)

**logger_system 통합**:
```cpp
NETWORK_LOG_DEBUG("Session callback invoked, strong refs: " +
                  std::to_string(self.use_count()));
```

**monitoring_system 통합**:
```cpp
monitoring.report_gauge("session_active_count", active_sessions_);
monitoring.report_gauge("session_memory_bytes", total_session_memory);
```

#### Step 1.2: Lock-order-inversion 데드락 수정

**파일**: `src/core/messaging_server.cpp`

**변경 내역**:
- 콜백을 로컬 변수로 복사 후 mutex 해제
- 락 없이 `set_*_callback()` 호출

**구현 방법**:

```cpp
// Before (데드락 위험)
{
    std::lock_guard lock(callback_mutex_);
    session->set_receive_callback(receive_callback_);  // M0 → M1
}

// After (락 순서 보장)
std::function<void(std::vector<uint8_t>, size_t)> local_callback;
{
    std::lock_guard lock(callback_mutex_);
    local_callback = receive_callback_;
}
// 락 해제 후 호출
session->set_receive_callback(local_callback);  // 락 없음
```

**검증 방법**:
- ThreadSanitizer 빌드로 데드락 감지
- 동시 연결 스트레스 테스트 (1000+ connections)
- Lock contention 프로파일링

**기간**: 2일

**작업 항목**:
- [ ] `messaging_server.cpp`의 모든 콜백 설정 수정
- [ ] ThreadSanitizer 빌드 실행
- [ ] 동시성 스트레스 테스트
- [ ] Lock contention 측정 (before/after)

**logger_system 통합**:
```cpp
NETWORK_LOG_TRACE("Setting callback without holding callback_mutex");
```

#### Step 1.3: HTTP URL 파싱 스레드 안전성 수정

**파일**: `src/core/http_client.cpp`

**변경 내역**:
- `url_regex`를 `static const`로 변경
- C++11 magic statics 활용

**구현 방법**:

```cpp
// Before (매번 재컴파일, 데이터 레이스)
Result<http_url> http_url::parse(const std::string& url_str) {
    std::regex url_regex(R"(...)");  // 스레드 안전하지 않음
    // ...
}

// After (한 번만 컴파일, 스레드 안전)
Result<http_url> http_url::parse(const std::string& url_str) {
    static const std::regex url_regex(R"(...)");  // magic statics
    // ...
}
```

**검증 방법**:
- ThreadSanitizer로 데이터 레이스 확인
- 멀티스레드 URL 파싱 테스트 (1000 threads × 1000 URLs)
- 성능 비교 (정규식 컴파일 오버헤드 제거)

**기간**: 1일

**작업 항목**:
- [ ] `http_client.cpp` 수정
- [ ] ThreadSanitizer 실행
- [ ] 멀티스레드 단위 테스트
- [ ] 성능 벤치마크

#### Step 1.4: TCP 우아한 종료 복원

**파일**: `src/internal/tcp_socket.cpp`, `src/session/messaging_session.cpp`

**변경 내역**:
- EOF/operation-aborted 이벤트를 에러 핸들러로 전파
- 피어 종료 시 우아한 종료 처리

**구현 방법**:

```cpp
// tcp_socket.cpp
void handle_receive_completion(const asio::error_code& ec, size_t bytes) {
    if (ec == asio::error::eof || ec == asio::error::operation_aborted) {
        // 에러 핸들러로 전파 (우아한 종료)
        if (error_handler_) {
            error_handler_(ec);
        }
        return;
    }
    // 실제 에러 처리...
}

// messaging_session.cpp
void on_error(const asio::error_code& ec) {
    if (ec == asio::error::eof) {
        NETWORK_LOG_INFO("Peer closed connection gracefully");
        // 정상 종료 처리
    } else {
        NETWORK_LOG_ERROR("Connection error: " + ec.message());
    }
}
```

**검증 방법**:
- 클라이언트 강제 종료 시나리오 테스트
- 정상 종료 vs 비정상 종료 구분 확인
- 로그 메시지 레벨 검증

**기간**: 2일

**작업 항목**:
- [ ] `tcp_socket.cpp` 수정
- [ ] `messaging_session.cpp` 에러 핸들러 개선
- [ ] 종료 시나리오 테스트 (정상/비정상)
- [ ] 로그 메시지 검증

**Phase 1 완료 기준**:
- ✅ AddressSanitizer clean (메모리 누수 0)
- ✅ ThreadSanitizer clean (데이터 레이스 0, 데드락 0)
- ✅ 모든 기존 테스트 통과
- ✅ 스트레스 테스트 통과 (1000+ connections)

**Phase 1 총 기간**: 7일

---

### Phase 2: HTTP 인프라 개선 (기반 구축)

**목표**: HTTP 기능 추가를 위한 기반 마련

**우선순위**: High 🟠

#### Step 2.1: zlib 의존성 통합

**파일**: `vcpkg.json`, `CMakeLists.txt`, `cmake/NetworkSystemDependencies.cmake`

**변경 내역**:

**vcpkg.json**:
```json
{
  "dependencies": [
    "asio",
    "fmt",
    "gtest",
    "benchmark",
    "zlib"  // 추가
  ]
}
```

**CMakeLists.txt**:
```cmake
# ZLIB 찾기
find_package(ZLIB REQUIRED)

# 라이브러리 링크
target_link_libraries(NetworkSystem
    PRIVATE
    asio::asio
    fmt::fmt
    ZLIB::ZLIB  # 추가
)
```

**검증 방법**:
- Windows, Linux, macOS 빌드 확인
- vcpkg 캐시 동작 확인
- 정적/동적 링크 테스트

**기간**: 1일

**작업 항목**:
- [ ] vcpkg.json 업데이트
- [ ] CMakeLists.txt 수정
- [ ] 멀티 플랫폼 빌드 검증
- [ ] CI/CD 파이프라인 업데이트

#### Step 2.2: HTTP 요청 버퍼링 메커니즘 구현

**파일**: `src/core/http_server.cpp`

**변경 내역**:
- 세션별 요청 버퍼 추가
- 헤더 끝 감지 (`\r\n\r\n`)
- Content-Length 기반 완전성 확인
- 보안 제한 (최대 크기)

**구현 방법**:

```cpp
struct http_request_buffer {
    std::vector<uint8_t> data;
    bool headers_complete = false;
    std::size_t headers_end_pos = 0;
    std::size_t content_length = 0;

    static constexpr std::size_t MAX_REQUEST_SIZE = 10 * 1024 * 1024;  // 10MB
    static constexpr std::size_t MAX_HEADER_SIZE = 64 * 1024;          // 64KB

    bool append(const std::vector<uint8_t>& chunk) {
        // 크기 제한 확인
        if (data.size() + chunk.size() > MAX_REQUEST_SIZE) {
            return false;  // 413 Payload Too Large
        }

        data.insert(data.end(), chunk.begin(), chunk.end());

        // 헤더 끝 찾기
        if (!headers_complete) {
            auto marker = find_header_end(data);
            if (marker != std::string::npos) {
                headers_complete = true;
                headers_end_pos = marker + 4;  // "\r\n\r\n" 길이
                content_length = parse_content_length(data, headers_end_pos);
            } else if (data.size() > MAX_HEADER_SIZE) {
                return false;  // 431 Request Header Fields Too Large
            }
        }

        // 완전한 요청인지 확인
        if (headers_complete) {
            return data.size() >= (headers_end_pos + content_length);
        }
        return false;
    }

    bool is_complete() const {
        return headers_complete &&
               data.size() >= (headers_end_pos + content_length);
    }
};
```

**세션 통합**:

```cpp
// http_server.cpp
void on_request_chunk(const std::vector<uint8_t>& chunk, session_id_t sid) {
    auto& buffer = session_buffers_[sid];

    if (!buffer.append(chunk)) {
        // 크기 초과
        send_error_response(sid, 413, "Payload Too Large");
        NETWORK_LOG_WARN("Request too large from session " +
                        std::to_string(sid));
        return;
    }

    if (buffer.is_complete()) {
        // 요청 처리
        auto request = http_parser::parse_request(buffer.data);
        if (request.is_error()) {
            send_error_response(sid, 400, "Bad Request");
            NETWORK_LOG_ERROR("Parse error: " + request.error_message());
            return;
        }

        handle_http_request(request.value(), sid);
        session_buffers_.erase(sid);  // 버퍼 정리
    }
}
```

**검증 방법**:
- 작은 요청 (< 1KB)
- 큰 요청 (1MB ~ 10MB)
- 청크별 수신 (100 bytes, 1KB, 10KB chunks)
- 크기 초과 요청 (> 10MB)
- 헤더 크기 초과 (> 64KB)

**기간**: 3일

**작업 항목**:
- [ ] `http_request_buffer` 구조체 구현
- [ ] 세션별 버퍼 관리
- [ ] 에러 응답 처리 (413, 431)
- [ ] 단위 테스트 (버퍼링 로직)
- [ ] 통합 테스트 (다양한 크기)

**logger_system 통합**:
```cpp
NETWORK_LOG_DEBUG("Received chunk: " + std::to_string(chunk.size()) +
                  " bytes, buffer: " + std::to_string(buffer.data.size()) +
                  "/" + std::to_string(buffer.content_length) + " bytes");
```

**monitoring_system 통합**:
```cpp
monitoring.report_histogram("http_request_buffer_size_bytes", buffer.data.size());
monitoring.report_counter("http_request_buffer_overflow_total", 1);
```

#### Step 2.3: 동기 응답 전송 구현

**파일**: `src/core/http_server.cpp`

**변경 내역**:
- `send_packet()` → `send_packet_sync()` 변경
- 응답 완료 대기
- Connection: close 지원

**구현 방법**:

```cpp
void send_http_response(const http_response& response, session_id_t sid) {
    auto response_data = serialize_response(response);

    // 동기 전송 (완료 대기)
    auto session = get_session(sid);
    if (session) {
        session->send_packet_sync(response_data);
        NETWORK_LOG_INFO("Sent HTTP " + std::to_string(response.status_code) +
                        " (" + std::to_string(response_data.size()) + " bytes)");
    }

    // Connection: close 처리
    if (should_close_connection(response)) {
        session->close();
        NETWORK_LOG_DEBUG("Closed connection after response");
    }
}
```

**검증 방법**:
- 응답 순서 확인
- Connection: keep-alive vs close
- 타임아웃 처리

**기간**: 2일

**작업 항목**:
- [ ] `send_packet_sync()` 활용
- [ ] Connection 헤더 처리
- [ ] 타임아웃 설정
- [ ] 통합 테스트

**Phase 2 완료 기준**:
- ✅ zlib 빌드 성공 (모든 플랫폼)
- ✅ 요청 버퍼링 테스트 통과
- ✅ 동기 응답 전송 검증
- ✅ 기존 HTTP 테스트 통과

**Phase 2 총 기간**: 6일

---

### Phase 3: HTTP 파싱 기능 확장

**목표**: Cookie 및 Multipart/form-data 파싱 구현

**우선순위**: High 🟠

#### Step 3.1: HTTP 타입 확장

**파일**: `include/network_system/internal/http_types.h`, `src/internal/http_types.cpp`

**변경 내역**:

**Cookie 구조체 추가**:
```cpp
// http_types.h
struct cookie {
    std::string name;
    std::string value;
    std::string path;
    std::string domain;
    std::string expires;
    int max_age = -1;  // -1 = session cookie
    bool secure = false;
    bool http_only = false;
    std::string same_site;  // "Strict", "Lax", "None"
};

// http_request에 추가
struct http_request {
    // ... 기존 필드 ...
    std::map<std::string, std::string> cookies;  // 파싱된 쿠키
};

// http_response에 메서드 추가
struct http_response {
    // ... 기존 필드 ...
    std::vector<cookie> set_cookies;  // 설정할 쿠키

    void set_cookie(const std::string& name, const std::string& value,
                   const std::string& path = "/", int max_age = -1,
                   bool http_only = true, bool secure = false);
};
```

**Multipart 구조체 추가**:
```cpp
// http_types.h
struct multipart_file {
    std::string field_name;
    std::string filename;
    std::string content_type;
    std::vector<uint8_t> content;
};

// http_request에 추가
struct http_request {
    // ... 기존 필드 ...
    std::map<std::string, std::string> form_data;  // 폼 필드
    std::map<std::string, multipart_file> files;  // 업로드된 파일
};
```

**기간**: 1일

**작업 항목**:
- [ ] `http_types.h` 업데이트
- [ ] `http_types.cpp` 구현 (`set_cookie()`)
- [ ] 직렬화 메서드 추가
- [ ] 단위 테스트

#### Step 3.2: Cookie 파싱 구현

**파일**: `src/internal/http_parser.cpp`

**변경 내역**:

```cpp
// http_parser.cpp
void http_parser::parse_cookies(http_request& request) {
    auto cookie_header = request.get_header("Cookie");
    if (!cookie_header) {
        return;
    }

    // "name1=value1; name2=value2; ..." 형식 파싱
    std::string cookie_str = *cookie_header;
    size_t pos = 0;

    while (pos < cookie_str.size()) {
        // 다음 세미콜론 찾기
        auto semi_pos = cookie_str.find(';', pos);
        if (semi_pos == std::string::npos) {
            semi_pos = cookie_str.size();
        }

        std::string pair = cookie_str.substr(pos, semi_pos - pos);
        trim(pair);  // 공백 제거

        // "name=value" 분리
        auto eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            trim(name);
            trim(value);
            request.cookies[name] = value;
        }

        pos = semi_pos + 1;
    }

    NETWORK_LOG_DEBUG("Parsed " + std::to_string(request.cookies.size()) +
                     " cookies from request");
}
```

**Set-Cookie 직렬화**:
```cpp
// http_types.cpp
void http_response::set_cookie(const std::string& name, const std::string& value,
                               const std::string& path, int max_age,
                               bool http_only, bool secure) {
    cookie c;
    c.name = name;
    c.value = value;
    c.path = path;
    c.max_age = max_age;
    c.http_only = http_only;
    c.secure = secure;
    set_cookies.push_back(c);
}

std::string cookie::to_header_value() const {
    std::ostringstream oss;
    oss << name << "=" << value;

    if (!path.empty()) {
        oss << "; Path=" << path;
    }
    if (max_age >= 0) {
        oss << "; Max-Age=" << max_age;
    }
    if (http_only) {
        oss << "; HttpOnly";
    }
    if (secure) {
        oss << "; Secure";
    }
    if (!same_site.empty()) {
        oss << "; SameSite=" << same_site;
    }

    return oss.str();
}
```

**검증 방법**:
- 단일 쿠키 파싱
- 다중 쿠키 파싱
- 특수 문자 처리 (URL 인코딩)
- Set-Cookie 직렬화

**기간**: 2일

**작업 항목**:
- [ ] `parse_cookies()` 구현
- [ ] `cookie::to_header_value()` 구현
- [ ] 단위 테스트 (다양한 쿠키 형식)
- [ ] 통합 테스트 (쿠키 왕복)

**logger_system 통합**:
```cpp
NETWORK_LOG_TRACE("Cookie: " + name + "=" + value);
```

**monitoring_system 통합**:
```cpp
monitoring.report_counter("http_cookies_parsed_total", request.cookies.size());
```

#### Step 3.3: Multipart/form-data 파싱 구현

**파일**: `src/internal/http_parser.cpp`

**변경 내역**:

```cpp
// http_parser.cpp
Result<void> http_parser::parse_multipart_form_data(http_request& request) {
    // Content-Type 확인
    auto content_type = request.get_header("Content-Type");
    if (!content_type || content_type->find("multipart/form-data") == std::string::npos) {
        return Result<void>::error(error_codes::common_errors::invalid_argument,
                                  "Not a multipart request");
    }

    // Boundary 추출
    auto boundary_pos = content_type->find("boundary=");
    if (boundary_pos == std::string::npos) {
        return Result<void>::error(error_codes::common_errors::invalid_argument,
                                  "Missing boundary");
    }
    std::string boundary = "--" + content_type->substr(boundary_pos + 9);
    trim(boundary);

    NETWORK_LOG_DEBUG("Parsing multipart with boundary: " + boundary);

    // Body를 boundary로 분할
    const auto& body = request.body;
    size_t pos = 0;

    while (pos < body.size()) {
        // 다음 boundary 찾기
        auto boundary_start = find_sequence(body, pos, boundary);
        if (boundary_start == std::string::npos) {
            break;
        }

        // Part 헤더 파싱
        size_t part_start = boundary_start + boundary.size() + 2;  // "\r\n"
        auto headers_end = find_sequence(body, part_start, "\r\n\r\n");
        if (headers_end == std::string::npos) {
            break;
        }

        // Content-Disposition 파싱
        std::string headers(body.begin() + part_start,
                          body.begin() + headers_end);
        auto disposition = parse_header_value(headers, "Content-Disposition");

        if (disposition.find("form-data") != std::string::npos) {
            auto field_name = extract_quoted_value(disposition, "name");
            auto filename = extract_quoted_value(disposition, "filename");

            // Content 추출
            size_t content_start = headers_end + 4;
            auto next_boundary = find_sequence(body, content_start, "\r\n" + boundary);

            std::vector<uint8_t> content(body.begin() + content_start,
                                        body.begin() + next_boundary);

            if (!filename.empty()) {
                // 파일 업로드
                multipart_file file;
                file.field_name = field_name;
                file.filename = filename;
                file.content_type = parse_header_value(headers, "Content-Type");
                file.content = std::move(content);

                request.files[field_name] = std::move(file);

                NETWORK_LOG_INFO("Parsed file: " + filename + " (" +
                               std::to_string(file.content.size()) + " bytes)");
            } else {
                // 일반 폼 필드
                std::string value(content.begin(), content.end());
                request.form_data[field_name] = value;

                NETWORK_LOG_TRACE("Form field: " + field_name + "=" + value);
            }
        }

        pos = next_boundary + boundary.size();
    }

    NETWORK_LOG_INFO("Parsed multipart: " +
                    std::to_string(request.form_data.size()) + " fields, " +
                    std::to_string(request.files.size()) + " files");

    return Result<void>::ok();
}
```

**헬퍼 함수**:
```cpp
std::string extract_quoted_value(const std::string& str, const std::string& key) {
    auto key_pos = str.find(key + "=\"");
    if (key_pos == std::string::npos) {
        return "";
    }
    size_t start = key_pos + key.size() + 2;  // key + ="
    auto end = str.find("\"", start);
    return str.substr(start, end - start);
}

size_t find_sequence(const std::vector<uint8_t>& data, size_t start,
                     const std::string& seq) {
    for (size_t i = start; i <= data.size() - seq.size(); ++i) {
        if (std::equal(seq.begin(), seq.end(), data.begin() + i)) {
            return i;
        }
    }
    return std::string::npos;
}
```

**검증 방법**:
- 단일 필드 multipart
- 파일 업로드 (텍스트, 바이너리)
- 다중 파일 업로드
- 큰 파일 (10MB+)
- Content-Type 감지

**기간**: 4일

**작업 항목**:
- [ ] `parse_multipart_form_data()` 구현
- [ ] 헬퍼 함수 구현
- [ ] 단위 테스트 (다양한 multipart 형식)
- [ ] 통합 테스트 (파일 업로드)
- [ ] 대용량 파일 테스트

**logger_system 통합**:
```cpp
NETWORK_LOG_INFO("Uploaded file: " + filename + ", size: " +
                std::to_string(file.content.size()) + " bytes, type: " +
                file.content_type);
```

**monitoring_system 통합**:
```cpp
monitoring.report_counter("http_multipart_files_total", request.files.size());
monitoring.report_histogram("http_multipart_file_size_bytes", file.content.size());
```

**thread_system 통합** (대용량 파일 처리):
```cpp
// 10MB 이상 파일은 백그라운드에서 처리
if (file.content.size() > 10 * 1024 * 1024) {
    auto& thread_mgr = integration::thread_integration_manager::instance();
    thread_mgr.submit_task([file = std::move(file)]() {
        // 파일 저장 또는 처리
        process_large_file(file);
    });
}
```

**Phase 3 완료 기준**:
- ✅ Cookie 파싱/직렬화 테스트 통과
- ✅ Multipart 파싱 테스트 통과
- ✅ 파일 업로드 (텍스트, 이미지, 대용량) 검증
- ✅ 성능 테스트 (1000개 쿠키, 100MB 파일)

**Phase 3 총 기간**: 7일

---

### Phase 4: HTTP 고급 기능 구현

**목표**: Chunked encoding 및 자동 압축 구현

**우선순위**: Medium 🟡

#### Step 4.1: Chunked Transfer Encoding 구현

**파일**: `src/core/http_server.cpp`, `src/internal/http_parser.cpp`

**변경 내역**:

**청킹 활성화 로직**:
```cpp
// http_server.cpp
void prepare_response(http_response& response) {
    constexpr size_t CHUNKED_THRESHOLD = 8 * 1024;  // 8KB

    // HTTP/1.1 + 큰 응답 → 청킹 활성화
    if (response.version == http_version::HTTP_1_1 &&
        response.body.size() > CHUNKED_THRESHOLD) {
        response.use_chunked_encoding = true;
        NETWORK_LOG_DEBUG("Enabled chunked encoding for " +
                         std::to_string(response.body.size()) + " byte response");
    }
}
```

**청킹 직렬화**:
```cpp
// http_parser.cpp
std::vector<uint8_t> serialize_chunked_response(const http_response& response) {
    std::vector<uint8_t> result;

    // Status line + headers
    auto header_data = serialize_headers(response);
    result.insert(result.end(), header_data.begin(), header_data.end());

    // Transfer-Encoding 헤더 추가
    std::string te_header = "Transfer-Encoding: chunked\r\n\r\n";
    result.insert(result.end(), te_header.begin(), te_header.end());

    // Body를 청크로 분할
    constexpr size_t CHUNK_SIZE = 8192;  // 8KB chunks
    size_t offset = 0;

    while (offset < response.body.size()) {
        size_t chunk_size = std::min(CHUNK_SIZE, response.body.size() - offset);

        // Chunk size (hex) + "\r\n"
        std::ostringstream oss;
        oss << std::hex << chunk_size << "\r\n";
        std::string size_line = oss.str();
        result.insert(result.end(), size_line.begin(), size_line.end());

        // Chunk data + "\r\n"
        result.insert(result.end(),
                     response.body.begin() + offset,
                     response.body.begin() + offset + chunk_size);
        result.push_back('\r');
        result.push_back('\n');

        offset += chunk_size;
    }

    // Last chunk: "0\r\n\r\n"
    std::string last_chunk = "0\r\n\r\n";
    result.insert(result.end(), last_chunk.begin(), last_chunk.end());

    NETWORK_LOG_DEBUG("Serialized chunked response: " +
                     std::to_string(result.size()) + " bytes");

    return result;
}
```

**검증 방법**:
- 작은 응답 (< 8KB, 청킹 비활성화)
- 큰 응답 (> 8KB, 청킹 활성화)
- 매우 큰 응답 (10MB+)
- 클라이언트 파싱 (curl, browsers)

**기간**: 3일

**작업 항목**:
- [ ] 청킹 활성화 로직 구현
- [ ] 청킹 직렬화 구현
- [ ] 임계값 설정 (환경 변수)
- [ ] 단위 테스트
- [ ] 통합 테스트 (큰 파일 다운로드)

**logger_system 통합**:
```cpp
NETWORK_LOG_INFO("Sending chunked response: " +
                std::to_string(num_chunks) + " chunks, " +
                std::to_string(total_size) + " bytes");
```

**monitoring_system 통합**:
```cpp
monitoring.report_counter("http_chunked_responses_total", 1);
monitoring.report_histogram("http_chunk_count", num_chunks);
```

#### Step 4.2: 자동 응답 압축 구현

**파일**: `src/utils/compression_pipeline.cpp`, `src/core/http_server.cpp`

**변경 내역**:

**압축 유틸리티**:
```cpp
// compression_pipeline.cpp
Result<std::vector<uint8_t>> compression_pipeline::compress_gzip(
    const std::vector<uint8_t>& data) {

    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    // gzip 초기화 (windowBits = 15 + 16 for gzip)
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return Result<std::vector<uint8_t>>::error(
            error_codes::common_errors::runtime_error,
            "Failed to initialize gzip compression");
    }

    stream.avail_in = data.size();
    stream.next_in = const_cast<Bytef*>(data.data());

    std::vector<uint8_t> compressed;
    compressed.resize(deflateBound(&stream, data.size()));

    stream.avail_out = compressed.size();
    stream.next_out = compressed.data();

    int ret = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        return Result<std::vector<uint8_t>>::error(
            error_codes::common_errors::runtime_error,
            "gzip compression failed");
    }

    compressed.resize(stream.total_out);

    NETWORK_LOG_DEBUG("gzip: " + std::to_string(data.size()) + " -> " +
                     std::to_string(compressed.size()) + " bytes (" +
                     std::to_string(100 - (compressed.size() * 100 / data.size())) +
                     "% reduction)");

    return Result<std::vector<uint8_t>>::ok(std::move(compressed));
}

Result<std::vector<uint8_t>> compression_pipeline::compress_deflate(
    const std::vector<uint8_t>& data) {
    // deflate 구현 (windowBits = -15 for raw deflate)
    // ... (gzip와 유사, windowBits만 다름)
}
```

**HTTP 서버 통합**:
```cpp
// http_server.cpp
void apply_compression(http_response& response, const http_request& request) {
    constexpr size_t COMPRESSION_THRESHOLD = 1024;  // 1KB

    // 작은 응답은 압축 안 함
    if (response.body.size() < COMPRESSION_THRESHOLD) {
        return;
    }

    // Accept-Encoding 확인
    auto accept_encoding = request.get_header("Accept-Encoding");
    if (!accept_encoding) {
        return;
    }

    compression_algorithm algo = compression_algorithm::none;
    if (accept_encoding->find("gzip") != std::string::npos) {
        algo = compression_algorithm::gzip;
    } else if (accept_encoding->find("deflate") != std::string::npos) {
        algo = compression_algorithm::deflate;
    } else {
        return;
    }

    // 압축 시도
    size_t original_size = response.body.size();
    auto compressed = compression_pipeline::compress(response.body, algo);

    if (compressed.is_error()) {
        NETWORK_LOG_WARN("Compression failed: " + compressed.error_message());
        return;
    }

    auto& compressed_data = compressed.value();

    // 압축 효과 확인 (최소 10% 감소)
    if (compressed_data.size() < original_size * 0.9) {
        response.body = std::move(compressed_data);
        response.set_header("Content-Encoding",
                          algo == compression_algorithm::gzip ? "gzip" : "deflate");

        double ratio = 100.0 - (response.body.size() * 100.0 / original_size);
        NETWORK_LOG_INFO("Compressed response: " +
                        std::to_string(original_size) + " -> " +
                        std::to_string(response.body.size()) + " bytes (" +
                        std::to_string(ratio) + "% reduction)");

        // 모니터링
        monitoring_integration_manager::instance().report_gauge(
            "http_compression_ratio", ratio);
    } else {
        NETWORK_LOG_DEBUG("Compression not beneficial, using original");
    }
}
```

**검증 방법**:
- 텍스트 응답 (HTML, JSON, XML) - 높은 압축률
- 이미지 응답 (JPEG, PNG) - 낮은 압축률, 압축 비활성화 확인
- Accept-Encoding 헤더 변형 (gzip, deflate, 없음)
- 압축률 측정

**기간**: 4일

**작업 항목**:
- [ ] gzip 압축 구현
- [ ] deflate 압축 구현
- [ ] HTTP 서버 통합
- [ ] 압축 효과 측정 로직
- [ ] 단위 테스트 (압축/해제 왕복)
- [ ] 통합 테스트 (다양한 Content-Type)
- [ ] 성능 벤치마크

**logger_system 통합**:
```cpp
NETWORK_LOG_INFO("Compression: " + algorithm_name + ", " +
                std::to_string(original_size) + " -> " +
                std::to_string(compressed_size) + " bytes");
```

**monitoring_system 통합**:
```cpp
monitoring.report_counter("http_compression_total", 1, {
    {"algorithm", algorithm_name}
});
monitoring.report_histogram("http_compression_ratio", ratio);
monitoring.report_histogram("http_compression_time_ms", duration_ms);
```

**thread_system 통합** (대용량 압축):
```cpp
// 1MB 이상은 백그라운드에서 압축
if (response.body.size() > 1024 * 1024) {
    auto& thread_mgr = integration::thread_integration_manager::instance();
    auto future = thread_mgr.submit_task([body = response.body, algo]() {
        return compression_pipeline::compress(body, algo);
    });
    response.body = future.get().value();
}
```

**Phase 4 완료 기준**:
- ✅ 청킹 테스트 통과 (다양한 크기)
- ✅ 압축 테스트 통과 (gzip, deflate)
- ✅ 압축률 검증 (텍스트 > 50%, 이미지 skip)
- ✅ 성능 오버헤드 < 10% (압축 비활성화 대비)

**Phase 4 총 기간**: 7일

---

### Phase 5: 테스트 및 샘플 통합

**목표**: 포괄적인 테스트 커버리지 및 예제 프로그램 추가

**우선순위**: High 🟠

#### Step 5.1: 단위 테스트 추가

**파일**: `tests/unit/http_parser_test.cpp`

**테스트 케이스**:

```cpp
// tests/unit/http_parser_test.cpp
TEST(HTTPParserTest, ParseCookies) {
    http_request req;
    req.set_header("Cookie", "session_id=abc123; user_pref=dark_mode");

    http_parser::parse_cookies(req);

    EXPECT_EQ(req.cookies.size(), 2);
    EXPECT_EQ(req.cookies["session_id"], "abc123");
    EXPECT_EQ(req.cookies["user_pref"], "dark_mode");
}

TEST(HTTPParserTest, ParseMultipartFormData) {
    http_request req;
    req.set_header("Content-Type",
                  "multipart/form-data; boundary=----WebKitFormBoundary");
    req.body = create_multipart_body();  // 테스트 데이터 생성

    auto result = http_parser::parse_multipart_form_data(req);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(req.form_data.size(), 2);
    EXPECT_EQ(req.files.size(), 1);
    EXPECT_EQ(req.form_data["username"], "testuser");
    EXPECT_EQ(req.files["avatar"].filename, "profile.jpg");
}

TEST(HTTPParserTest, ChunkedEncoding) {
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.body = std::vector<uint8_t>(10000, 'A');  // 10KB

    auto serialized = http_parser::serialize_response(resp);

    // Transfer-Encoding 헤더 확인
    std::string data(serialized.begin(), serialized.end());
    EXPECT_NE(data.find("Transfer-Encoding: chunked"), std::string::npos);

    // 청크 형식 검증
    EXPECT_NE(data.find("0\r\n\r\n"), std::string::npos);  // Last chunk
}

TEST(CompressionTest, GzipCompression) {
    std::string original = "Hello, World! " * 1000;  // 반복 데이터
    std::vector<uint8_t> data(original.begin(), original.end());

    auto compressed = compression_pipeline::compress_gzip(data);

    ASSERT_TRUE(compressed.is_ok());
    EXPECT_LT(compressed.value().size(), data.size() / 2);  // > 50% 압축

    auto decompressed = compression_pipeline::decompress_gzip(compressed.value());
    ASSERT_TRUE(decompressed.is_ok());
    EXPECT_EQ(decompressed.value(), data);
}

TEST(HTTPServerTest, RequestBuffering) {
    // 청크별 수신 시뮬레이션
    http_request_buffer buffer;

    // 헤더만
    std::vector<uint8_t> chunk1 = make_chunk("GET /test HTTP/1.1\r\n");
    EXPECT_FALSE(buffer.append(chunk1));
    EXPECT_FALSE(buffer.is_complete());

    // 헤더 끝
    std::vector<uint8_t> chunk2 = make_chunk("Content-Length: 10\r\n\r\n");
    EXPECT_FALSE(buffer.append(chunk2));
    EXPECT_FALSE(buffer.is_complete());

    // Body
    std::vector<uint8_t> chunk3 = make_chunk("0123456789");
    EXPECT_TRUE(buffer.append(chunk3));
    EXPECT_TRUE(buffer.is_complete());
}
```

**기간**: 3일

**작업 항목**:
- [ ] Cookie 테스트
- [ ] Multipart 테스트
- [ ] 청킹 테스트
- [ ] 압축 테스트
- [ ] 버퍼링 테스트
- [ ] 에러 케이스 테스트

#### Step 5.2: 통합 테스트 추가

**파일**: `tests/integration/http_server_client_test.cpp`

**테스트 시나리오**:

```cpp
// tests/integration/http_server_client_test.cpp
TEST(HTTPIntegrationTest, CookieRoundTrip) {
    // 서버 시작
    http_server server("127.0.0.1", 8080);
    server.add_route(http_method::GET, "/set-cookie",
        [](const request_context& ctx) {
            http_response resp(200);
            resp.set_cookie("session_id", "test123", "/", 3600);
            resp.body = "Cookie set";
            return resp;
        });
    server.start();

    // 클라이언트 요청
    http_client client;
    auto resp1 = client.get("http://127.0.0.1:8080/set-cookie");

    ASSERT_TRUE(resp1.is_ok());
    auto set_cookie_header = resp1.value().get_header("Set-Cookie");
    ASSERT_TRUE(set_cookie_header.has_value());
    EXPECT_NE(set_cookie_header->find("session_id=test123"), std::string::npos);
}

TEST(HTTPIntegrationTest, FileUpload) {
    http_server server("127.0.0.1", 8080);
    server.add_route(http_method::POST, "/upload",
        [](const request_context& ctx) {
            EXPECT_EQ(ctx.request.files.size(), 1);
            auto& file = ctx.request.files.at("file");
            EXPECT_EQ(file.filename, "test.txt");
            EXPECT_GT(file.content.size(), 0);

            http_response resp(200);
            resp.body = "File uploaded: " + std::to_string(file.content.size()) + " bytes";
            return resp;
        });
    server.start();

    // 클라이언트 파일 업로드
    http_client client;
    multipart_form form;
    form.add_file("file", "test.txt", "text/plain", "Hello, World!");

    auto resp = client.post("http://127.0.0.1:8080/upload", form);

    ASSERT_TRUE(resp.is_ok());
    EXPECT_EQ(resp.value().status_code, 200);
}

TEST(HTTPIntegrationTest, ChunkedResponse) {
    http_server server("127.0.0.1", 8080);
    server.add_route(http_method::GET, "/large",
        [](const request_context& ctx) {
            http_response resp(200);
            resp.body = std::vector<uint8_t>(100000, 'A');  // 100KB
            return resp;
        });
    server.start();

    http_client client;
    auto resp = client.get("http://127.0.0.1:8080/large");

    ASSERT_TRUE(resp.is_ok());
    EXPECT_EQ(resp.value().body.size(), 100000);
    // 청킹 여부는 클라이언트가 투명하게 처리
}

TEST(HTTPIntegrationTest, Compression) {
    http_server server("127.0.0.1", 8080);
    server.add_route(http_method::GET, "/json",
        [](const request_context& ctx) {
            http_response resp(200);
            resp.set_header("Content-Type", "application/json");
            resp.body = R"({"data": ")" + std::string(10000, 'A') + R"("})";
            return resp;
        });
    server.start();

    http_client client;
    auto resp = client.get("http://127.0.0.1:8080/json", {
        {"Accept-Encoding", "gzip"}
    });

    ASSERT_TRUE(resp.is_ok());
    auto encoding = resp.value().get_header("Content-Encoding");
    EXPECT_TRUE(encoding.has_value());
    EXPECT_EQ(*encoding, "gzip");
    // 클라이언트가 자동 압축 해제
}
```

**기간**: 3일

**작업 항목**:
- [ ] Cookie 통합 테스트
- [ ] 파일 업로드 통합 테스트
- [ ] 청킹 통합 테스트
- [ ] 압축 통합 테스트
- [ ] 동시 요청 테스트
- [ ] 에러 시나리오 테스트

#### Step 5.3: 샘플 프로그램 추가

**파일**:
- `samples/chunked_encoding_demo.cpp`
- `samples/cookie_multipart_demo.cpp`

**Chunked Encoding Demo**:
```cpp
// samples/chunked_encoding_demo.cpp
#include <network_system/core/http_server.h>
#include <network_system/integration/logger_integration.h>

int main() {
    using namespace network_system;

    // Logger 설정
    auto logger = std::make_shared<integration::basic_logger>(
        integration::log_level::info);
    integration::logger_integration_manager::instance().set_logger(logger);

    // HTTP 서버 생성
    core::http_server server("0.0.0.0", 8080);

    // 큰 응답 (자동 청킹)
    server.add_route(core::http_method::GET, "/large",
        [](const core::request_context& ctx) {
            core::http_response resp(200);
            resp.set_header("Content-Type", "text/plain");

            // 1MB 응답 생성
            std::string data;
            for (int i = 0; i < 10000; ++i) {
                data += "This is line " + std::to_string(i) + "\n";
            }
            resp.body = std::vector<uint8_t>(data.begin(), data.end());

            return resp;
        });

    // 압축 가능한 응답
    server.add_route(core::http_method::GET, "/json",
        [](const core::request_context& ctx) {
            core::http_response resp(200);
            resp.set_header("Content-Type", "application/json");

            // 반복 데이터 (높은 압축률)
            std::string json = R"({"message": ")" +
                              std::string(10000, 'A') + R"("})";
            resp.body = std::vector<uint8_t>(json.begin(), json.end());

            return resp;
        });

    std::cout << "Server listening on http://0.0.0.0:8080\n";
    std::cout << "Try:\n";
    std::cout << "  curl http://localhost:8080/large\n";
    std::cout << "  curl -H 'Accept-Encoding: gzip' http://localhost:8080/json\n";

    server.start();

    // 서버 실행 대기
    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}
```

**Cookie & Multipart Demo**:
```cpp
// samples/cookie_multipart_demo.cpp
#include <network_system/core/http_server.h>
#include <network_system/integration/logger_integration.h>
#include <fstream>

int main() {
    using namespace network_system;

    // Logger 설정
    auto logger = std::make_shared<integration::basic_logger>(
        integration::log_level::info);
    integration::logger_integration_manager::instance().set_logger(logger);

    core::http_server server("0.0.0.0", 8080);

    // 쿠키 설정
    server.add_route(core::http_method::GET, "/login",
        [](const core::request_context& ctx) {
            core::http_response resp(200);
            resp.set_cookie("session_id", "user123", "/", 3600, true, false);
            resp.body = "Logged in! Session cookie set.";
            return resp;
        });

    // 쿠키 확인
    server.add_route(core::http_method::GET, "/profile",
        [](const core::request_context& ctx) {
            auto& cookies = ctx.request.cookies;
            if (cookies.find("session_id") != cookies.end()) {
                core::http_response resp(200);
                resp.body = "Welcome, " + cookies.at("session_id");
                return resp;
            } else {
                core::http_response resp(401);
                resp.body = "Unauthorized. Please login.";
                return resp;
            }
        });

    // 파일 업로드
    server.add_route(core::http_method::POST, "/upload",
        [](const core::request_context& ctx) {
            core::http_response resp(200);

            if (ctx.request.files.empty()) {
                resp.status_code = 400;
                resp.body = "No file uploaded";
                return resp;
            }

            // 첫 번째 파일 저장
            auto& file = ctx.request.files.begin()->second;
            std::string filename = "uploaded_" + file.filename;

            std::ofstream ofs(filename, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(file.content.data()),
                     file.content.size());
            ofs.close();

            resp.body = "File uploaded: " + filename + " (" +
                       std::to_string(file.content.size()) + " bytes)";
            return resp;
        });

    std::cout << "Server listening on http://0.0.0.0:8080\n";
    std::cout << "Try:\n";
    std::cout << "  curl http://localhost:8080/login\n";
    std::cout << "  curl -b cookies.txt http://localhost:8080/profile\n";
    std::cout << "  curl -F 'file=@test.txt' http://localhost:8080/upload\n";

    server.start();

    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}
```

**기간**: 2일

**작업 항목**:
- [ ] `chunked_encoding_demo.cpp` 작성
- [ ] `cookie_multipart_demo.cpp` 작성
- [ ] CMakeLists.txt 업데이트
- [ ] README 업데이트 (샘플 설명)

**Phase 5 완료 기준**:
- ✅ 단위 테스트 커버리지 > 90%
- ✅ 통합 테스트 통과
- ✅ 샘플 프로그램 실행 성공
- ✅ 문서화 완료

**Phase 5 총 기간**: 8일

---

### Phase 6: 최종 검증 및 정리

**목표**: 프로덕션 배포 준비

**우선순위**: Critical 🔴

#### Step 6.1: Sanitizer 빌드 검증

**작업 항목**:

```bash
# AddressSanitizer (메모리 누수, 버퍼 오버플로)
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..
make -j$(nproc)
./bin/NetworkSystemTests

# ThreadSanitizer (데이터 레이스, 데드락)
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON ..
make -j$(nproc)
./bin/NetworkSystemTests

# UndefinedBehaviorSanitizer (UB)
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON ..
make -j$(nproc)
./bin/NetworkSystemTests
```

**기간**: 2일

**작업 항목**:
- [ ] AddressSanitizer 빌드 및 실행
- [ ] ThreadSanitizer 빌드 및 실행
- [ ] UndefinedBehaviorSanitizer 빌드 및 실행
- [ ] 모든 경고 해결
- [ ] Sanitizer 리포트 문서화

#### Step 6.2: 성능 벤치마크

**작업 항목**:

```cpp
// benchmarks/http_bench.cpp
void BM_HTTPRequestParsing(benchmark::State& state) {
    std::string request = "GET /test HTTP/1.1\r\n"
                         "Host: localhost\r\n"
                         "Cookie: session=abc123\r\n"
                         "\r\n";
    std::vector<uint8_t> data(request.begin(), request.end());

    for (auto _ : state) {
        auto req = http_parser::parse_request(data);
        benchmark::DoNotOptimize(req);
    }
}
BENCHMARK(BM_HTTPRequestParsing);

void BM_Compression(benchmark::State& state) {
    std::vector<uint8_t> data(state.range(0), 'A');

    for (auto _ : state) {
        auto compressed = compression_pipeline::compress_gzip(data);
        benchmark::DoNotOptimize(compressed);
    }

    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_Compression)->Range(1024, 1024*1024);
```

**성능 목표**:

| 작업 | 목표 | 측정 방법 |
|------|------|-----------|
| HTTP 파싱 | > 100,000 req/s | Google Benchmark |
| Cookie 파싱 | > 50,000 req/s | Google Benchmark |
| Multipart 파싱 | > 1,000 req/s (1MB 파일) | Google Benchmark |
| gzip 압축 | > 50 MB/s | Google Benchmark |
| Chunked 직렬화 | < 5% 오버헤드 | Google Benchmark |

**기간**: 3일

**작업 항목**:
- [ ] 벤치마크 작성
- [ ] 베이스라인 측정
- [ ] 성능 최적화 (필요 시)
- [ ] 성능 리포트 작성

#### Step 6.3: 문서 업데이트

**작업 항목**:

1. **API 문서** (`docs/api/http.md`):
   - Cookie API
   - Multipart API
   - Chunked encoding 설정
   - 압축 설정

2. **Migration Guide** (`docs/migration/v2.0.md`):
   - 변경 사항 요약
   - 코드 마이그레이션 예제
   - Breaking changes

3. **CHANGELOG.md**:
   - 모든 변경 사항 기록
   - 버그 수정 목록
   - 신규 기능 목록

4. **README.md**:
   - 새 기능 강조
   - 빠른 시작 가이드 업데이트

**기간**: 2일

**작업 항목**:
- [ ] API 문서 작성
- [ ] Migration Guide 작성
- [ ] CHANGELOG.md 업데이트
- [ ] README.md 업데이트
- [ ] 샘플 코드 문서화

**Phase 6 완료 기준**:
- ✅ 모든 Sanitizer clean
- ✅ 성능 목표 달성
- ✅ 문서 완성
- ✅ CI/CD 파이프라인 green

**Phase 6 총 기간**: 7일

---

## 시스템 통합 전략

### logger_system 통합 상세

**로깅 레벨 전략**:

| 레벨 | 용도 | 예시 |
|------|------|------|
| TRACE | 상세 디버깅 | "Cookie: name=value", "Multipart boundary: ..." |
| DEBUG | 작업 진행 상황 | "Applied gzip compression", "Parsed 10 cookies" |
| INFO | 중요 이벤트 | "HTTP GET /api - 200 (1234 bytes)" |
| WARN | 경고 (동작 계속) | "Compression not beneficial", "Large request (9MB)" |
| ERROR | 에러 (요청 실패) | "Parse error: invalid Content-Length", "File too large" |
| FATAL | 치명적 에러 (서버 중단) | "Memory allocation failed" |

**로깅 포인트**:

```cpp
// HTTP 요청 수신
NETWORK_LOG_INFO("HTTP " + method + " " + path + " from " + client_ip);

// 파싱 성공
NETWORK_LOG_DEBUG("Parsed request: " + std::to_string(headers.size()) +
                 " headers, " + std::to_string(body.size()) + " bytes body");

// Cookie 파싱
NETWORK_LOG_TRACE("Parsed cookie: " + name + "=" + value);

// Multipart 파싱
NETWORK_LOG_INFO("Uploaded file: " + filename + " (" +
                std::to_string(size) + " bytes)");

// 압축 적용
NETWORK_LOG_DEBUG("Compressed: " + std::to_string(original) + " -> " +
                 std::to_string(compressed) + " bytes (" +
                 std::to_string(ratio) + "% reduction)");

// 청킹 적용
NETWORK_LOG_DEBUG("Using chunked encoding for " + std::to_string(size) +
                 " byte response");

// 에러
NETWORK_LOG_ERROR("Failed to parse multipart: " + error_message);
NETWORK_LOG_WARN("Request too large: " + std::to_string(size) +
                " bytes (limit: " + std::to_string(limit) + ")");

// 응답 전송
NETWORK_LOG_INFO("HTTP " + std::to_string(status_code) + " (" +
                std::to_string(response_size) + " bytes, " +
                std::to_string(duration_ms) + "ms)");
```

### thread_system 통합 상세

**비동기 작업 전략**:

1. **대용량 Multipart 파싱** (> 10MB):
   ```cpp
   if (request.body.size() > 10 * 1024 * 1024) {
       auto& thread_mgr = integration::thread_integration_manager::instance();
       auto future = thread_mgr.submit_task([request]() {
           return http_parser::parse_multipart_form_data(request);
       });
       auto result = future.get();  // 결과 대기
   }
   ```

2. **대용량 압축** (> 1MB):
   ```cpp
   if (response.body.size() > 1024 * 1024) {
       auto future = thread_mgr.submit_task([body = response.body]() {
           return compression_pipeline::compress_gzip(body);
       });
       response.body = future.get().value();
   }
   ```

3. **파일 저장** (백그라운드):
   ```cpp
   thread_mgr.submit_task([file]() {
       save_uploaded_file(file);
   });
   // 즉시 응답 반환
   ```

**스레드 풀 구성**:
- Worker threads: `std::thread::hardware_concurrency()` (기본값)
- 큐 크기: 1000 (설정 가능)
- 작업 타임아웃: 30초 (HTTP 요청별)

### monitoring_system 통합 상세

**메트릭 정의**:

1. **요청 메트릭**:
   ```cpp
   // 요청 수
   monitoring.report_counter("http_requests_total", 1, {
       {"method", request.method},
       {"path", request.path},
       {"status", std::to_string(status_code)}
   });

   // 요청 처리 시간
   monitoring.report_histogram("http_request_duration_seconds",
                              duration.count());

   // 요청 크기
   monitoring.report_histogram("http_request_size_bytes",
                              request.body.size());

   // 응답 크기
   monitoring.report_histogram("http_response_size_bytes",
                              response.body.size());
   ```

2. **파싱 메트릭**:
   ```cpp
   // Cookie 수
   monitoring.report_histogram("http_cookies_per_request",
                              request.cookies.size());

   // Multipart 파일 수
   monitoring.report_counter("http_multipart_files_total",
                            request.files.size());

   // 파일 크기
   monitoring.report_histogram("http_multipart_file_size_bytes",
                              file.content.size());

   // 파싱 에러
   monitoring.report_counter("http_parse_errors_total", 1, {
       {"type", error_type}
   });
   ```

3. **압축 메트릭**:
   ```cpp
   // 압축 적용 횟수
   monitoring.report_counter("http_compression_total", 1, {
       {"algorithm", "gzip"}
   });

   // 압축률
   monitoring.report_gauge("http_compression_ratio",
                          100.0 - (compressed * 100.0 / original));

   // 압축 시간
   monitoring.report_histogram("http_compression_time_ms",
                              duration.count());
   ```

4. **청킹 메트릭**:
   ```cpp
   // 청킹 응답 수
   monitoring.report_counter("http_chunked_responses_total", 1);

   // 청크 수
   monitoring.report_histogram("http_chunk_count", num_chunks);
   ```

**대시보드 구성** (Grafana):
- HTTP 요청/응답 차트
- 에러율 차트
- 압축률 차트
- 처리 시간 히트맵

### common_system 통합 상세

**Result 타입 활용**:

```cpp
// 파싱 함수
Result<http_request> parse_http_request(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return Result<http_request>::error(
            error_codes::common_errors::invalid_argument,
            "Empty request data");
    }

    http_request req;
    if (!parse_headers(data, req)) {
        return Result<http_request>::error(
            error_codes::common_errors::parse_error,
            "Invalid HTTP headers");
    }

    return Result<http_request>::ok(std::move(req));
}

// 사용
auto result = parse_http_request(data);
if (result.is_error()) {
    NETWORK_LOG_ERROR("Parse failed: " + result.error_message());
    send_error_response(400, "Bad Request");
    return;
}
auto& request = result.value();
```

**에러 코드 체계**:

```cpp
namespace error_codes {
    enum class http_errors {
        parse_error = 4000,
        invalid_method = 4001,
        invalid_version = 4002,
        header_too_large = 4003,
        body_too_large = 4004,
        invalid_content_length = 4005,
        invalid_multipart = 4006,
        compression_failed = 4007,
        // ...
    };
}
```

---

## 검증 및 테스트 계획

### 테스트 커버리지 목표

| 컴포넌트 | 목표 커버리지 | 현재 | 측정 도구 |
|----------|---------------|------|-----------|
| http_parser | 95% | TBD | gcov/lcov |
| http_server | 90% | TBD | gcov/lcov |
| http_client | 90% | TBD | gcov/lcov |
| compression_pipeline | 95% | TBD | gcov/lcov |
| messaging_session | 85% | TBD | gcov/lcov |

### 테스트 분류

**1. 단위 테스트 (Unit Tests)**
- 파일: `tests/unit/*.cpp`
- 실행 시간: < 5분
- 범위: 개별 함수/클래스

**2. 통합 테스트 (Integration Tests)**
- 파일: `tests/integration/*.cpp`
- 실행 시간: < 30분
- 범위: 컴포넌트 간 상호작용

**3. 스트레스 테스트 (Stress Tests)**
- 파일: `tests/stress/*.cpp`
- 실행 시간: < 2시간
- 범위: 고부하 시나리오

**4. 성능 테스트 (Performance Tests)**
- 파일: `benchmarks/*.cpp`
- 실행 시간: < 1시간
- 범위: 처리량, 지연시간

### 테스트 시나리오

**Cookie 테스트**:
- [x] 단일 쿠키 파싱
- [x] 다중 쿠키 파싱
- [x] 공백 처리
- [x] 특수 문자 (URL 인코딩)
- [x] Set-Cookie 직렬화
- [x] 쿠키 속성 (path, domain, expires, etc.)

**Multipart 테스트**:
- [x] 단일 필드
- [x] 다중 필드
- [x] 텍스트 파일 업로드
- [x] 바이너리 파일 업로드
- [x] 대용량 파일 (100MB)
- [x] Content-Type 감지
- [x] Boundary 파싱
- [x] 중첩 multipart

**Chunked 테스트**:
- [x] 작은 응답 (< 8KB, 비활성화)
- [x] 큰 응답 (> 8KB, 활성화)
- [x] 매우 큰 응답 (10MB+)
- [x] 청크 크기 검증
- [x] Last chunk 검증

**압축 테스트**:
- [x] gzip 압축/해제 왕복
- [x] deflate 압축/해제 왕복
- [x] 텍스트 (높은 압축률)
- [x] 이미지 (낮은 압축률, 압축 skip)
- [x] 압축 효과 측정 (< 10% 개선 시 skip)
- [x] Accept-Encoding 협상

**버퍼링 테스트**:
- [x] 완전한 요청 (한 번에 수신)
- [x] 청크별 수신 (100 bytes)
- [x] 헤더만 수신
- [x] 불완전한 body
- [x] 크기 초과 (413)
- [x] 헤더 크기 초과 (431)

**메모리 테스트**:
- [x] 메모리 누수 (AddressSanitizer)
- [x] 버퍼 오버플로
- [x] Use-after-free
- [x] Double free

**동시성 테스트**:
- [x] 데이터 레이스 (ThreadSanitizer)
- [x] 데드락
- [x] 동시 연결 (1000+)
- [x] 동시 요청 (각 연결당 100+)

### CI/CD 파이프라인

**빌드 매트릭스**:

| OS | 컴파일러 | 빌드 타입 | Sanitizer |
|----|----------|-----------|-----------|
| Ubuntu 22.04 | GCC 11 | Release | - |
| Ubuntu 22.04 | GCC 11 | Debug | AddressSanitizer |
| Ubuntu 22.04 | Clang 14 | Debug | ThreadSanitizer |
| Ubuntu 22.04 | Clang 14 | Debug | UndefinedBehaviorSanitizer |
| macOS 12 | AppleClang 14 | Release | - |
| macOS 12 | AppleClang 14 | Debug | AddressSanitizer |
| Windows 10 | MSVC 2022 | Release | - |
| Windows 10 | MSVC 2022 | Debug | - |

**파이프라인 단계**:

1. **Checkout**: 코드 체크아웃
2. **Dependencies**: vcpkg로 의존성 설치
3. **Build**: CMake 빌드
4. **Unit Tests**: 단위 테스트 실행
5. **Integration Tests**: 통합 테스트 실행
6. **Coverage**: 커버리지 보고서 생성
7. **Sanitizers**: Sanitizer 빌드 및 테스트
8. **Benchmarks**: 성능 벤치마크 (main 브랜치만)
9. **Artifacts**: 빌드 산출물 저장

**성공 기준**:
- ✅ 모든 플랫폼 빌드 성공
- ✅ 모든 테스트 통과 (unit + integration)
- ✅ 커버리지 > 85%
- ✅ Sanitizer clean (에러 0)
- ✅ 성능 회귀 < 5% (main 대비)

---

## 위험 관리

### 식별된 위험

| 위험 | 확률 | 영향 | 완화 전략 | 대응 계획 |
|------|------|------|-----------|-----------|
| zlib 빌드 실패 (Windows) | Medium | High | vcpkg 사용, CI 테스트 | 정적 링크 대안 |
| 메모리 누수 재발 | Low | High | AddressSanitizer 상시 검사 | weak_ptr 패턴 재점검 |
| 성능 회귀 (압축 오버헤드) | Medium | Medium | 압축 임계값 조정, 벤치마크 | 압축 비활성화 옵션 제공 |
| API 호환성 깨짐 | Low | High | 기존 테스트 유지, deprecation 경로 | 버전 1.x 브랜치 유지 |
| Multipart 파싱 복잡도 | High | Medium | 단계별 구현, 철저한 테스트 | 외부 라이브러리 고려 |
| 스레드 안전성 이슈 | Low | High | ThreadSanitizer 상시 검사 | 락 전략 재검토 |
| 대용량 파일 OOM | Medium | Medium | 크기 제한, 스트리밍 파싱 | 파일 크기 제한 강제 |

### 위험 대응 절차

**1. 메모리 누수 재발**:
- **감지**: AddressSanitizer 빌드
- **대응**: 즉시 수정, hotfix 릴리스
- **예방**: 모든 콜백에 weak_ptr 패턴 강제

**2. 성능 회귀**:
- **감지**: 벤치마크 자동화
- **대응**: 프로파일링, 최적화
- **예방**: 성능 테스트 CI 통합

**3. 빌드 실패**:
- **감지**: CI 파이프라인
- **대응**: 플랫폼별 수정
- **예방**: 멀티 플랫폼 테스트

### 롤백 계획

**Phase별 롤백 포인트**:

| Phase | 롤백 트리거 | 롤백 방법 |
|-------|-------------|-----------|
| Phase 1 | Sanitizer 에러 | `git revert` 개별 커밋 |
| Phase 2 | 빌드 실패 | 의존성 버전 다운그레이드 |
| Phase 3 | 파싱 에러 | Cookie/Multipart 비활성화 플래그 |
| Phase 4 | 성능 회귀 | 압축/청킹 비활성화 플래그 |
| Phase 5 | 테스트 실패 | Phase 4로 롤백 |
| Phase 6 | 프로덕션 이슈 | main 브랜치로 롤백 |

---

## 일정 및 마일스톤

### 전체 일정

```
Phase 0: 준비 단계              [1일]    ████
Phase 1: 중요 버그 수정          [7일]    ████████████████████████
Phase 2: HTTP 인프라 개선        [6일]    ████████████████████
Phase 3: HTTP 파싱 기능 확장     [7일]    ████████████████████████
Phase 4: HTTP 고급 기능 구현     [7일]    ████████████████████████
Phase 5: 테스트 및 샘플 통합     [8일]    ████████████████████████████
Phase 6: 최종 검증 및 정리       [7일]    ████████████████████████
PR 리뷰 및 머지                  [3일]    ██████████

총 소요 기간: 46일 (약 9주)
```

### 마일스톤

**Milestone 1: 안정성 확보** (Day 8)
- ✅ Phase 1 완료
- ✅ 모든 Sanitizer clean
- ✅ 메모리 누수 0
- ✅ 데드락 0

**Milestone 2: 기반 구축** (Day 14)
- ✅ Phase 2 완료
- ✅ zlib 통합
- ✅ 요청 버퍼링 동작
- ✅ 동기 응답 전송

**Milestone 3: 파싱 기능 완성** (Day 21)
- ✅ Phase 3 완료
- ✅ Cookie 파싱/직렬화
- ✅ Multipart 파싱
- ✅ 파일 업로드 동작

**Milestone 4: 고급 기능 완성** (Day 28)
- ✅ Phase 4 완료
- ✅ Chunked encoding
- ✅ 자동 압축
- ✅ 성능 목표 달성

**Milestone 5: 테스트 완성** (Day 36)
- ✅ Phase 5 완료
- ✅ 테스트 커버리지 > 85%
- ✅ 샘플 프로그램 동작

**Milestone 6: 프로덕션 준비** (Day 43)
- ✅ Phase 6 완료
- ✅ 모든 Sanitizer clean
- ✅ 성능 벤치마크 통과
- ✅ 문서 완성

**Milestone 7: 머지** (Day 46)
- ✅ PR 승인
- ✅ main 브랜치 머지
- ✅ 태그 생성 (v2.0.0)

### 리소스 계획

**개발 리소스**:
- 백엔드 개발자 1명 (풀타임)
- 선택: 추가 개발자 (Phase 3, 4 병렬 작업)

**검증 리소스**:
- QA 엔지니어 1명 (Phase 5, 6)
- 코드 리뷰어 1-2명 (전체 기간)

**인프라 리소스**:
- CI/CD 파이프라인
- 테스트 환경 (Linux, macOS, Windows)
- 성능 테스트 서버

---

## 부록

### A. 참조 커밋 목록

| 커밋 ID | 제목 | Phase | 파일 |
|---------|------|-------|------|
| c2d3031 | fix(session): resolve memory leak caused by circular reference | Phase 1 | `src/session/messaging_session.cpp` |
| 68f9d29 | fix(messaging_server): resolve lock-order-inversion deadlock | Phase 1 | `src/core/messaging_server.cpp` |
| deb86e4 | fix(http): resolve thread safety issue in http_url::parse() | Phase 1 | `src/core/http_client.cpp` |
| d48b6db | fix(tcp): restore graceful shutdown on peer disconnects | Phase 1 | `src/internal/tcp_socket.cpp` |
| 5b344a9 | fix(http): implement request buffering and synchronous response | Phase 2 | `src/core/http_server.cpp` |
| 3f2b74e | feat(http): implement cookie and multipart/form-data parsing | Phase 3 | `src/internal/http_parser.cpp` |
| 428c248 | feat(http): implement chunked transfer encoding | Phase 4 | `src/core/http_server.cpp` |
| d138348 | feat(http): add automatic response compression (gzip/deflate) | Phase 4 | `src/utils/compression_pipeline.cpp` |

### B. 설정 옵션

**CMake 빌드 옵션**:
```cmake
-DBUILD_WITH_LOGGER_SYSTEM=ON          # Logger 통합
-DBUILD_WITH_THREAD_SYSTEM=ON          # Thread 통합
-DBUILD_WITH_MONITORING_SYSTEM=ON      # Monitoring 통합
-DBUILD_WITH_COMMON_SYSTEM=ON          # Common 통합
-DNETWORK_ENABLE_HTTP_COMPRESSION=ON   # 압축 기능
-DNETWORK_HTTP_BUFFER_SIZE=10485760    # 최대 요청 크기 (10MB)
-DNETWORK_HTTP_CHUNK_THRESHOLD=8192    # 청킹 임계값 (8KB)
-DNETWORK_HTTP_COMPRESSION_THRESHOLD=1024  # 압축 임계값 (1KB)
```

**런타임 설정** (환경 변수):
```bash
export NETWORK_HTTP_MAX_REQUEST_SIZE=10485760      # 10MB
export NETWORK_HTTP_MAX_HEADER_SIZE=65536          # 64KB
export NETWORK_HTTP_CHUNK_SIZE=8192                # 8KB
export NETWORK_HTTP_COMPRESSION_LEVEL=6            # zlib 압축 레벨 (1-9)
export NETWORK_HTTP_ENABLE_COMPRESSION=true
export NETWORK_HTTP_ENABLE_CHUNKING=true
```

### C. 에러 코드 참조

| 코드 | 이름 | 설명 | HTTP 상태 |
|------|------|------|-----------|
| 4000 | PARSE_ERROR | 일반 파싱 에러 | 400 |
| 4001 | INVALID_METHOD | 지원하지 않는 HTTP 메서드 | 405 |
| 4002 | INVALID_VERSION | 지원하지 않는 HTTP 버전 | 505 |
| 4003 | HEADER_TOO_LARGE | 헤더 크기 초과 | 431 |
| 4004 | BODY_TOO_LARGE | 바디 크기 초과 | 413 |
| 4005 | INVALID_CONTENT_LENGTH | 잘못된 Content-Length | 400 |
| 4006 | INVALID_MULTIPART | Multipart 파싱 에러 | 400 |
| 4007 | COMPRESSION_FAILED | 압축 실패 | 500 |

### D. 성능 벤치마크 결과 (예상)

| 작업 | 목표 | 예상 결과 |
|------|------|-----------|
| HTTP 요청 파싱 | > 100K req/s | 150K req/s |
| Cookie 파싱 (10개) | > 50K req/s | 80K req/s |
| Multipart 파싱 (1MB) | > 1K req/s | 2K req/s |
| gzip 압축 | > 50 MB/s | 100 MB/s |
| gzip 해제 | > 100 MB/s | 200 MB/s |
| Chunked 직렬화 | < 5% 오버헤드 | 2% 오버헤드 |

---

## 승인 및 변경 이력

| 버전 | 날짜 | 작성자 | 변경 내용 |
|------|------|--------|-----------|
| 1.0 | 2025-11-13 | Claude | 초안 작성 |

---

**문의**:
- 기술 문의: 개발팀
- 일정 문의: 프로젝트 매니저

**다음 단계**:
1. 계획서 리뷰 및 승인
2. Phase 0 시작 (의존성 설치 및 검증)
3. Phase 1 시작 (버그 수정)
