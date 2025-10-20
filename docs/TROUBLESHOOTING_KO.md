# 문제 해결 가이드

> **Language:** [English](TROUBLESHOOTING.md) | **한국어**

이 가이드는 Network System의 일반적인 문제, 디버깅 기법 및 문제 해결 전략에 대한 솔루션을 제공합니다.

## 목차

- [일반적인 문제 및 솔루션](#일반적인-문제-및-솔루션)
- [디버깅 기법](#디버깅-기법)
- [성능 문제](#성능-문제)
- [연결 문제](#연결-문제)
- [메모리 누수](#메모리-누수)
- [빌드 문제](#빌드-문제)
- [플랫폼별 문제](#플랫폼별-문제)
- [오류 참조](#오류-참조)
- [지원 리소스](#지원-리소스)

## 일반적인 문제 및 솔루션

### 서버가 시작되지 않음

#### 증상
- 서버 프로세스가 즉시 종료됨
- 로그 출력 없음
- 서비스 초기화 실패

#### 진단 단계
1. 시스템 로그 확인
```bash
# Linux/macOS
tail -f /var/log/syslog
journalctl -u network_service -f

# Windows 이벤트 뷰어
eventvwr.msc
```

2. 디버그 모드로 실행
```bash
./network_server --debug --foreground
```

3. 구성 검증
```bash
./network_server --validate-config
```

#### 일반적인 원인 및 솔루션

**포트가 이미 사용 중**
```bash
# 포트 사용 확인
lsof -i :8080  # Linux/macOS
netstat -ano | findstr :8080  # Windows

# 솔루션: 프로세스 종료 또는 다른 포트 사용
kill -9 <PID>
# 또는 구성 변경
export NETWORK_SERVER_PORT=8081
```

**권한 거부됨**
```bash
# 파일 권한 확인
ls -la /etc/network_system/

# 권한 수정
sudo chown -R network_service:network_service /etc/network_system/
sudo chmod 600 /etc/network_system/server.key
```

**종속성 누락**
```bash
# 라이브러리 종속성 확인
ldd network_server  # Linux
otool -L network_server  # macOS
dumpbin /DEPENDENTS network_server.exe  # Windows

# 누락된 라이브러리 설치
sudo apt-get install libssl1.1  # Ubuntu/Debian
brew install openssl  # macOS
```

### 높은 CPU 사용률

#### 증상
- CPU 사용률이 지속적으로 80% 이상
- 시스템이 응답하지 않게 됨
- 응답 시간 증가

#### 진단
```bash
# 스레드별 CPU 사용률 모니터링
top -H -p $(pidof network_server)

# CPU 사용 프로파일링
perf top -p $(pidof network_server)

# 스레드 활동 확인
gdb -p $(pidof network_server)
(gdb) info threads
(gdb) thread apply all bt
```

#### 솔루션

**코드의 무한 루프**
```cpp
// 종료 조건 없는 루프 확인
void process_messages() {
    while (running_) {  // 적절한 종료 조건 확인
        if (!message_queue_.empty()) {
            process_message(message_queue_.pop());
        } else {
            // 바쁜 대기를 방지하기 위해 sleep 추가
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
```

**과도한 로깅**
```cpp
// 프로덕션에서 로그 레벨 낮추기
logger.set_level(LogLevel::WARNING);

// 조건부 로깅 사용
if (logger.should_log(LogLevel::DEBUG)) {
    logger.debug("Expensive debug info: {}", generate_debug_info());
}
```

**락 경합**
```cpp
// 락 프리 데이터 구조 사용
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);

// 또는 읽기-쓰기 락 사용
std::shared_mutex mutex;
// 읽기용
std::shared_lock lock(mutex);
// 쓰기용
std::unique_lock lock(mutex);
```

### 메모리 문제

#### 증상
- 시간이 지남에 따라 메모리 사용량 증가
- 메모리 부족 오류
- 시스템 스왑 사용량 증가

#### 진단
```bash
# 메모리 사용량 모니터링
valgrind --leak-check=full --track-origins=yes ./network_server

# AddressSanitizer 사용
g++ -fsanitize=address -g network_server.cpp
ASAN_OPTIONS=detect_leaks=1 ./network_server

# 메모리 프로파일링
heaptrack ./network_server
heaptrack_gui heaptrack.network_server.*.gz
```

#### 솔루션
자세한 솔루션은 [메모리 누수](#메모리-누수) 섹션을 참조하십시오.

## 디버깅 기법

### 코어 덤프 분석

#### 코어 덤프 활성화
```bash
# 코어 덤프 활성화
ulimit -c unlimited

# 코어 덤프 패턴 설정
echo "/tmp/core.%e.%p.%t" | sudo tee /proc/sys/kernel/core_pattern
```

#### 코어 덤프 분석
```bash
# GDB에서 코어 덤프 열기
gdb network_server core.12345

# 기본 명령어
(gdb) bt                    # 백트레이스
(gdb) info threads          # 모든 스레드 나열
(gdb) thread 3              # 스레드 3으로 전환
(gdb) frame 2               # 프레임 2로 전환
(gdb) info locals           # 로컬 변수 표시
(gdb) print variable_name   # 변수 값 출력
```

### 로깅 및 추적

#### 디버그 로깅 활성화
```cpp
// 상세 로깅 구성
LogConfig config;
config.level = LogLevel::TRACE;
config.include_thread_id = true;
config.include_timestamp = true;
config.include_source_location = true;
logger.configure(config);
```

#### 실행 흐름 추적
```cpp
class TraceGuard {
    std::string function_name_;
    std::chrono::steady_clock::time_point start_;

public:
    TraceGuard(const std::string& name)
        : function_name_(name), start_(std::chrono::steady_clock::now()) {
        LOG_TRACE("Entering: {}", function_name_);
    }

    ~TraceGuard() {
        auto duration = std::chrono::steady_clock::now() - start_;
        LOG_TRACE("Exiting: {} ({}ms)", function_name_,
            std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    }
};

// 사용법
void process_request(const Request& req) {
    TraceGuard guard(__FUNCTION__);
    // 함수 구현
}
```

### 네트워크 패킷 캡처

```bash
# 네트워크 트래픽 캡처
sudo tcpdump -i any -w network.pcap port 8080

# Wireshark로 분석
wireshark network.pcap

# tcpdump로 빠른 분석
tcpdump -r network.pcap -nn -A

# 특정 연결 모니터링
sudo netstat -antp | grep network_server
sudo ss -antp | grep network_server
```

### Strace/DTrace 분석

```bash
# 시스템 콜 추적
strace -f -e network -p $(pidof network_server)

# 파일 작업 추적
strace -f -e file -p $(pidof network_server)

# 시스템 콜 개수 세기
strace -c -p $(pidof network_server)

# macOS dtrace
sudo dtrace -n 'syscall:::entry /pid == $target/ { @[probefunc] = count(); }' -p $(pgrep network_server)
```

## 성능 문제

### 느린 응답 시간

#### 진단
```cpp
class PerformanceProfiler {
    struct Timing {
        std::chrono::microseconds parse_time;
        std::chrono::microseconds process_time;
        std::chrono::microseconds send_time;
    };

    void profile_request(const Request& req) {
        auto start = std::chrono::steady_clock::now();

        // 파싱 단계
        auto parsed = parse_request(req);
        auto parse_end = std::chrono::steady_clock::now();

        // 처리 단계
        auto result = process_request(parsed);
        auto process_end = std::chrono::steady_clock::now();

        // 전송 단계
        send_response(result);
        auto send_end = std::chrono::steady_clock::now();

        // 타이밍 로깅
        LOG_INFO("Request timing - Parse: {}μs, Process: {}μs, Send: {}μs",
            duration_cast<microseconds>(parse_end - start).count(),
            duration_cast<microseconds>(process_end - parse_end).count(),
            duration_cast<microseconds>(send_end - process_end).count());
    }
};
```

#### 일반적인 원인

**데이터베이스 쿼리**
```cpp
// 쿼리 캐싱 추가
class QueryCache {
    std::unordered_map<std::string, CachedResult> cache_;

    Result execute_query(const std::string& query) {
        auto it = cache_.find(query);
        if (it != cache_.end() && !it->second.is_expired()) {
            return it->second.result;
        }

        auto result = db_.execute(query);
        cache_[query] = {result, std::chrono::steady_clock::now()};
        return result;
    }
};
```

**동기 I/O**
```cpp
// 비동기 I/O로 변환
class AsyncIOHandler {
    std::future<std::string> read_file_async(const std::string& path) {
        return std::async(std::launch::async, [path]() {
            std::ifstream file(path);
            return std::string(std::istreambuf_iterator<char>(file), {});
        });
    }
};
```

**락 경합**
```cpp
// 세밀한 락 사용
class ConnectionManager {
    struct ConnectionBucket {
        mutable std::mutex mutex;
        std::vector<Connection> connections;
    };

    static constexpr size_t BUCKET_COUNT = 16;
    std::array<ConnectionBucket, BUCKET_COUNT> buckets_;

    size_t get_bucket_index(ConnectionId id) {
        return std::hash<ConnectionId>{}(id) % BUCKET_COUNT;
    }

    Connection& get_connection(ConnectionId id) {
        auto& bucket = buckets_[get_bucket_index(id)];
        std::lock_guard lock(bucket.mutex);
        // 버킷에서 연결 찾기
    }
};
```

### 처리량 문제

#### 진단
```bash
# 네트워크 처리량 테스트
iperf3 -s  # 서버 측
iperf3 -c server_ip -t 60 -P 10  # 클라이언트 측

# 애플리케이션 처리량 테스트
wrk -t12 -c400 -d30s --latency http://localhost:8080/
```

#### 솔루션

**버퍼 크기 증가**
```cpp
// 소켓 버퍼 튜닝
int send_buffer_size = 1048576;  // 1MB
setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF,
    &send_buffer_size, sizeof(send_buffer_size));

int recv_buffer_size = 1048576;  // 1MB
setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF,
    &recv_buffer_size, sizeof(recv_buffer_size));
```

**배치 처리**
```cpp
class BatchProcessor {
    std::vector<Request> batch_;
    std::mutex batch_mutex_;
    std::condition_variable cv_;

    void add_request(Request req) {
        {
            std::lock_guard lock(batch_mutex_);
            batch_.push_back(std::move(req));
        }

        if (batch_.size() >= BATCH_SIZE) {
            cv_.notify_one();
        }
    }

    void process_batch() {
        std::unique_lock lock(batch_mutex_);
        cv_.wait(lock, [this] {
            return batch_.size() >= BATCH_SIZE || shutdown_;
        });

        auto local_batch = std::move(batch_);
        batch_.clear();
        lock.unlock();

        // 락 없이 배치 처리
        process_all(local_batch);
    }
};
```

## 연결 문제

### 연결 거부됨

#### 진단
```bash
# 서버 리스닝 확인
netstat -tlnp | grep 8080
ss -tlnp | grep 8080

# 연결 테스트
telnet localhost 8080
nc -zv localhost 8080

# 방화벽 규칙 확인
sudo iptables -L -n -v
sudo ufw status verbose
```

#### 솔루션

**방화벽 차단**
```bash
# 방화벽에서 포트 허용
sudo ufw allow 8080/tcp
sudo iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
```

**바인드 주소 문제**
```cpp
// 모든 인터페이스에 바인드
address.sin_addr.s_addr = INADDR_ANY;

// 또는 특정 인터페이스
inet_pton(AF_INET, "0.0.0.0", &address.sin_addr);
```

### 연결 타임아웃

#### 진단
```cpp
class TimeoutDiagnostics {
    void log_timeout_details(const Connection& conn) {
        LOG_ERROR("Connection timeout: {}", conn.id);
        LOG_ERROR("  State: {}", conn.state);
        LOG_ERROR("  Last activity: {}ms ago",
            duration_since(conn.last_activity));
        LOG_ERROR("  Pending writes: {}", conn.write_buffer.size());
        LOG_ERROR("  RTT: {}ms", conn.round_trip_time);
    }
};
```

#### 솔루션

**타임아웃 값 조정**
```cpp
// 느린 네트워크를 위한 타임아웃 증가
struct TimeoutConfig {
    std::chrono::seconds connect_timeout{30};
    std::chrono::seconds read_timeout{60};
    std::chrono::seconds write_timeout{60};
    std::chrono::seconds keep_alive_timeout{300};
};
```

**Keep-Alive 구현**
```cpp
// TCP keep-alive
int enable = 1;
setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));

int idle = 60;  // 60초 후 keep-alive 시작
setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));

int interval = 10;  // 10초마다 keep-alive 전송
setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));

int count = 6;  // 6개의 keep-alive 프로브 전송
setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
```

### 연결 끊김

#### 진단
```bash
# 연결 상태 모니터링
watch 'netstat -tan | grep :8080 | awk "{print \$6}" | sort | uniq -c'

# 네트워크 오류 확인
netstat -s | grep -i error
ip -s link show
```

#### 솔루션

**부분 쓰기 처리**
```cpp
ssize_t send_all(int socket, const char* buffer, size_t length) {
    size_t total_sent = 0;

    while (total_sent < length) {
        ssize_t sent = send(socket, buffer + total_sent,
            length - total_sent, MSG_NOSIGNAL);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 소켓이 쓰기 가능해질 때까지 대기
                continue;
            }
            return -1;  // 오류
        }

        total_sent += sent;
    }

    return total_sent;
}
```

## 메모리 누수

### 탐지

#### Valgrind 사용
```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-out.txt \
         ./network_server
```

#### AddressSanitizer 사용
```bash
# AddressSanitizer로 컴파일
g++ -fsanitize=address -fno-omit-frame-pointer -g -O1 network_server.cpp

# 누수 탐지로 실행
ASAN_OPTIONS=detect_leaks=1:check_initialization_order=1 ./network_server
```

#### HeapTrack 사용
```bash
heaptrack ./network_server
heaptrack_gui heaptrack.network_server.*.gz
```

### 일반적인 메모리 누수 패턴

#### 누락된 Delete
```cpp
// 문제
void process() {
    Buffer* buffer = new Buffer(1024);
    // delete 누락
}

// 솔루션
void process() {
    std::unique_ptr<Buffer> buffer = std::make_unique<Buffer>(1024);
    // 자동 정리
}
```

#### 순환 참조
```cpp
// 문제
class Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;  // 순환 생성
};

// 솔루션
class Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;  // 순환 끊기
};
```

#### 컨테이너 누수
```cpp
// 문제
std::vector<Resource*> resources;
resources.push_back(new Resource());
// 벡터가 지워져도 리소스는 삭제되지 않음

// 솔루션
std::vector<std::unique_ptr<Resource>> resources;
resources.push_back(std::make_unique<Resource>());
// 벡터가 지워지면 자동 정리
```

### 메모리 누수 방지

```cpp
// C 리소스를 위한 RAII 래퍼
template<typename T, typename Deleter>
class RAIIWrapper {
    T* resource_;
    Deleter deleter_;

public:
    explicit RAIIWrapper(T* resource, Deleter deleter)
        : resource_(resource), deleter_(deleter) {}

    ~RAIIWrapper() {
        if (resource_) {
            deleter_(resource_);
        }
    }

    // 복사 작업 삭제
    RAIIWrapper(const RAIIWrapper&) = delete;
    RAIIWrapper& operator=(const RAIIWrapper&) = delete;

    // 이동 작업 허용
    RAIIWrapper(RAIIWrapper&& other) noexcept
        : resource_(std::exchange(other.resource_, nullptr)),
          deleter_(std::move(other.deleter_)) {}
};

// 사용법
RAIIWrapper file(fopen("data.txt", "r"), fclose);
RAIIWrapper ssl_ctx(SSL_CTX_new(TLS_method()), SSL_CTX_free);
```

## 빌드 문제

### 컴파일 오류

#### 헤더 누락
```bash
# 오류: fatal error: openssl/ssl.h: No such file or directory

# 솔루션 - 개발 패키지 설치
sudo apt-get install libssl-dev  # Ubuntu/Debian
sudo yum install openssl-devel  # RHEL/CentOS
brew install openssl  # macOS

# include 경로 업데이트
export CPLUS_INCLUDE_PATH=/usr/local/opt/openssl/include:$CPLUS_INCLUDE_PATH
```

#### 정의되지 않은 참조
```bash
# 오류: undefined reference to `SSL_library_init'

# 솔루션 - 라이브러리 올바르게 링크
g++ network_server.cpp -lssl -lcrypto -lpthread

# 또는 CMakeLists.txt에서
find_package(OpenSSL REQUIRED)
target_link_libraries(network_server OpenSSL::SSL OpenSSL::Crypto)
```

#### 템플릿 인스턴스화 문제
```cpp
// 문제: 템플릿 함수에 대한 정의되지 않은 참조

// 솔루션 1: 헤더에 정의
template<typename T>
class Container {
    T* data_;
public:
    T& get() { return *data_; }  // 인라인 정의
};

// 솔루션 2: 명시적 인스턴스화
// .cpp 파일에서
template class Container<int>;
template class Container<std::string>;
```

### 링킹 오류

#### 심볼 충돌
```bash
# 다중 정의 오류

# 중복 심볼 확인
nm -C *.o | grep "T " | sort | uniq -d

# 내부 링크를 위한 익명 네임스페이스 사용
namespace {
    void internal_function() { }
}
```

#### 라이브러리 순서
```bash
# 올바른 라이브러리 순서 (종속성 마지막)
g++ main.o -lmylib -lssl -lcrypto -lpthread

# 잘못된 순서 (실패함)
g++ main.o -lpthread -lcrypto -lssl -lmylib
```

### CMake 문제

#### 패키지 찾기
```cmake
# 패키지 찾기를 위한 힌트 설정
set(CMAKE_PREFIX_PATH "/usr/local/opt/openssl")
find_package(OpenSSL REQUIRED)

# 또는 pkg-config 사용
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED openssl)
```

#### 크로스 플랫폼 빌드
```cmake
# 플랫폼별 설정
if(WIN32)
    set(PLATFORM_LIBS ws2_32)
elseif(APPLE)
    set(PLATFORM_LIBS "-framework CoreFoundation")
else()
    set(PLATFORM_LIBS pthread dl)
endif()

target_link_libraries(network_server ${PLATFORM_LIBS})
```

## 플랫폼별 문제

### Linux 문제

#### 파일 디스크립터 제한
```bash
# 현재 제한 확인
ulimit -n

# 임시로 제한 증가
ulimit -n 65535

# 영구적으로 제한 증가
# /etc/security/limits.conf
* soft nofile 65535
* hard nofile 65535
```

#### Systemd 서비스 문제
```bash
# 서비스 상태 확인
systemctl status network_service

# 전체 로그 보기
journalctl -u network_service --no-pager

# 일반적인 수정
systemctl daemon-reload  # 서비스 파일 변경 후
systemctl reset-failed network_service  # 실패 상태 지우기
```

### macOS 문제

#### 코드 서명
```bash
# 방화벽 프롬프트를 피하기 위해 바이너리 서명
codesign -s "Developer ID" network_server

# 서명되지 않은 바이너리 허용
spctl --add network_server
```

#### Homebrew 라이브러리 경로
```bash
# 라이브러리 경로 수정
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH

# 또는 install_name_tool 사용
install_name_tool -change @rpath/libssl.dylib /usr/local/lib/libssl.dylib network_server
```

### Windows 문제

#### DLL을 찾을 수 없음
```powershell
# 종속성 확인
dumpbin /DEPENDENTS network_server.exe

# PATH에 추가
$env:PATH += ";C:\Program Files\OpenSSL\bin"

# 또는 실행 파일 디렉토리로 DLL 복사
copy "C:\Program Files\OpenSSL\bin\*.dll" .
```

#### Windows 방화벽
```powershell
# 방화벽 예외 추가
netsh advfirewall firewall add rule name="Network Server" dir=in action=allow program="C:\network\network_server.exe"

# 방화벽 규칙 확인
netsh advfirewall firewall show rule name="Network Server"
```

#### Visual Studio 런타임
```powershell
# Visual C++ 재배포 가능 패키지 설치
# Microsoft 웹사이트에서 다운로드

# 또는 정적으로 링크
# CMake에서:
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

## 오류 참조

### 오류 코드

```cpp
enum class ErrorCode {
    SUCCESS = 0,

    // 연결 오류 (1000-1999)
    CONNECTION_REFUSED = 1001,
    CONNECTION_TIMEOUT = 1002,
    CONNECTION_CLOSED = 1003,
    CONNECTION_RESET = 1004,

    // 프로토콜 오류 (2000-2999)
    INVALID_MESSAGE = 2001,
    UNSUPPORTED_VERSION = 2002,
    AUTHENTICATION_FAILED = 2003,

    // 리소스 오류 (3000-3999)
    OUT_OF_MEMORY = 3001,
    TOO_MANY_CONNECTIONS = 3002,
    RESOURCE_EXHAUSTED = 3003,

    // 시스템 오류 (4000-4999)
    FILE_NOT_FOUND = 4001,
    PERMISSION_DENIED = 4002,
    OPERATION_NOT_SUPPORTED = 4003
};

const char* error_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::CONNECTION_REFUSED:
            return "Connection refused by remote host";
        case ErrorCode::CONNECTION_TIMEOUT:
            return "Connection attempt timed out";
        // ... 더 많은 케이스
    }
}
```

### 오류 복구 전략

```cpp
class ErrorRecovery {
    template<typename Func>
    auto retry_with_backoff(Func func, int max_retries = 3) {
        int retry = 0;
        std::chrono::milliseconds delay(100);

        while (retry < max_retries) {
            try {
                return func();
            } catch (const std::exception& e) {
                if (++retry >= max_retries) {
                    throw;
                }

                LOG_WARN("Attempt {} failed: {}. Retrying in {}ms",
                    retry, e.what(), delay.count());

                std::this_thread::sleep_for(delay);
                delay *= 2;  // 지수 백오프
            }
        }
    }
};
```

## 지원 리소스

### 로깅 모범 사례

```cpp
// 더 나은 디버깅을 위한 구조화된 로깅
class StructuredLogger {
    void log_error(const std::string& message,
                   const std::map<std::string, std::any>& context) {
        json log_entry;
        log_entry["timestamp"] = std::chrono::system_clock::now();
        log_entry["level"] = "ERROR";
        log_entry["message"] = message;

        for (const auto& [key, value] : context) {
            log_entry["context"][key] = value;
        }

        std::cerr << log_entry.dump() << std::endl;
    }
};

// 사용법
logger.log_error("Connection failed", {
    {"remote_ip", "192.168.1.100"},
    {"port", 8080},
    {"error_code", ECONNREFUSED},
    {"retry_count", 3}
});
```

### 성능 메트릭 수집

```cpp
class MetricsCollector {
    void record_latency(const std::string& operation,
                       std::chrono::microseconds duration) {
        histogram_[operation].record(duration.count());

        // 높은 지연시간에 대한 알림
        if (duration > std::chrono::seconds(1)) {
            alert_manager_.send({
                .severity = AlertSeverity::WARNING,
                .message = fmt::format("{} took {}ms",
                    operation, duration.count() / 1000)
            });
        }
    }
};
```

### 도움 받기

1. **먼저 로그 확인**
   - 애플리케이션 로그: `/var/log/network_system/`
   - 시스템 로그: `journalctl`, `dmesg`, 이벤트 뷰어
   - 상세 정보를 위한 디버그 로깅 활성화

2. **정보 수집**
   - 운영 체제 및 버전
   - 컴파일러 버전
   - 라이브러리 버전
   - 구성 설정
   - 최근 변경 사항
   - 오류 메시지 및 스택 추적

3. **최소 재현 생성**
```cpp
// 최소 테스트 케이스
#include <iostream>
#include "network_system.h"

int main() {
    try {
        NetworkServer server;
        server.configure("config.json");
        server.start();

        // 여기서 문제 재현

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

4. **커뮤니티 리소스**
   - GitHub Issues: 버그 보고 및 기능 요청
   - Stack Overflow: 유사한 문제 검색
   - Discord/Slack: 실시간 커뮤니티 지원
   - 문서: API 참조 및 가이드 확인

5. **전문 지원**
   - 엔터프라이즈 지원 계약
   - 컨설팅 서비스
   - 교육 워크샵
   - 우선순위 버그 수정

## 결론

이 문제 해결 가이드는 가장 일반적인 문제와 해결책을 다룹니다. 여기서 다루지 않은 문제의 경우:

1. 더 많은 정보를 수집하기 위해 디버그 로깅 활성화
2. 특정 오류 코드에 대한 오류 참조 확인
3. 디버깅 기법을 사용하여 문제 격리
4. 최소 재현 케이스 생성
5. 커뮤니티 또는 지원 채널에 문의

항상 다음을 기억하십시오:
- 시스템을 최신 상태로 유지
- 로그 및 메트릭 모니터링
- 적절한 오류 처리 구현
- 배포 전 철저한 테스트
- 해결 방법 또는 사용자 지정 솔루션 문서화

운영 절차는 [운영 가이드](OPERATIONS_KO.md)를 참조하십시오.
API 문서는 [API 참조](API_REFERENCE_KO.md)를 참조하십시오.
