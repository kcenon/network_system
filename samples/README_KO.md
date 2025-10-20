# Network System 샘플

> **Language:** [English](README.md) | **한국어**

이 디렉토리는 TCP/UDP 통신, HTTP 클라이언트 기능 및 현대적인 Result<T> 오류 처리 패턴을 포함한 Network System의 네트워킹 기능을 시연하는 예제 프로그램을 포함합니다.

## 최신 업데이트 (2025-10-10)

**Result<T> 오류 처리**: modern_usage.cpp 예제는 이제 타입 안전 오류 처리를 위한 새로운 Result<T> 패턴을 시연합니다. 모든 주요 네트워킹 API(start_server, start_client, send_packet 등)는 이제 예외를 던지는 대신 VoidResult를 반환하여 명시적인 오류 확인과 더 나은 오류 복구 패턴을 제공합니다.

## 사용 가능한 샘플

### 1. 기본 사용법 (`basic_usage.cpp`)
기본적인 네트워크 작업을 시연합니다:
- 네트워크 관리자 설정 및 구성
- TCP 서버 및 클라이언트 작업
- UDP 통신
- HTTP 클라이언트 요청 (GET/POST)
- 바이너리 데이터 전송
- 네트워크 유틸리티 및 진단
- 오류 처리 및 연결 관리

**사용법:**
```bash
./basic_usage
```

### 2. TCP 서버/클라이언트 데모 (`tcp_server_client.cpp`)
고급 TCP 통신 패턴:
- 다중 클라이언트 TCP 서버 구현
- 동시 클라이언트 연결
- 텍스트 및 바이너리 데이터 교환
- 메시지 및 연결 핸들러
- 성능 벤치마킹
- 스레드 안전 서버 작업
- 클라이언트 부하 테스트

**사용법:**
```bash
./tcp_server_client
```

### 3. HTTP 클라이언트 데모 (`http_client_demo.cpp`)
포괄적인 HTTP 클라이언트 기능:
- 다양한 데이터 타입을 사용한 GET 및 POST 요청
- 사용자 정의 헤더 및 인증
- 폼 데이터 및 JSON 페이로드 처리
- 파일 업로드 및 다운로드 작업
- 다양한 HTTP 상태 코드에 대한 오류 처리
- 동시 요청 처리
- 성능 벤치마킹

**사용법:**
```bash
./http_client_demo
```

### 4. 현대적 사용법 (`messaging_system_integration/modern_usage.cpp`)
**신규**: Result<T> 오류 처리 패턴을 사용한 현대적 API를 시연합니다:
- Result<T> 반환 타입을 사용한 현대적 network_system API
- 예외 없이 타입 안전 오류 확인
- 모든 작업에 대한 명시적 오류 코드 처리
- 스레드 풀 및 컨테이너 시스템과의 통합
- 적절한 오류 처리 및 복구 패턴
- 오류 보고를 사용한 비동기 작업

**주요 기능:**
- 모든 서버/클라이언트 작업이 VoidResult 반환
- 명시적 오류 확인: `result.is_err()` 및 `result.error()`
- 상세한 메시지를 포함한 오류 코드
- 우아한 오류 복구 예제

**사용법:**
```bash
cd messaging_system_integration/build
./modern_usage
```

### 5. 모든 샘플 실행 (`run_all_samples.cpp`)
모든 샘플 또는 특정 샘플을 실행하는 유틸리티:

**사용법:**
```bash
# 모든 샘플 실행
./run_all_samples

# 특정 샘플 실행
./run_all_samples basic_usage
./run_all_samples tcp_server_client
./run_all_samples http_client_demo

# 사용 가능한 샘플 목록
./run_all_samples --list

# 도움말 표시
./run_all_samples --help
```

## 샘플 빌드

