# API 참조

> **Language:** [English](API_REFERENCE.md) | **한국어**

Network System 라이브러리에 대한 포괄적인 API 문서입니다.

## 목차

- [개요](#개요)
- [핵심 클래스](#핵심-클래스)
- [네트워킹 구성 요소](#네트워킹-구성-요소)
- [프로토콜 핸들러](#프로토콜-핸들러)
- [유틸리티 클래스](#유틸리티-클래스)
- [오류 처리](#오류-처리)
- [구성](#구성)
- [예제](#예제)

## 개요

Network System은 여러 프로토콜과 플랫폼을 지원하는 모듈식 고성능 네트워킹 라이브러리를 제공합니다.

### 네임스페이스 구조

```cpp
namespace network {
    namespace tcp { /* TCP 구현 */ }
    namespace udp { /* UDP 구현 */ }
    namespace http { /* HTTP 구현 */ }
    namespace websocket { /* WebSocket 구현 */ }
    namespace ssl { /* SSL/TLS 지원 */ }
    namespace utils { /* 유틸리티 함수 */ }
}
```

### 헤더 파일

```cpp
#include <network/server.hpp>          // 서버 클래스
#include <network/client.hpp>          // 클라이언트 클래스
#include <network/protocol.hpp>        // 프로토콜 인터페이스
#include <network/message.hpp>         // 메시지 처리
#include <network/connection.hpp>      // 연결 관리
#include <network/error.hpp>           // 오류 처리
#include <network/config.hpp>          // 구성
```

## 핵심 클래스

### NetworkServer

네트워크 연결을 처리하는 메인 서버 클래스입니다.

```cpp
namespace network {

class NetworkServer {
public:
    /**
     * 새로운 NetworkServer 인스턴스를 생성합니다.
     * @param config 서버 구성 객체
     */
    explicit NetworkServer(const ServerConfig& config);

    /**
     * 서버를 시작하고 연결 수신을 시작합니다.
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> start();

    /**
     * 서버를 정상적으로 중지합니다.
     * @param timeout 정상 종료를 기다리는 최대 시간
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> stop(std::chrono::seconds timeout = std::chrono::seconds(30));

    /**
     * 수신 메시지의 메시지 핸들러를 설정합니다.
     * @param handler 수신 메시지를 처리하는 함수
     */
    void set_message_handler(MessageHandler handler);

    /**
     * 현재 서버 통계를 가져옵니다.
     * @return 현재 메트릭을 포함한 ServerStats 객체
     */
    ServerStats get_stats() const;

    /**
     * 연결된 모든 클라이언트에 메시지를 브로드캐스트합니다.
     * @param message 브로드캐스트할 메시지
     * @return 성공적으로 전송된 클라이언트 수
     */
    size_t broadcast(const Message& message);

    /**
     * 특정 클라이언트에 메시지를 전송합니다.
     * @param client_id 클라이언트 식별자
     * @param message 전송할 메시지
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> send_to(ClientId client_id, const Message& message);

    /**
     * 특정 클라이언트의 연결을 끊습니다.
     * @param client_id 클라이언트 식별자
     * @param reason 연결 해제 이유
     */
    void disconnect_client(ClientId client_id, const std::string& reason = "");

    /**
     * 연결된 클라이언트에 대한 정보를 가져옵니다.
     * @param client_id 클라이언트 식별자
     * @return 발견된 경우 클라이언트 정보를 포함한 Optional<ClientInfo>
     */
    std::optional<ClientInfo> get_client_info(ClientId client_id) const;

    /**
     * 연결된 모든 클라이언트를 나열합니다.
     * @return 클라이언트 식별자 벡터
     */
    std::vector<ClientId> get_connected_clients() const;
};

} // namespace network
```

### NetworkClient

네트워크 서버에 연결하기 위한 클라이언트 클래스입니다.

```cpp
namespace network {

class NetworkClient {
public:
    /**
     * 새로운 NetworkClient 인스턴스를 생성합니다.
     * @param config 클라이언트 구성 객체
     */
    explicit NetworkClient(const ClientConfig& config);

    /**
     * 서버에 연결합니다.
     * @param host 서버 호스트 이름 또는 IP 주소
     * @param port 서버 포트 번호
     * @param timeout 연결 타임아웃
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> connect(const std::string& host, uint16_t port,
                        std::chrono::seconds timeout = std::chrono::seconds(10));

    /**
     * 서버에서 연결을 끊습니다.
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> disconnect();

    /**
     * 서버로 메시지를 전송합니다.
     * @param message 전송할 메시지
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> send(const Message& message);

    /**
     * 서버에서 메시지를 받습니다.
     * @param timeout 수신 타임아웃 (블로킹의 경우 nullopt)
     * @return 수신된 메시지 또는 오류를 포함한 Result<Message>
     */
    Result<Message> receive(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * 요청을 보내고 응답을 기다립니다.
     * @param request 요청 메시지
     * @param timeout 응답 타임아웃
     * @return 응답 또는 오류를 포함한 Result<Message>
     */
    Result<Message> request(const Message& request,
                           std::chrono::seconds timeout = std::chrono::seconds(30));

    /**
     * 클라이언트가 연결되어 있는지 확인합니다.
     * @return 연결된 경우 true, 그렇지 않으면 false
     */
    bool is_connected() const;

    /**
     * 수신 메시지의 메시지 핸들러를 설정합니다.
     * @param handler 수신 메시지를 처리하는 함수
     */
    void set_message_handler(MessageHandler handler);

    /**
     * 연결 통계를 가져옵니다.
     * @return 메트릭을 포함한 ConnectionStats 객체
     */
    ConnectionStats get_stats() const;
};

} // namespace network
```

## 네트워킹 구성 요소

### Connection

단일 네트워크 연결을 나타냅니다.

```cpp
namespace network {

class Connection {
public:
    using Id = uint64_t;

    /**
     * 고유한 연결 식별자를 가져옵니다.
     * @return 연결 ID
     */
    Id get_id() const;

    /**
     * 원격 엔드포인트 정보를 가져옵니다.
     * @return 주소 및 포트를 포함한 Endpoint 객체
     */
    Endpoint get_remote_endpoint() const;

    /**
     * 로컬 엔드포인트 정보를 가져옵니다.
     * @return 주소 및 포트를 포함한 Endpoint 객체
     */
    Endpoint get_local_endpoint() const;

    /**
     * 연결이 활성 상태인지 확인합니다.
     * @return 활성 상태이면 true, 그렇지 않으면 false
     */
    bool is_active() const;

    /**
     * 연결을 통해 데이터를 전송합니다.
     * @param data 전송할 데이터 버퍼
     * @param size 바이트 단위의 데이터 크기
     * @return 전송된 바이트 또는 오류를 포함한 Result<size_t>
     */
    Result<size_t> send(const uint8_t* data, size_t size);

    /**
     * 연결에서 데이터를 받습니다.
     * @param buffer 수신할 버퍼
     * @param size 수신할 최대 크기
     * @param timeout 수신 타임아웃 (선택 사항)
     * @return 수신된 바이트 또는 오류를 포함한 Result<size_t>
     */
    Result<size_t> receive(uint8_t* buffer, size_t size,
                          std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * 연결을 닫습니다.
     */
    void close();

    /**
     * TCP no-delay 옵션을 설정합니다.
     * @param enable 활성화하려면 true, 비활성화하려면 false
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> set_no_delay(bool enable);

    /**
     * keep-alive 옵션을 설정합니다.
     * @param enable 활성화하려면 true, 비활성화하려면 false
     * @param idle keep-alive 프로브를 보내기 전 대기 시간
     * @param interval keep-alive 프로브 간 간격
     * @param count 연결이 끊어지기 전 프로브 수
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> set_keep_alive(bool enable,
                                std::chrono::seconds idle = std::chrono::seconds(60),
                                std::chrono::seconds interval = std::chrono::seconds(10),
                                int count = 6);

    /**
     * 연결 통계를 가져옵니다.
     * @return 메트릭을 포함한 ConnectionStats 객체
     */
    ConnectionStats get_stats() const;
};

} // namespace network
```

### ConnectionPool

재사용 가능한 연결 풀을 관리합니다.

```cpp
namespace network {

class ConnectionPool {
public:
    /**
     * 연결 풀 구성
     */
    struct Config {
        size_t min_connections = 5;
        size_t max_connections = 50;
        std::chrono::seconds idle_timeout = std::chrono::seconds(60);
        std::chrono::seconds connection_timeout = std::chrono::seconds(10);
        bool validate_on_checkout = true;
    };

    /**
     * 새로운 연결 풀을 생성합니다.
     * @param config 풀 구성
     */
    explicit ConnectionPool(const Config& config);

    /**
     * 풀에서 연결을 가져옵니다.
     * @param endpoint 대상 엔드포인트
     * @return 연결 또는 오류를 포함한 Result<std::shared_ptr<Connection>>
     */
    Result<std::shared_ptr<Connection>> acquire(const Endpoint& endpoint);

    /**
     * 연결을 풀로 반환합니다.
     * @param connection 반환할 연결
     */
    void release(std::shared_ptr<Connection> connection);

    /**
     * 풀 통계를 가져옵니다.
     * @return 메트릭을 포함한 PoolStats 객체
     */
    PoolStats get_stats() const;

    /**
     * 모든 유휴 연결을 지웁니다.
     */
    void clear_idle();

    /**
     * 풀의 모든 연결을 검증합니다.
     * @return 제거된 유효하지 않은 연결 수
     */
    size_t validate_all();
};

} // namespace network
```

## 프로토콜 핸들러

### HTTP 핸들러

HTTP 프로토콜 구현입니다.

```cpp
namespace network::http {

class HttpServer {
public:
    /**
     * HTTP 요청 핸들러 함수 타입
     */
    using RequestHandler = std::function<Response(const Request&)>;

    /**
     * HTTP 서버를 생성합니다.
     * @param config 서버 구성
     */
    explicit HttpServer(const HttpConfig& config);

    /**
     * 경로 핸들러를 등록합니다.
     * @param method HTTP 메서드 (GET, POST 등)
     * @param path URL 경로 패턴
     * @param handler 요청 핸들러 함수
     */
    void route(const std::string& method, const std::string& path,
              RequestHandler handler);

    /**
     * 요청 파이프라인에 미들웨어를 추가합니다.
     * @param middleware 미들웨어 함수
     */
    void use(MiddlewareFunc middleware);

    /**
     * HTTP 서버를 시작합니다.
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> start();

    /**
     * HTTP 서버를 중지합니다.
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> stop();
};

class Request {
public:
    /**
     * HTTP 메서드를 가져옵니다.
     * @return HTTP 메서드 문자열
     */
    std::string get_method() const;

    /**
     * 요청 URI를 가져옵니다.
     * @return URI 문자열
     */
    std::string get_uri() const;

    /**
     * 헤더 값을 가져옵니다.
     * @param name 헤더 이름
     * @return 선택적 헤더 값
     */
    std::optional<std::string> get_header(const std::string& name) const;

    /**
     * 모든 헤더를 가져옵니다.
     * @return 헤더 이름-값 쌍의 맵
     */
    std::unordered_map<std::string, std::string> get_headers() const;

    /**
     * 요청 본문을 가져옵니다.
     * @return 문자열로서의 요청 본문
     */
    std::string get_body() const;

    /**
     * 쿼리 매개변수를 가져옵니다.
     * @param name 매개변수 이름
     * @return 선택적 매개변수 값
     */
    std::optional<std::string> get_query_param(const std::string& name) const;

    /**
     * 모든 쿼리 매개변수를 가져옵니다.
     * @return 매개변수 이름-값 쌍의 맵
     */
    std::unordered_map<std::string, std::string> get_query_params() const;
};

class Response {
public:
    /**
     * 상태 코드를 설정합니다.
     * @param code HTTP 상태 코드
     * @return 체이닝을 위한 이 응답에 대한 참조
     */
    Response& status(int code);

    /**
     * 헤더를 설정합니다.
     * @param name 헤더 이름
     * @param value 헤더 값
     * @return 체이닝을 위한 이 응답에 대한 참조
     */
    Response& header(const std::string& name, const std::string& value);

    /**
     * 응답 본문을 설정합니다.
     * @param body 응답 본문
     * @return 체이닝을 위한 이 응답에 대한 참조
     */
    Response& body(const std::string& body);

    /**
     * JSON 응답 본문을 설정합니다.
     * @param json JSON 객체
     * @return 체이닝을 위한 이 응답에 대한 참조
     */
    Response& json(const nlohmann::json& json);

    /**
     * 응답으로 파일을 전송합니다.
     * @param file_path 파일 경로
     * @return 체이닝을 위한 이 응답에 대한 참조
     */
    Response& send_file(const std::string& file_path);
};

} // namespace network::http
```

### WebSocket 핸들러

WebSocket 프로토콜 구현입니다.

```cpp
namespace network::websocket {

class WebSocketServer {
public:
    /**
     * WebSocket 연결 핸들러
     */
    using ConnectionHandler = std::function<void(WebSocketConnection&)>;

    /**
     * WebSocket 서버를 생성합니다.
     * @param config 서버 구성
     */
    explicit WebSocketServer(const WebSocketConfig& config);

    /**
     * 연결 핸들러를 설정합니다.
     * @param handler 연결 핸들러 함수
     */
    void on_connection(ConnectionHandler handler);

    /**
     * WebSocket 서버를 시작합니다.
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> start();

    /**
     * WebSocket 서버를 중지합니다.
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> stop();

    /**
     * 연결된 모든 클라이언트에 메시지를 브로드캐스트합니다.
     * @param message 브로드캐스트할 메시지
     */
    void broadcast(const std::string& message);

    /**
     * 연결된 모든 클라이언트에 바이너리 데이터를 브로드캐스트합니다.
     * @param data 바이너리 데이터
     * @param size 데이터 크기
     */
    void broadcast_binary(const uint8_t* data, size_t size);
};

class WebSocketConnection {
public:
    /**
     * 텍스트 메시지를 전송합니다.
     * @param message 텍스트 메시지
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> send(const std::string& message);

    /**
     * 바이너리 데이터를 전송합니다.
     * @param data 바이너리 데이터
     * @param size 데이터 크기
     * @return 성공 또는 실패를 나타내는 Result<void>
     */
    Result<void> send_binary(const uint8_t* data, size_t size);

    /**
     * 메시지 핸들러를 설정합니다.
     * @param handler 메시지 핸들러 함수
     */
    void on_message(std::function<void(const std::string&)> handler);

    /**
     * 바이너리 메시지 핸들러를 설정합니다.
     * @param handler 바이너리 메시지 핸들러 함수
     */
    void on_binary(std::function<void(const uint8_t*, size_t)> handler);

    /**
     * 닫기 핸들러를 설정합니다.
     * @param handler 닫기 핸들러 함수
     */
    void on_close(std::function<void(int code, const std::string& reason)> handler);

    /**
     * 연결을 닫습니다.
     * @param code 닫기 코드
     * @param reason 닫기 이유
     */
    void close(int code = 1000, const std::string& reason = "");

    /**
     * 연결 정보를 가져옵니다.
     * @return ConnectionInfo 객체
     */
    ConnectionInfo get_info() const;
};

} // namespace network::websocket
```

## 유틸리티 클래스

### Message

네트워크 통신을 위한 메시지 컨테이너입니다.

```cpp
namespace network {

class Message {
public:
    /**
     * 메시지 타입 열거형
     */
    enum class Type {
        REQUEST,
        RESPONSE,
        NOTIFICATION,
        ERROR
    };

    /**
     * 빈 메시지를 생성합니다.
     */
    Message();

    /**
     * 데이터로 메시지를 생성합니다.
     * @param data 메시지 데이터
     */
    explicit Message(const std::string& data);

    /**
     * 바이너리 데이터로 메시지를 생성합니다.
     * @param data 바이너리 데이터
     * @param size 데이터 크기
     */
    Message(const uint8_t* data, size_t size);

    /**
     * 메시지 ID를 가져옵니다.
     * @return 메시지 ID
     */
    uint64_t get_id() const;

    /**
     * 메시지 ID를 설정합니다.
     * @param id 메시지 ID
     */
    void set_id(uint64_t id);

    /**
     * 메시지 타입을 가져옵니다.
     * @return 메시지 타입
     */
    Type get_type() const;

    /**
     * 메시지 타입을 설정합니다.
     * @param type 메시지 타입
     */
    void set_type(Type type);

    /**
     * 메시지 데이터를 문자열로 가져옵니다.
     * @return 메시지 데이터
     */
    std::string get_data() const;

    /**
     * 메시지 데이터를 바이너리로 가져옵니다.
     * @return 데이터 포인터 및 크기 쌍
     */
    std::pair<const uint8_t*, size_t> get_binary_data() const;

    /**
     * 헤더 값을 설정합니다.
     * @param key 헤더 키
     * @param value 헤더 값
     */
    void set_header(const std::string& key, const std::string& value);

    /**
     * 헤더 값을 가져옵니다.
     * @param key 헤더 키
     * @return 선택적 헤더 값
     */
    std::optional<std::string> get_header(const std::string& key) const;

    /**
     * 메시지를 직렬화합니다.
     * @return 직렬화된 메시지 바이트
     */
    std::vector<uint8_t> serialize() const;

    /**
     * 메시지를 역직렬화합니다.
     * @param data 직렬화된 데이터
     * @param size 데이터 크기
     * @return 역직렬화된 메시지 또는 오류를 포함한 Result<Message>
     */
    static Result<Message> deserialize(const uint8_t* data, size_t size);
};

} // namespace network
```

### Buffer

네트워크 I/O를 위한 효율적인 버퍼 관리입니다.

```cpp
namespace network {

class Buffer {
public:
    /**
     * 초기 용량으로 버퍼를 생성합니다.
     * @param capacity 바이트 단위의 초기 용량
     */
    explicit Buffer(size_t capacity = 4096);

    /**
     * 버퍼에 데이터를 씁니다.
     * @param data 쓸 데이터
     * @param size 데이터 크기
     * @return 쓰여진 바이트 수
     */
    size_t write(const uint8_t* data, size_t size);

    /**
     * 버퍼에서 데이터를 읽습니다.
     * @param data 읽을 버퍼
     * @param size 읽을 최대 크기
     * @return 읽은 바이트 수
     */
    size_t read(uint8_t* data, size_t size);

    /**
     * 소비하지 않고 데이터를 엿봅니다.
     * @param data 엿볼 버퍼
     * @param size 엿볼 최대 크기
     * @return 엿본 바이트 수
     */
    size_t peek(uint8_t* data, size_t size) const;

    /**
     * 버퍼에서 바이트를 버립니다.
     * @param size 버릴 바이트 수
     */
    void discard(size_t size);

    /**
     * 읽을 수 있는 바이트 수를 가져옵니다.
     * @return 읽을 수 있는 바이트 수
     */
    size_t readable_bytes() const;

    /**
     * 쓸 수 있는 바이트 수를 가져옵니다.
     * @return 쓸 수 있는 바이트 수
     */
    size_t writable_bytes() const;

    /**
     * 버퍼를 지웁니다.
     */
    void clear();

    /**
     * 용량을 예약합니다.
     * @param capacity 예약할 최소 용량
     */
    void reserve(size_t capacity);
};

} // namespace network
```

## 오류 처리

### 오류 타입

```cpp
namespace network {

/**
 * 네트워크 오류를 위한 기본 예외 클래스
 */
class NetworkException : public std::runtime_error {
public:
    explicit NetworkException(const std::string& message);
    virtual ErrorCode get_error_code() const = 0;
};

/**
 * 연결 관련 오류
 */
class ConnectionException : public NetworkException {
public:
    ConnectionException(const std::string& message, ErrorCode code);
    ErrorCode get_error_code() const override;
};

/**
 * 프로토콜 관련 오류
 */
class ProtocolException : public NetworkException {
public:
    ProtocolException(const std::string& message, ErrorCode code);
    ErrorCode get_error_code() const override;
};

/**
 * 타임아웃 오류
 */
class TimeoutException : public NetworkException {
public:
    explicit TimeoutException(const std::string& message);
    ErrorCode get_error_code() const override;
};

} // namespace network
```

### Result 타입

```cpp
namespace network {

/**
 * 예외 없는 오류 처리를 위한 Result 타입
 */
template<typename T>
class Result {
public:
    /**
     * 성공적인 결과를 생성합니다.
     * @param value 성공 값
     * @return 값을 포함한 Result
     */
    static Result success(T value);

    /**
     * 오류 결과를 생성합니다.
     * @param error 오류 객체
     * @return 오류를 포함한 Result
     */
    static Result error(Error error);

    /**
     * 결과가 성공인지 확인합니다.
     * @return 성공이면 true, 그렇지 않으면 false
     */
    bool is_success() const;

    /**
     * 결과가 오류인지 확인합니다.
     * @return 오류이면 true, 그렇지 않으면 false
     */
    bool is_error() const;

    /**
     * 값을 가져옵니다 (오류이면 throw).
     * @return 성공 값
     * @throws std::runtime_error 결과가 오류인 경우
     */
    T& value();
    const T& value() const;

    /**
     * 오류를 가져옵니다 (성공이면 throw).
     * @return 오류 객체
     * @throws std::runtime_error 결과가 성공인 경우
     */
    Error& error();
    const Error& error() const;

    /**
     * 값 또는 기본값을 가져옵니다.
     * @param default_value 오류인 경우 기본값
     * @return 성공 값 또는 기본값
     */
    T value_or(T default_value) const;

    /**
     * 성공 값을 매핑합니다.
     * @param func 매핑 함수
     * @return 매핑된 결과
     */
    template<typename U>
    Result<U> map(std::function<U(const T&)> func) const;

    /**
     * 오류를 매핑합니다.
     * @param func 오류 매핑 함수
     * @return 매핑된 오류를 포함한 Result
     */
    Result<T> map_error(std::function<Error(const Error&)> func) const;
};

// void용 특수화
template<>
class Result<void> {
public:
    static Result success();
    static Result error(Error error);
    bool is_success() const;
    bool is_error() const;
    void value() const;  // 오류이면 throw
    const Error& error() const;
};

} // namespace network
```

## 구성

### 서버 구성

```cpp
namespace network {

struct ServerConfig {
    // 네트워크 설정
    std::string bind_address = "0.0.0.0";
    uint16_t port = 8080;
    int backlog = 128;

    // 연결 설정
    size_t max_connections = 1000;
    std::chrono::seconds connection_timeout{30};
    std::chrono::seconds keep_alive_timeout{60};

    // 스레드 풀 설정
    size_t worker_threads = std::thread::hardware_concurrency();
    size_t io_threads = 2;

    // 버퍼 설정
    size_t receive_buffer_size = 65536;
    size_t send_buffer_size = 65536;

    // SSL 설정
    bool ssl_enabled = false;
    std::string cert_file;
    std::string key_file;
    std::string ca_file;

    // 프로토콜 설정
    std::vector<std::string> supported_protocols;

    /**
     * 파일에서 구성을 로드합니다.
     * @param file_path 구성 파일 경로
     * @return 구성 또는 오류를 포함한 Result<ServerConfig>
     */
    static Result<ServerConfig> from_file(const std::string& file_path);

    /**
     * JSON에서 구성을 로드합니다.
     * @param json JSON 구성
     * @return 구성 또는 오류를 포함한 Result<ServerConfig>
     */
    static Result<ServerConfig> from_json(const nlohmann::json& json);

    /**
     * 구성을 검증합니다.
     * @return 유효하거나 오류를 나타내는 Result<void>
     */
    Result<void> validate() const;
};

} // namespace network
```

### 클라이언트 구성

```cpp
namespace network {

struct ClientConfig {
    // 연결 설정
    std::chrono::seconds connect_timeout{10};
    std::chrono::seconds read_timeout{30};
    std::chrono::seconds write_timeout{30};

    // 재시도 설정
    int max_retries = 3;
    std::chrono::milliseconds retry_delay{1000};
    bool exponential_backoff = true;

    // 버퍼 설정
    size_t receive_buffer_size = 65536;
    size_t send_buffer_size = 65536;

    // SSL 설정
    bool ssl_enabled = false;
    bool verify_peer = true;
    std::string ca_file;
    std::string cert_file;
    std::string key_file;

    // keep-alive 설정
    bool keep_alive_enabled = true;
    std::chrono::seconds keep_alive_interval{30};

    /**
     * 기본 구성을 생성합니다.
     * @return 기본 클라이언트 구성
     */
    static ClientConfig default_config();

    /**
     * 구성을 검증합니다.
     * @return 유효하거나 오류를 나타내는 Result<void>
     */
    Result<void> validate() const;
};

} // namespace network
```

## 예제

### 기본 TCP 서버

```cpp
#include <network/server.hpp>

int main() {
    // 서버 구성 생성
    network::ServerConfig config;
    config.port = 8080;
    config.max_connections = 100;

    // 서버 생성 및 구성
    network::NetworkServer server(config);

    // 메시지 핸들러 설정
    server.set_message_handler([](const network::Message& msg,
                                  network::Connection& conn) {
        std::cout << "Received: " << msg.get_data() << std::endl;

        // 메시지를 에코백
        network::Message response("Echo: " + msg.get_data());
        conn.send(response.serialize().data(), response.serialize().size());
    });

    // 서버 시작
    auto result = server.start();
    if (!result.is_success()) {
        std::cerr << "Failed to start server: "
                  << result.error().message << std::endl;
        return 1;
    }

    std::cout << "Server listening on port 8080" << std::endl;

    // 중단될 때까지 실행
    std::signal(SIGINT, [](int) {
        std::cout << "Shutting down..." << std::endl;
    });

    // 종료 대기
    server.wait();

    return 0;
}
```

### 기본 TCP 클라이언트

```cpp
#include <network/client.hpp>

int main() {
    // 클라이언트 구성 생성
    network::ClientConfig config;
    config.connect_timeout = std::chrono::seconds(5);

    // 클라이언트 생성
    network::NetworkClient client(config);

    // 서버에 연결
    auto result = client.connect("localhost", 8080);
    if (!result.is_success()) {
        std::cerr << "Failed to connect: "
                  << result.error().message << std::endl;
        return 1;
    }

    // 메시지 전송
    network::Message message("Hello, Server!");
    result = client.send(message);
    if (!result.is_success()) {
        std::cerr << "Failed to send: "
                  << result.error().message << std::endl;
        return 1;
    }

    // 응답 받기
    auto response = client.receive(std::chrono::seconds(5));
    if (response.is_success()) {
        std::cout << "Received: " << response.value().get_data() << std::endl;
    }

    // 연결 끊기
    client.disconnect();

    return 0;
}
```

## 스레드 안전성

Network System 라이브러리의 모든 공개 메서드는 명시적으로 문서화되지 않는 한 스레드 안전합니다. 다음 보장이 제공됩니다:

1. **서버 클래스**: 여러 스레드가 동일한 서버 인스턴스에서 메서드를 안전하게 호출할 수 있습니다
2. **클라이언트 클래스**: 전송 및 수신 작업은 스레드 안전합니다
3. **연결 클래스**: 개별 연결은 동시 작업에 대해 스레드 안전합니다
4. **메시지 클래스**: 메시지 객체는 생성 후 불변입니다
5. **버퍼 클래스**: 스레드 안전하지 않음; 스레드당 별도 인스턴스를 사용하거나 외부 동기화를 추가하십시오

## 성능 고려사항

### 제로 카피 작업

라이브러리는 가능한 경우 제로 카피 작업을 지원합니다.

### 메모리 풀링

자주 할당되는 객체에 메모리 풀을 사용합니다.

### 배치 작업

더 나은 성능을 위해 여러 작업을 배치 처리합니다.

## 플랫폼별 참고사항

### Linux
- I/O 다중화에 epoll 사용
- 로드 밸런싱을 위한 SO_REUSEPORT 지원
- io_uring을 사용한 커널 우회 (선택 사항)

### Windows
- I/O 다중화에 IOCP 사용
- Windows 10 또는 Windows Server 2016+ 필요
- Visual Studio 2019 이상 권장

### macOS
- I/O 다중화에 kqueue 사용
- macOS 10.14 이상 필요
- Xcode 11 이상 권장

## 버전 기록

자세한 버전 기록은 [CHANGELOG.md](CHANGELOG_KO.md)를 참조하십시오.

## 라이선스

이 라이브러리는 MIT 라이선스에 따라 라이선스가 부여됩니다. 자세한 내용은 LICENSE 파일을 참조하십시오.

---

*Last Updated: 2025-10-20*
