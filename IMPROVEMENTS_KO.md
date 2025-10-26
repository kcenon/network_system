# Network System - 개선 계획

> **Language:** [English](IMPROVEMENTS.md) | **한국어**

## 현재 상태

**버전:** 1.0.0
**최종 검토:** 2025-01-20
**전체 점수:** 3.5/5

### 치명적 이슈

## 1. Session Vector 메모리 누수

**위치:** `include/network_system/core/messaging_server.h:254`

**현재 문제:**
```cpp
class messaging_server {
private:
    std::vector<std::shared_ptr<messaging_session>> sessions_;  // ❌ 절대 정리 안됨!
};
```

**문제점:**
- 종료된 세션이 벡터에 무한정 남음
- 장기 실행 서버에서 메모리 무제한 증가
- 자동 정리 메커니즘 없음

**제안된 해결책:**
```cpp
void cleanup_dead_sessions() {
    sessions_.erase(
        std::remove_if(sessions_.begin(), sessions_.end(),
            [](const auto& session) {
                return !session || session->is_closed();
            }),
        sessions_.end()
    );
}
```

**우선순위:** P0
**작업량:** 1-2일
**영향:** 치명적 (메모리 누수 방지)

---

## 고우선순위 개선사항

### 2. 빠른 Sender에 대한 배압(Backpressure) 없음

클라이언트가 빠른 메시지로 서버를 압도 가능

**해결책:** 큐 크기 체크 및 flow control 메시지

**우선순위:** P1
**작업량:** 2-3일

---

### 3. Client용 Connection Pooling 추가

여러 커넥션 재사용으로 성능 향상

**우선순위:** P2
**작업량:** 3-4일

---

## ~~4. TLS/SSL 지원 추가~~ ✅ 완료

**상태:** v1.4.0에서 구현됨 (Phase 9)

**해결책:**
```cpp
// SSL 스트림을 사용한 보안 소켓 래퍼
class secure_tcp_socket {
public:
    using ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>;

    secure_tcp_socket(asio::ip::tcp::socket socket,
                      asio::ssl::context& ssl_context);

    auto async_handshake(
        asio::ssl::stream_base::handshake_type type,
        std::function<void(std::error_code)> handler) -> void;
};

// SSL 핸드셰이크를 갖춘 보안 세션
class secure_session {
public:
    secure_session(asio::ip::tcp::socket socket,
                   asio::ssl::context& ssl_context,
                   std::string_view server_id);

    auto start_session() -> void;  // SSL 핸드셰이크 수행
    auto send_packet(std::vector<uint8_t>&& data) -> void;
};

// 인증서 로딩을 갖춘 보안 서버
class secure_messaging_server {
public:
    secure_messaging_server(const std::string& server_id,
                           const std::string& cert_file,
                           const std::string& key_file);

    auto start_server(unsigned short port) -> VoidResult;
};

// 선택적 인증서 검증을 갖춘 보안 클라이언트
class secure_messaging_client {
public:
    explicit secure_messaging_client(const std::string& client_id,
                                     bool verify_cert = true);

    auto start_client(const std::string& host, unsigned short port) -> VoidResult;
};
```

**기능:**
- TCP 연결을 위한 완전한 TLS/SSL 암호화
- 서버측 인증서 및 개인키 로딩
- 선택적 클라이언트측 인증서 검증
- 병렬 클래스 계층 구조 (tcp_socket → secure_tcp_socket)
- `BUILD_TLS_SUPPORT` 옵션을 통한 조건부 컴파일 (기본값: ON)
- 암호화 작업에 OpenSSL 사용
- Phase 8의 세션 정리 및 백프레셔 상속

**우선순위:** P2
**작업량:** 5-7일

---

### 5. Client 재연결 로직 추가

자동 재연결 및 지수 백오프

**우선순위:** P3
**작업량:** 2-3일

---

**총 작업량:** 13-19일

---

## 참고 자료

- [Asio Documentation](https://think-async.com/Asio/)
- [Network Programming Best Practices](https://beej.us/guide/bgnet/)
