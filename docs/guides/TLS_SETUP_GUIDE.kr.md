---
doc_id: "NET-SECU-001"
doc_title: "network_system을 위한 TLS/SSL 설정 가이드"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "SECU"
---

# network_system을 위한 TLS/SSL 설정 가이드

> **SSOT**: This document is the single source of truth for **network_system을 위한 TLS/SSL 설정 가이드**.

> **Language:** [English](TLS_SETUP_GUIDE.md) | **한국어**

## 목차

- [개요](#개요)
- [⚠️ 현재 상태](#-현재-상태)
- [구성 구조](#구성-구조)
  - [기본 TLS 구성](#기본-tls-구성)
  - [클라이언트 구성](#클라이언트-구성)
- [보안 모범 사례](#보안-모범-사례)
  - [✅ 권장 사항](#-권장-사항)
  - [❌ 금지 사항](#-금지-사항)
- [인증서 관리](#인증서-관리)
  - [자체 서명 인증서 생성 (개발)](#자체-서명-인증서-생성-개발)
  - [프로덕션 인증서](#프로덕션-인증서)
- [구성 예제](#구성-예제)
  - [고보안 서버](#고보안-서버)
  - [상호 TLS (mTLS)를 사용한 클라이언트](#상호-tls-mtls를-사용한-클라이언트)
- [문제 해결](#문제-해결)
  - [인증서 검증 실패](#인증서-검증-실패)
  - [핸드셰이크 타임아웃](#핸드셰이크-타임아웃)
  - [암호 스위트 불일치](#암호-스위트-불일치)
- [구현 로드맵](#구현-로드맵)
  - [✅ Phase 1: 인프라 (완료)](#-phase-1-인프라-완료)
  - [🔄 Phase 2: 핵심 통합 (진행 중)](#-phase-2-핵심-통합-진행-중)
  - [📋 Phase 3: 고급 기능 (계획됨)](#-phase-3-고급-기능-계획됨)
- [ASIO SSL과의 통합](#asio-ssl과의-통합)
  - [향후 API (미리보기)](#향후-api-미리보기)
- [보안 감사](#보안-감사)
  - [권장 도구](#권장-도구)
- [규정 준수](#규정-준수)
- [추가 읽기 자료](#추가-읽기-자료)
- [지원](#지원)

## 개요

이 가이드는 network_system 라이브러리에서 TLS/SSL 암호화를 구성하고 사용하는 방법에 대한 지침을 제공합니다. TLS (Transport Layer Security)는 프로덕션 환경에서 네트워크 통신을 보안하는 데 필수적입니다.

## ⚠️ 현재 상태

**TLS 인프라: 준비 완료**
**전체 구현: 진행 중**

TLS 구성 구조 및 보안 가이드라인이 마련되었습니다. ASIO SSL 소켓과의 완전한 통합은 향후 릴리스에서 계획되어 있습니다.

## 구성 구조

### 기본 TLS 구성

```cpp
#include "network_system/internal/common_defs.h"

using namespace network_system::internal;

// TLS가 포함된 서버 구성
tls_config server_tls = tls_config::secure_defaults();
server_tls.certificate_file = "/path/to/server.crt";
server_tls.private_key_file = "/path/to/server.key";
server_tls.ca_file = "/path/to/ca.crt";

// 구성 검증
if (!server_tls.is_valid()) {
    throw std::runtime_error("Invalid TLS configuration");
}
```

### 클라이언트 구성

```cpp
// TLS가 포함된 클라이언트 구성
tls_config client_tls = tls_config::secure_defaults();
client_tls.ca_file = "/path/to/ca.crt";
client_tls.sni_hostname = "example.com";

// 테스트 전용 (안전하지 않음!)
// auto client_tls = tls_config::insecure_for_testing();
```

## 보안 모범 사례

### ✅ 권장 사항

1. **TLS 1.2 또는 1.3 사용** - 항상 `min_version`을 `tls_version::tls_1_2` 이상으로 설정
2. **피어 인증서 검증** - 프로덕션에서 `certificate_verification::verify_peer` 사용
3. **개인 키 보호** - 파일 권한을 600으로 설정 (소유자만 읽기/쓰기)
4. **강력한 암호 사용** - AES-GCM 또는 ChaCha20-Poly1305와 함께 ECDHE 선호
5. **SNI 활성화** - 가상 호스팅을 지원하기 위해 클라이언트에 `sni_hostname` 설정
6. **인증서 교체** - 만료 전에 인증서 교체

### ❌ 금지 사항

1. **검증을 비활성화하지 마세요** - `certificate_verification::none`은 테스트 전용
2. **TLS 1.0/1.1 사용하지 마세요** - 이 버전은 알려진 취약점이 있음
3. **암호를 하드코딩하지 마세요** - 환경 변수 또는 보안 키 저장소 사용
4. **개인 키를 커밋하지 마세요** - `.gitignore`에 `*.key` 및 `*.pem` 추가
5. **검증을 건너뛰지 마세요** - 구성 사용 전 항상 `is_valid()` 호출

## 인증서 관리

### 자체 서명 인증서 생성 (개발)

```bash
# CA 키 및 인증서 생성
openssl genrsa -out ca.key 4096
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
    -subj "/CN=Test CA/O=Development"

# 서버 키 및 CSR 생성
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
    -subj "/CN=localhost/O=Development"

# CA로 서버 인증서 서명
openssl x509 -req -days 365 -in server.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out server.crt

# 적절한 권한 설정
chmod 600 server.key ca.key
chmod 644 server.crt ca.crt
```

### 프로덕션 인증서

프로덕션의 경우, 신뢰할 수 있는 인증 기관(CA)의 인증서를 사용하세요:
- Let's Encrypt (무료, 자동화)
- DigiCert
- GlobalSign
- 조직 내부 CA

## 구성 예제

### 고보안 서버

```cpp
tls_config secure_server;
secure_server.enabled = true;
secure_server.min_version = tls_version::tls_1_3;
secure_server.certificate_file = "/etc/ssl/certs/server.crt";
secure_server.private_key_file = "/etc/ssl/private/server.key";
secure_server.ca_file = "/etc/ssl/certs/ca-bundle.crt";
secure_server.verify_mode = certificate_verification::verify_fail_if_no_peer_cert;
secure_server.cipher_list = "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-CHACHA20-POLY1305";
secure_server.enable_session_resumption = true;
secure_server.handshake_timeout_ms = 5000;
```

### 상호 TLS (mTLS)를 사용한 클라이언트

```cpp
tls_config mtls_client;
mtls_client.enabled = true;
mtls_client.min_version = tls_version::tls_1_2;
mtls_client.certificate_file = "/path/to/client.crt";
mtls_client.private_key_file = "/path/to/client.key";
mtls_client.ca_file = "/path/to/ca.crt";
mtls_client.verify_mode = certificate_verification::verify_peer;
mtls_client.sni_hostname = "api.example.com";
```

## 문제 해결

### 인증서 검증 실패

**문제**: 인증서 검증 오류로 연결 실패

**해결책**:
1. CA 인증서가 올바르고 신뢰할 수 있는지 확인
2. 인증서 체인이 완전한지 확인
3. 서버 인증서 CN/SAN이 호스트 이름과 일치하는지 확인
4. 인증서 만료 날짜 확인

```bash
# 인증서 검증
openssl x509 -in server.crt -text -noout

# 인증서 체인 확인
openssl verify -CAfile ca.crt server.crt
```

### 핸드셰이크 타임아웃

**문제**: TLS 핸드셰이크가 너무 오래 걸리거나 타임아웃

**해결책**:
1. `handshake_timeout_ms` 증가
2. 네트워크 지연 확인
3. 서버가 응답하는지 확인
4. 방화벽 규칙 확인

### 암호 스위트 불일치

**문제**: 클라이언트와 서버 간 공통 암호 스위트 없음

**해결책**:
1. 호환 가능한 암호를 포함하도록 암호 목록 업데이트
2. OpenSSL 버전 호환성 확인
3. 더 많은 암호 스위트 활성화 (신중하게)

## 구현 로드맵

### ✅ Phase 1: 인프라 (완료)
- TLS 구성 구조
- 보안 열거형 및 상수
- 검증 로직
- 문서화

### 🔄 Phase 2: 핵심 통합 (진행 중)
- ASIO SSL 컨텍스트 통합
- SSL 소켓 래퍼
- 인증서 로딩
- 핸드셰이크 처리

### 📋 Phase 3: 고급 기능 (계획됨)
- 세션 재개
- OCSP 스테이플링
- 인증서 고정
- SNI 콜백

## ASIO SSL과의 통합

### 향후 API (미리보기)

```cpp
// 이 API는 향후 구현을 위해 계획됨
#include <asio/ssl.hpp>

// 서버 예제 (향후)
asio::ssl::context ssl_ctx(asio::ssl::context::tlsv13);
ssl_ctx.use_certificate_file(tls_cfg.certificate_file.value(),
                             asio::ssl::context::pem);
ssl_ctx.use_private_key_file(tls_cfg.private_key_file.value(),
                            asio::ssl::context::pem);

// SSL 소켓 생성
asio::ssl::stream<asio::ip::tcp::socket> ssl_socket(io_context, ssl_ctx);

// 핸드셰이크 수행
ssl_socket.async_handshake(asio::ssl::stream_base::server,
    [](const asio::error_code& ec) {
        if (!ec) {
            // 핸드셰이크 성공
        }
    });
```

## 보안 감사

### 권장 도구

1. **testssl.sh** - 포괄적인 TLS/SSL 스캐너
   ```bash
   ./testssl.sh --protocols --ciphers --server-defaults localhost:8443
   ```

2. **OpenSSL s_client** - 수동 테스트
   ```bash
   openssl s_client -connect localhost:8443 -tls1_2
   ```

3. **Nmap SSL Scripts** - 자동화된 스캐닝
   ```bash
   nmap --script ssl-enum-ciphers -p 8443 localhost
   ```

## 규정 준수

이 TLS 구성은 다음을 지원합니다:
- ✅ PCI DSS 3.2+ (TLS 1.2+)
- ✅ HIPAA Security Rule
- ✅ GDPR Article 32
- ✅ NIST SP 800-52 Rev. 2

## 추가 읽기 자료

- [OWASP TLS Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Transport_Layer_Protection_Cheat_Sheet.html)
- [Mozilla SSL Configuration Generator](https://ssl-config.mozilla.org/)
- [RFC 8446 - TLS 1.3](https://tools.ietf.org/html/rfc8446)
- [ASIO SSL Documentation](https://think-async.com/Asio/asio-1.18.0/doc/asio/overview/ssl.html)

## 지원

TLS 구성에 관한 질문이나 문제:
1. 먼저 이 문서 확인
2. ASIO SSL 예제 검토
3. OpenSSL 문서 참조
4. 프로젝트 저장소에 이슈 제출

---

**마지막 업데이트**: 2025-10-19
**상태**: 인프라 완료, 전체 통합 대기 중

---

*Last Updated: 2025-10-20*
