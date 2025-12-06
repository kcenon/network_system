# 변경 이력 - Network System

> **언어:** [English](CHANGELOG.md) | **한국어**

Network System 프로젝트의 모든 주요 변경 사항이 이 파일에 문서화됩니다.

형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.0.0/)를 따르며,
이 프로젝트는 [Semantic Versioning](https://semver.org/lang/ko/spec/v2.0.0.html)을 준수합니다.

---

## [미배포]

### 수정됨
- **Thread System Adapter**: 지연 작업마다 분리된 `std::thread`를 생성하는 대신 단일 스케줄러 스레드와 우선순위 큐를 사용하도록 `submit_delayed()` 수정 (#273)
  - 지연 작업 대량 제출 시 스레드 폭발 방지
  - 적절한 스레드 수명주기 관리 (joinable 스케줄러 스레드)
  - 대기 중인 작업 취소와 함께 깔끔한 종료 지원
  - 지연 작업 실행에 대한 포괄적인 단위 테스트 추가

### 리팩토링됨
- **메모리 프로파일러**: `std::thread`에서 `thread_integration_manager::submit_delayed_task()`로 마이그레이션 (#277)
  - 전용 `std::thread worker_` 멤버 제거
  - 주기적 샘플링이 공유 스레드 풀을 통해 실행
  - 지연 작업 스케줄링을 사용한 간격 기반 스냅샷
  - atomic 플래그를 통한 깔끔한 종료 (스레드 join 불필요)

- **gRPC 클라이언트 비동기 호출**: `call_raw_async()`를 `std::thread().detach()`에서 `thread_integration_manager::submit_task()`로 마이그레이션 (#278)
  - 분리된 스레드 생성 대신 공유 스레드 풀로 비동기 gRPC 호출 제출
  - 중앙 집중식 스레드 관리를 통한 제어된 스레드 수명주기
  - 대량 비동기 호출 시 스레드 폭발 방지

- **Send Coroutine Fallback**: `async_send_with_pipeline_no_co()`를 `std::thread().detach()`에서 `thread_integration_manager::submit_task()`로 마이그레이션 (#274)
  - 분리된 스레드 생성 대신 공유 스레드 풀로 작업 제출
  - 관리되는 풀을 통한 제어된 스레드 수명주기
  - 고부하 시 리소스 고갈 방지
  - 스레드 풀 제출 실패에 대한 에러 처리 추가

### 추가됨
- **QUIC 프로토콜 지원 (Phase 4.1)**: messaging_quic_client 공개 API
  - `messaging_client` 패턴을 따르는 `messaging_quic_client` 클래스
    - 기존 TCP/UDP/WebSocket 클라이언트와 일관된 API
    - 내부 잠금을 통한 스레드 안전 작업
    - 완전한 Result<T> 에러 처리
  - 연결 관리: `start_client()`, `stop_client()`, `wait_for_stop()`
  - 기본 스트림에서의 데이터 전송: `send_packet()`
  - 멀티 스트림 지원 (QUIC 전용):
    - `create_stream()`: 양방향 스트림 생성
    - `create_unidirectional_stream()`: 단방향 스트림 생성
    - `send_on_stream()`: 스트림별 데이터 전송
    - `close_stream()`: 스트림 종료
  - 이벤트 콜백 시스템:
    - `set_receive_callback()`: 기본 스트림 데이터
    - `set_stream_receive_callback()`: 모든 스트림 데이터
    - `set_connected_callback()`, `set_disconnected_callback()`
    - `set_error_callback()`: 에러 처리
  - `quic_client_config`를 통한 설정 옵션:
    - TLS 설정 (CA 인증서, 클라이언트 인증서, 키, 검증)
    - ALPN 프로토콜 협상
    - 전송 파라미터 (타임아웃, 흐름 제어 제한)
    - 0-RTT 조기 데이터 지원
  - `stats()` 메서드를 통한 연결 통계
  - API 사용법을 보여주는 예제 코드
  - 22개 테스트 케이스를 포함한 종합 테스트 스위트

- **QUIC 프로토콜 지원 (Phase 3.2)**: 연결 상태 머신 (RFC 9000 Section 5)
  - RFC 9000 Section 18 준수 인코딩/디코딩을 갖춘 `transport_parameters` 구조체
    - 모든 표준 전송 파라미터 (max_idle_timeout, 흐름 제어 제한 등)
    - 연결 ID 파라미터 (original/initial/retry)
    - 서버 전용 파라미터 (preferred_address, stateless_reset_token)
    - 클라이언트/서버 역할별 검증
  - 연결 상태 머신을 구현하는 `connection` 클래스
    - 연결 상태: idle, handshaking, connected, closing, draining, closed
    - 핸드셰이크 상태: initial, waiting_server_hello, waiting_finished, complete
    - 패킷 번호 공간: Initial, Handshake, Application
    - 롱 및 숏 헤더 패킷 처리
    - std::visit 패턴을 통한 프레임 처리
    - 연결 ID 관리 (추가, 폐기, 로테이션)
    - 전송 파라미터 협상
    - 에러 코드 보존을 통한 연결 종료
    - 유휴 타임아웃 처리
    - stream_manager 및 flow_controller와의 통합
  - 서버 주소 마이그레이션 지원을 위한 `preferred_address_info` 구조체
  - 40개 테스트 케이스를 포함한 종합 테스트 스위트

### 계획됨
- C++20 코루틴 완전 통합
- HTTP/2 및 HTTP/3 지원
- 고급 로드 밸런싱 알고리즘

---

## [1.4.0] - 2025-10-26

### 추가됨
- **TLS/SSL 지원 (Phase 9)**: 완전한 암호화 통신 구현
  - **secure_tcp_socket (Phase 9.1)**:
    - `asio::ssl::stream`을 사용한 TCP 소켓용 SSL/TLS 래퍼
    - 비동기 SSL 핸드셰이크 지원 (클라이언트/서버 모드)
    - 암호화된 비동기 읽기/쓰기 작업
    - 일관성을 위해 `tcp_socket`과 동일한 패턴 따름

  - **secure_session (Phase 9.1)**:
    - 서버측에서 단일 보안 클라이언트 세션 관리
    - 데이터 교환 전 서버측 SSL 핸드셰이크 수행
    - Phase 8.2의 백프레셔 메커니즘 상속 (1000/2000 메시지 제한)
    - 뮤텍스 보호를 통한 스레드 안전 메시지 큐

  - **secure_messaging_server (Phase 9.2)**:
    - TLS/SSL 암호화 연결을 수락하는 보안 서버
    - 파일에서 인증서 체인 및 개인키 로딩
    - SSL 컨텍스트 구성 (sslv23, no SSLv2, single DH use)
    - 수락된 각 연결에 대해 `secure_session` 생성
    - Phase 8.1의 세션 정리 상속 (주기적 + 요청 시)
    - common_system의 모니터링 지원 상속 (선택 사항)

  - **secure_messaging_client (Phase 9.3)**:
    - TLS/SSL 암호화 연결용 보안 클라이언트
    - 선택적 인증서 검증 (기본값: 활성화)
    - 검증 활성화 시 시스템 인증서 경로 사용
    - 10초 타임아웃을 갖춘 클라이언트측 SSL 핸드셰이크
    - 원자적 작업을 통한 연결 상태 관리

  - **빌드 시스템 (Phase 9.4)**:
    - `BUILD_TLS_SUPPORT` CMake 옵션 추가 (기본값: ON)
    - TLS/SSL 컴포넌트의 조건부 컴파일
    - 자동 OpenSSL 감지 및 링크
    - TLS/SSL 상태 표시를 위한 빌드 구성 요약 업데이트
    - 기존 WebSocket 지원과 호환 (공유 OpenSSL 의존성)

### 변경됨
- Issue #4 (TLS/SSL 지원 추가)를 완료로 표시하기 위해 IMPROVEMENTS_KO.md 업데이트
- 기본 라이브러리에서 TLS/SSL 소스를 분리하도록 CMakeLists.txt 재구성

### 기술 세부사항
- 암호화 작업에 OpenSSL 3.6.0 사용
- 병렬 클래스 계층 구조가 하위 호환성 유지
- SSL 컨텍스트 수명은 서버/클라이언트 인스턴스가 관리
- 핸드셰이크 실패 시 자세한 오류 코드와 함께 연결 거부
- 모든 보안 컴포넌트는 적절한 동기화로 스레드 안전

---

## [1.3.0] - 2025-10-26

### 추가됨
- **성능 최적화 (Phase 8)**: 메모리 관리 및 클라이언트 연결 효율성에 대한 중요 개선
  - **세션 정리 메커니즘 (Phase 8.1)**:
    - 상태 확인을 위한 `messaging_session`에 `is_stopped()` 메서드 추가
    - 벡터에서 중지된 세션을 제거하는 `cleanup_dead_sessions()` 추가
    - 30초마다 주기적 정리를 위한 `start_cleanup_timer()` 추가
    - 스레드 안전을 위해 `sessions_mutex_`로 `sessions_` 벡터 보호
    - 주기적으로 및 새 연결 시 정리 트리거
    - 장기 실행 서버에서 무제한 메모리 증가 방지

  - **수신자 백프레셔 (Phase 8.2)**:
    - 들어오는 메시지를 버퍼링하기 위한 `pending_messages_` 큐 (std::deque) 추가
    - 스레드 안전 큐 액세스를 위한 `queue_mutex_` 추가
    - `max_pending_messages_` 제한을 1000개 메시지로 설정
    - 큐가 제한에 도달하면 경고 로그 (백프레셔 신호)
    - 큐가 2배 제한 (2000개 메시지)을 초과하면 남용 클라이언트 연결 해제
    - 메시지를 디큐하고 처리하기 위한 `process_next_message()` 추가
    - 빠르게 전송하는 클라이언트로부터 메모리 소진 방지

  - **연결 풀링 (Phase 8.3)**:
    - 재사용 가능한 클라이언트 연결을 위한 `connection_pool` 클래스 추가
    - 초기화 시 고정된 수의 연결 미리 생성
    - 뮤텍스 및 조건 변수를 사용한 스레드 안전 획득/해제 의미
    - 모든 연결이 사용 중일 때 사용 가능해질 때까지 차단
    - 해제 시 손실된 연결 자동 재연결
    - 모니터링을 위한 활성 연결 수 추적
    - 구성 가능한 풀 크기 (기본값: 10개 연결)
    - 최대 60%까지 연결 오버헤드 감소

### 변경됨
- 뮤텍스로 보호되는 세션을 반영하도록 `messaging_server` 스레드 안전 문서 업데이트
- Issue #1, #2, #3을 완료로 표시하기 위해 IMPROVEMENTS_KO.md 업데이트

### 기술 세부사항
- 세션 정리는 원자적 작업과 함께 `is_stopped()` 확인 사용
- 백프레셔는 1000개 메시지에서 적용되고, 2000개에서 연결 해제
- 연결 풀은 안전한 리소스 관리를 위해 RAII 사용
- 모든 성능 기능은 선택 사항이며 하위 호환

---

## [1.2.0] - 2025-10-26

### 추가됨
- **네트워크 부하 테스트 프레임워크**: 실제 네트워크 성능 측정을 위한 포괄적인 인프라
  - **메모리 프로파일러**: 크로스 플랫폼 메모리 사용량 추적
    - `MemoryProfiler`: Linux, macOS 및 Windows용 RSS/힙/VM 측정
    - /proc, task_info() 및 GetProcessMemoryInfo()를 사용한 플랫폼별 구현
    - 전후 비교를 위한 메모리 델타 계산

  - **결과 작성기**: 성능 메트릭 직렬화
    - 테스트 결과를 위한 JSON 및 CSV 출력 형식
    - ISO 8601 타임스탬프 생성
    - 지연 시간 (P50/P95/P99), 처리량 및 메모리 메트릭을 위한 구조화된 데이터

  - **TCP 부하 테스트**: 엔드투엔드 TCP 성능 테스트
    - 64B, 1KB 및 64KB 메시지에 대한 처리량 테스트
    - 동시 연결 테스트 (10 및 50 클라이언트)
    - 왕복 지연 시간 측정
    - 메모리 소비 추적

  - **UDP 부하 테스트**: UDP 프로토콜 성능 검증
    - 64B, 512B 및 1KB 데이터그램에 대한 처리량 테스트
    - 패킷 손실 감지 및 성공률 추적
    - 버스트 전송 성능 (버스트당 100개 메시지)
    - 동시 클라이언트 테스트 (10 및 50 클라이언트)

  - **WebSocket 부하 테스트**: WebSocket 프로토콜 벤치마킹
    - 텍스트 및 바이너리 메시지 처리량 테스트
    - 핑/퐁 지연 시간 측정
    - 동시 연결 처리 (10 클라이언트)
    - 프레임 오버헤드 분석

- **CI/CD 자동화**:
  - **GitHub Actions 워크플로우**: 자동화된 부하 테스트 파이프라인
    - 주간 예약 실행 (일요일 00:00 UTC)
    - 베이스라인 업데이트 옵션과 함께 수동 트리거
    - 다중 플랫폼 지원 (Ubuntu 22.04, macOS 13, Windows 2022)
    - 90일 보존 기간과 함께 자동 아티팩트 업로드

  - **메트릭 수집 스크립트**: Python 기반 결과 집계
    - `collect_metrics.py`: 모든 프로토콜의 JSON 결과 구문 분석 및 결합
    - 요약 통계 생성
    - 플랫폼 및 컴파일러 메타데이터 추적

  - **베이스라인 업데이트 스크립트**: 자동화된 문서 업데이트
    - `update_baseline.py`: 실제 네트워크 메트릭을 BASELINE_KO.md에 삽입
    - 플랫폼별 결과 추적
    - 미리보기를 위한 드라이런 모드
    - 베이스라인 변경을 위한 자동 PR 생성

- **문서**:
  - **부하 테스트 가이드**: 포괄적인 400+ 줄 테스트 문서
    - 테스트 구조 및 방법론
    - 결과 해석 가이드
    - 프로토콜별 성능 기대치
    - 문제 해결 섹션
    - 고급 프로파일링 기술

  - **베이스라인 업데이트**: 실제 네트워크 성능 섹션
    - BASELINE_KO.md에 "실제 네트워크 성능 메트릭" 추가
    - 부하 테스트 인프라 문서
    - 방법론 및 예상 메트릭 섹션

  - **README 업데이트**: 성능 및 테스트 섹션
    - 합성 벤치마크 섹션
    - 실제 네트워크 부하 테스트 섹션
    - 자동화된 베이스라인 수집 지침
    - 부하 테스트 가이드와 함께 문서 테이블 업데이트

### 변경됨
- 완료된 부하 테스트 인프라를 반영하도록 BASELINE_KO.md의 Phase 0 체크리스트 업데이트
- 프레임워크 라이브러리로 통합 테스트 CMakeLists.txt 재구성
- 테스트 스위트 수를 4에서 7로 업데이트 (TCP, UDP, WebSocket 부하 테스트 추가)

### 기술 세부사항
- 모든 부하 테스트는 일관된 측정을 위해 localhost 루프백 사용
- 타이밍 관련 불안정성을 피하기 위해 CI 환경에서 테스트 자동 건너뛰기
- 메모리 프로파일링은 정확한 측정을 위해 플랫폼 네이티브 API 사용
- 결과에는 자세한 분석을 위한 P50/P95/P99 지연 시간 백분위수 포함

---

## [1.1.0] - 2025-10-26

### 추가됨
- **WebSocket 프로토콜 지원 (RFC 6455)**: 완전한 기능을 갖춘 WebSocket 클라이언트 및 서버 구현
  - **messaging_ws_client**: 고수준 WebSocket 클라이언트 API
    - 텍스트 및 바이너리 메시지 지원
    - 구성 가능한 연결 매개변수 (호스트, 포트, 경로, 헤더)
    - 자동 핑/퐁 킵얼라이브 메커니즘
    - 우아한 닫기 핸드셰이크
    - 연결, 연결 해제, 메시지 및 오류에 대한 이벤트 콜백

  - **messaging_ws_server**: 고수준 WebSocket 서버 API
    - 멀티 클라이언트 연결 관리
    - 연결된 모든 클라이언트에 대한 브로드캐스트 지원
    - 연결별 메시지 처리
    - 연결 제한을 갖춘 세션 관리
    - 새 연결, 연결 해제 및 메시지에 대한 이벤트 콜백

  - **websocket_socket**: 저수준 WebSocket 프레이밍 계층
    - 기존 tcp_socket 인프라 위에 구축
    - WebSocket 핸드셰이크 (HTTP/1.1 업그레이드)
    - 마스킹 지원을 통한 프레임 인코딩/디코딩
    - 메시지 단편화 및 재조립
    - 제어 프레임 처리 (핑, 퐁, 닫기)

  - **websocket_protocol**: 프로토콜 상태 머신
    - RFC 6455 준수 프레임 처리
    - 텍스트 메시지에 대한 UTF-8 검증
    - 핑 프레임에 대한 자동 퐁 응답
    - 우아한 종료를 갖춘 닫기 코드 처리

  - **ws_session_manager**: WebSocket 연결 수명 주기 관리
    - shared_mutex를 사용한 스레드 안전 연결 추적
    - 연결 제한 시행
    - 임계값 초과 시 백프레셔 신호
    - 자동 연결 ID 생성
    - 연결 메트릭 (총 수락, 총 거부, 활성 개수)

- **문서**:
  - WebSocket 기능 및 사용 예제로 README_KO.md 업데이트
  - WebSocket 구성 요소를 포함하도록 아키텍처 다이어그램 업데이트
  - TCP 및 WebSocket 프로토콜 모두에 대한 API 예제

### 변경됨
- WebSocket 지원을 "계획된 기능"에서 "핵심 기능"으로 이동
- WebSocket 계층 통합을 표시하도록 아키텍처 다이어그램 업데이트

### 보안
- 클라이언트-서버 통신을 위한 프레임 마스킹 (RFC 6455 요구 사항)
- 잘못된 데이터를 방지하기 위한 텍스트 프레임에 대한 UTF-8 검증
- 리소스 소진을 방지하기 위한 ws_session_manager를 통한 연결 제한

---

## [1.0.0] - 2025-10-22

### 추가됨
- **핵심 Network System**: 프로덕션 준비 완료된 C++20 비동기 네트워크 라이브러리
  - ASIO 기반 논블로킹 I/O 작업
  - 최대 효율성을 위한 제로카피 파이프라인
  - 지능형 라이프사이클 관리를 통한 커넥션 풀링
  - 비동기 작업을 위한 C++20 코루틴 지원

- **고성능**: 초고속 처리량 네트워킹
  - 평균 305K+ 메시지/초 처리량
  - 작은 메시지(<1KB) 769K+ msg/s
  - 마이크로초 미만 지연시간
  - 제로카피 전송을 위한 직접 메모리 매핑

- **핵심 컴포넌트**:
  - **MessagingClient**: 커넥션 풀링을 갖춘 고성능 클라이언트
  - **MessagingServer**: 세션 관리를 갖춘 확장 가능한 서버
  - **MessagingSession**: 커넥션별 세션 처리
  - **Pipeline**: 제로카피 버퍼 관리 시스템

- **모듈형 아키텍처**:
  - messaging_system으로부터 독립 (100% 하위 호환성)
  - container_system과의 선택적 통합
  - thread_system과의 선택적 통합
  - logger_system과의 선택적 통합
  - common_system과의 선택적 통합

- **통합 지원**:
  - container_system: 바이너리 프로토콜을 통한 타입 안전 데이터 전송
  - thread_system: 동시 작업을 위한 스레드 풀 관리
  - logger_system: 네트워크 작업 로깅 및 진단
  - common_system: Result<T> 오류 처리 패턴
  - messaging_system: 메시징 전송 계층 (브리지 모드)

- **고급 기능**:
  - 지수 백오프를 통한 자동 재연결
  - 커넥션 타임아웃 관리
  - 우아한 종료 처리
  - 세션 라이프사이클 관리
  - 오류 복구 패턴

- **빌드 시스템**:
  - 모듈형 구성을 갖춘 CMake 3.16+
  - 유연한 빌드를 위한 13개 이상의 기능 플래그
  - 크로스 플랫폼 지원 (Windows, Linux, macOS)
  - 컴파일러 지원 (GCC 10+, Clang 10+, MSVC 19.29+)

### 변경됨
- 해당 없음 (최초 독립 릴리스)

### 사용 중단됨
- 해당 없음 (최초 독립 릴리스)

### 제거됨
- 해당 없음 (최초 독립 릴리스)

### 수정됨
- 해당 없음 (최초 독립 릴리스)

### 보안
- TLS 지원을 통한 안전한 커넥션 처리
- 모든 네트워크 작업에 대한 입력 유효성 검사
- 버퍼 오버플로우 보호
- DoS 방지를 위한 커넥션 제한

---

## [0.9.0] - 2025-09-15 (messaging_system으로부터 분리 이전)

### 추가됨
- messaging_system의 일부로서의 초기 네트워크 기능
- 기본 클라이언트/서버 통신
- 메시지 직렬화 및 역직렬화
- 커넥션 관리

### 변경됨
- messaging_system과의 긴밀한 결합
- 제한된 독립적 사용

---

## 프로젝트 정보

**저장소:** https://github.com/kcenon/network_system
**문서:** [docs/](docs/)
**라이선스:** LICENSE 파일 참조
**관리자:** kcenon@naver.com

---

## 버전 지원 매트릭스

| 버전  | 릴리스 날짜 | C++ 표준 | CMake 최소 버전 | 상태   |
|-------|-------------|----------|----------------|--------|
| 1.4.0 | 2025-10-26  | C++20    | 3.16           | 현재   |
| 1.3.0 | 2025-10-26  | C++20    | 3.16           | 안정   |
| 1.2.0 | 2025-10-26  | C++20    | 3.16           | 안정   |
| 1.1.0 | 2025-10-26  | C++20    | 3.16           | 안정   |
| 1.0.0 | 2025-10-22  | C++20    | 3.16           | 안정   |
| 0.9.0 | 2025-09-15  | C++20    | 3.16           | 레거시 (messaging_system의 일부) |

---

## 마이그레이션 가이드

messaging_system 통합 네트워크 모듈에서 마이그레이션하려면 [MIGRATION.md](MIGRATION.md)를 참조하십시오.

---

## 성능 벤치마크

**플랫폼**: 시스템에 따라 다름 (자세한 내용은 benchmarks/ 참조)

### 처리량 벤치마크
- 작은 메시지 (<1KB): 769K+ msg/s
- 중간 메시지 (1-10KB): 305K+ msg/s
- 큰 메시지 (>10KB): 150K+ msg/s

### 지연시간 벤치마크
- 로컬 커넥션: < 1 마이크로초
- 네트워크 커넥션: 네트워크 지연시간 + 처리 오버헤드에 따라 다름

### 확장성
- 동시 커넥션: 10,000개 이상의 동시 커넥션
- 8코어까지 선형 확장
- 커넥션 풀링으로 오버헤드 60% 감소

---

## 호환성을 깨는 변경사항

### 1.0.0
- **네임스페이스 변경**: `messaging::network` → `network_system`
- **헤더 경로**: `<messaging/network/*>` → `<network_system/*>`
- **CMake 타겟**: `messaging_system` → `network_system`
- **빌드 옵션**: 생태계 통합을 위한 새로운 `BUILD_WITH_*` 옵션

자세한 마이그레이션 지침은 [MIGRATION.md](MIGRATION.md)를 참조하십시오.

---

**최종 업데이트:** 2025-10-22