### 필수 요구사항
- C++20 호환 컴파일러 (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.16 이상
- Network System 라이브러리
- 플랫폼별 네트워킹 라이브러리 (자동으로 처리됨)
- 선택사항: 향상된 HTTP 기능을 위한 libcurl

### 빌드 지침

1. **메인 프로젝트 디렉토리에서:**
```bash
mkdir build && cd build
cmake .. -DBUILD_NETWORK_SAMPLES=ON
make
```

2. **샘플 실행:**
```bash
cd bin
./basic_usage
./tcp_server_client
./http_client_demo
./run_all_samples
```

### 대체 빌드 (샘플만)
```bash
cd samples
mkdir build && cd build
cmake ..
make
```

## 네트워크 구성

### TCP 서버 구성
샘플은 다음 기본 구성을 사용합니다:
- **주소**: `127.0.0.1` (localhost)
- **포트**: TCP는 `8080`, UDP는 `8081`
- **버퍼 크기**: `8192` 바이트
- **타임아웃**: `5000` ms (5초)

### HTTP 클라이언트 구성
- **User-Agent**: `NetworkSystem/1.0`
- **타임아웃**: `30000` ms (30초)
- **최대 리다이렉트**: `5`
- **기본 헤더**: Accept, Accept-Language, Accept-Encoding

### 구성 사용자 정의
샘플에서 네트워크 설정을 수정할 수 있습니다:
```cpp
// TCP 구성
const std::string server_address = "192.168.1.100";
const int server_port = 9090;

// HTTP 구성
http_client->set_timeout(10000);  // 10초
http_client->set_user_agent("MyApp/2.0");
```

## 샘플 출력 예제

### 기본 사용법 출력
```
=== Network System - Basic Usage Example ===

1. Network Manager Setup:
Network manager created
Network configuration set

2. TCP Server Operations:
Starting TCP server on 127.0.0.1:8080
✓ TCP server started successfully
Server status: Running

3. TCP Client Operations:
Connecting to TCP server...
✓ TCP client connected successfully
Connection status: Connected

4. Data Transmission:
Sending message: "Hello from TCP client!"
✓ Message sent successfully
✓ Response received: "Echo: Hello from TCP client!"
...
```

### TCP 서버/클라이언트 데모 출력
```
=== Network System - TCP Server/Client Demo ===

=== TCP Server Demo ===
Starting server on 127.0.0.1:8080
✓ TCP Server started successfully

=== TCP Client 1 Demo ===
[Server] Client connected: client_001
[Client 1] Testing text communication...
[Server] Received from client_001: Hello Server!
[Client 1] Sent: Hello Server!
[Client 1] Received: Echo: Hello Server! (server time: 1640995200)

=== TCP Client 2 Demo ===
[Client 2] Testing binary communication...
[Server] Received binary data from client_002 (4 bytes)
[Client 2] ✓ Binary echo verified
...
```

### HTTP 클라이언트 데모 출력
```
=== Network System - HTTP Client Demo ===

1. Basic GET Requests:
Testing simple GET request...
✓ GET request successful
Response size: 1247 bytes
Status code: 200
Content-Type: application/json

2. POST Requests:
Testing JSON POST request...
✓ JSON POST successful
Status code: 200

3. Headers and Authentication:
Testing basic authentication...
✓ Basic authentication successful
Status code: 200
...
```

## 결과 이해

### 성능 지표
- **응답 시간**: 개별 요청에 걸리는 시간
- **처리량**: 동시 작업에 대한 초당 요청 수
- **성공률**: 성공한 작업의 백분율
- **연결 시간**: 네트워크 연결 설정 시간

### TCP 통신
- **서버 용량**: 동시 클라이언트 연결 수
- **데이터 무결성**: 전송된 데이터 검증
- **연결 안정성**: 지속적인 연결 관리
- **바이너리 프로토콜**: 사용자 정의 프로토콜 구현

### HTTP 작업
- **상태 코드**: 2xx, 4xx, 5xx 응답의 적절한 처리
- **콘텐츠 타입**: JSON, 폼 데이터, 바이너리 및 텍스트 처리
- **인증**: 기본 인증 및 사용자 정의 헤더 지원
- **오류 복구**: 타임아웃 및 재시도 메커니즘

## Result<T> 오류 처리 패턴

### 타입 안전 오류 확인

현대적 network_system API는 명시적 오류 처리를 위해 Result<T> 패턴을 사용합니다:

```cpp
// 오류 확인을 사용한 서버 시작
auto result = server->start_server(8080);
if (result.is_err()) {
    std::cerr << "Server start failed: " << result.error().message
              << " (code: " << result.error().code << ")" << std::endl;
    return -1;
}

// 오류 처리를 사용한 패킷 전송
auto send_result = client->send_packet(data);
if (send_result.is_err()) {
    std::cerr << "Send failed: " << send_result.error().message << std::endl;
    // 재시도 또는 복구 로직 구현
}
```

### 오류 코드 카테고리

네트워크 시스템 오류 코드는 카테고리별로 구성됩니다:
- **연결 오류** (-600 ~ -619): connection_failed, connection_refused, connection_timeout
- **세션 오류** (-620 ~ -639): session_not_found, session_expired
- **송수신 오류** (-640 ~ -659): send_failed, receive_failed
- **서버 오류** (-660 ~ -679): server_already_running, bind_failed

### 오류 복구 패턴

```cpp
// 실패 시 재시도
int max_retries = 3;
for (int i = 0; i < max_retries; ++i) {
    auto result = client->send_packet(data);
    if (result.is_ok()) {
        break;  // 성공
    }

    if (i < max_retries - 1) {
        std::cerr << "Retry " << (i + 1) << "/" << max_retries << std::endl;
        std::this_thread::sleep_for(100ms);
    }
}

// 특정 오류에 대한 폴백
auto result = server->start_server(port);
if (result.is_err()) {
    if (result.error().code == error_codes::network_system::bind_failed) {
        // 대체 포트 시도
        result = server->start_server(port + 1);
    }
}
```

## 고급 사용법

### 사용자 정의 TCP 프로토콜
사용자 정의 메시지 프로토콜 구현:
```cpp
// 사용자 정의 메시지 형식
struct custom_message {
    uint32_t message_id;
    uint32_t payload_size;
    std::vector<uint8_t> payload;
};

// 직렬화 및 전송
auto serialized = serialize_message(custom_msg);
client->send_binary(serialized);
```

### HTTP 클라이언트 확장
사용자 정의 기능 추가:
```cpp
// 특정 헤더를 사용한 사용자 정의 요청
std::map<std::string, std::string> headers = {
    {"Authorization", "Bearer your-token"},
    {"X-API-Version", "2.0"},
    {"Content-Type", "application/json"}
};

auto response = http_client->post(url, json_data, headers);
```

### 동시 서버 패턴
```cpp
// 멀티스레드 서버 처리
server->set_message_handler([](const std::string& msg, const std::string& client_id) {
    // 스레드 풀에서 처리
    thread_pool.enqueue([msg, client_id]() {
        return process_client_message(msg, client_id);
    });
});
```

## 문제 해결

### 일반적인 문제

1. **포트가 이미 사용 중**
   ```
   ✗ Failed to start TCP server
   ```
   - 샘플에서 포트 번호 변경
   - 포트를 사용하는 다른 애플리케이션 확인
   - `netstat -an | grep 8080`을 사용하여 포트 사용 확인

2. **네트워크 연결**
   ```
   ✗ HTTP GET request failed
   ```
   - 인터넷 연결 확인
   - 방화벽 설정 확인
   - 간단한 연결 도구로 테스트 (ping, curl)

3. **권한 오류**
   ```
   ✗ Failed to bind to port
   ```
   - 루트가 아닌 사용자는 1024보다 큰 포트 사용
   - 적절한 권한으로 실행
   - 시스템 네트워크 정책 확인

4. **컴파일 오류**
   - C++20 지원이 활성화되어 있는지 확인
   - 네트워크 시스템 라이브러리가 올바르게 링크되어 있는지 확인
   - 플랫폼별 네트워크 라이브러리가 사용 가능한지 확인

### 플랫폼별 고려사항

#### Windows
- Winsock2 라이브러리 필요 (ws2_32.dll)
- 방화벽이 네트워크 액세스를 요청할 수 있음
- Windows 전용 네트워크 인터페이스 열거 사용

#### Linux
- pthread 라이브러리 필요
- 개발 패키지 설치 필요할 수 있음
- 네트워크 작업에 대한 SELinux 정책 확인

#### macOS
- 내장 네트워크 라이브러리가 작동해야 함
- 시스템 네트워크 기본 설정 확인
- 시스템 환경설정에서 방화벽 구성

### 성능 최적화

1. **버퍼 크기**: 일반적인 메시지 크기에 따라 조정
2. **연결 풀링**: HTTP 연결 재사용
3. **동시 제한**: 스레드 수와 시스템 리소스 균형 조정
4. **타임아웃 값**: 사용 사례에 적절한 타임아웃 설정

### 네트워크 기능 테스트

#### 로컬 테스트
```bash
# TCP 서버를 별도로 테스트
./tcp_server_client &
telnet localhost 8080

# 로컬 서버로 HTTP 클라이언트 테스트
python3 -m http.server 8080 &
./http_client_demo
```

#### 네트워크 테스트
```bash
# 외부 서비스로 테스트
curl -X GET "https://httpbin.org/get"
nc -l 8080  # 테스트용 간단한 TCP 서버
```

## 보안 고려사항

### 네트워크 보안
- 모든 수신 데이터 검증
- 적절한 인증 메커니즘 구현
- 민감한 데이터에 대해 보안 프로토콜 사용 (HTTPS, TLS)
- 사용자 입력을 삭제하여 주입 공격 방지

### 오류 처리
- 오류 메시지에 민감한 정보를 노출하지 않음
- 자격 증명 없이 적절한 로깅 구현
- 세션 토큰에 안전한 난수 생성기 사용
- 타임아웃 및 재시도 로직을 안전하게 처리

## 확장 포인트

### 새 샘플 추가
1. samples 디렉토리에 새 `.cpp` 파일 생성
2. `CMakeLists.txt`의 `SAMPLE_PROGRAMS` 목록에 추가
3. `run_all_samples.cpp` 샘플 레지스트리에 포함
4. 새 샘플 설명으로 이 README 업데이트

### 사용자 정의 프로토콜
네트워크 시스템은 사용자 정의 프로토콜 구현을 지원합니다:
- 사용자 정의 직렬화/역직렬화 구현
- 프로토콜별 오류 처리 추가
- 사용자 정의 메시지 라우팅 로직 생성
- 프로토콜 버전 관리 구현

### 통합 예제
- WebSocket 클라이언트/서버 구현
- JSON 처리를 사용한 REST API 클라이언트
- IoT 기기용 바이너리 프로토콜
- 프록시 및 로드 밸런서 구현

## 도움말 얻기

- 자세한 빌드 지침은 메인 프로젝트 README 확인
- 네트워크 시스템 사용법은 API 문서 검토
- 구현 세부사항은 샘플 소스 코드 검토
- 문제를 격리하기 위해 간단한 네트워크 도구로 테스트

## 라이선스

이 샘플은 Network System 프로젝트와 동일한 BSD 3-Clause License로 제공됩니다.
