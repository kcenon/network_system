# 운영 가이드

> **Language:** [English](OPERATIONS.md) | **한국어**

이 가이드는 Network System의 배포, 구성, 모니터링 및 운영 모범 사례를 다룹니다.

## 목차

- [배포 모범 사례](#배포-모범-사례)
- [구성 관리](#구성-관리)
- [모니터링 및 메트릭](#모니터링-및-메트릭)
- [성능 튜닝](#성능-튜닝)
- [백업 및 복구](#백업-및-복구)
- [보안 고려사항](#보안-고려사항)
- [확장 전략](#확장-전략)

## 배포 모범 사례

### 배포 전 체크리스트

1. **환경 검증**
   - 대상 플랫폼 호환성 확인
   - 필수 종속성 설치 확인
   - 충분한 시스템 리소스 확인
   - 네트워크 연결 검증

2. **빌드 구성**
   ```bash
   # 최적화된 프로덕션 빌드
   cmake -DCMAKE_BUILD_TYPE=Release \
         -DENABLE_TESTING=OFF \
         -DENABLE_PROFILING=OFF \
         -B build

   cmake --build build --config Release --parallel
   ```

3. **배포 단계**
   - 기존 서비스 정상 종료
   - 현재 구성 백업
   - 새 바이너리 배포
   - 구성 파일 업데이트
   - 모니터링과 함께 서비스 시작
   - 서비스 상태 확인

### 플랫폼별 배포

#### Linux 배포
```bash
# 서비스 사용자 생성
sudo useradd -r -s /bin/false network_service

# systemd 서비스 설치
sudo cp network_service.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable network_service
sudo systemctl start network_service
```

#### Docker 배포
```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    libssl-dev \
    libcurl4-openssl-dev
COPY build/network_server /usr/local/bin/
EXPOSE 8080
CMD ["/usr/local/bin/network_server"]
```

#### Windows 서비스
```powershell
# Windows 서비스로 설치
sc create NetworkService binPath= "C:\Program Files\NetworkSystem\network_server.exe"
sc config NetworkService start= auto
sc start NetworkService
```

## 구성 관리

### 구성 구조

```yaml
# config.yaml
server:
  host: 0.0.0.0
  port: 8080
  max_connections: 1000
  worker_threads: 4

security:
  ssl_enabled: true
  cert_file: /etc/network/server.crt
  key_file: /etc/network/server.key
  allowed_origins:
    - https://trusted.domain.com

performance:
  connection_pool_size: 100
  request_timeout_ms: 30000
  keep_alive_timeout_ms: 60000
  max_request_size: 10485760  # 10MB

logging:
  level: INFO
  file: /var/log/network/server.log
  max_size_mb: 100
  max_files: 10
  format: json
```

### 환경 변수

```bash
# 환경 변수를 통한 구성 재정의
export NETWORK_SERVER_HOST=0.0.0.0
export NETWORK_SERVER_PORT=8080
export NETWORK_LOG_LEVEL=DEBUG
export NETWORK_SSL_ENABLED=true
export NETWORK_MAX_CONNECTIONS=5000
```

### 구성 검증

```cpp
// 시작 시 구성 검증
bool validate_config(const Config& config) {
    if (config.port < 1 || config.port > 65535) {
        return false;
    }
    if (config.max_connections < 1) {
        return false;
    }
    if (config.worker_threads < 1) {
        return false;
    }
    return true;
}
```

## 모니터링 및 메트릭

### 모니터링할 주요 메트릭

1. **시스템 메트릭**
   - 코어당 CPU 사용률
   - 메모리 사용량 (RSS, heap, stack)
   - 네트워크 I/O (바이트 입출력, 패킷)
   - 디스크 I/O (로깅/캐싱 시)
   - 파일 디스크립터 사용량

2. **애플리케이션 메트릭**
   - 활성 연결 수
   - 요청 속도 (req/sec)
   - 응답 시간 (p50, p95, p99)
   - 유형별 오류율
   - 연결 풀 사용률
   - 메시지 큐 깊이

3. **비즈니스 메트릭**
   - 트랜잭션 성공률
   - 데이터 전송량
   - 클라이언트 세션 지속 시간
   - 프로토콜별 메트릭

### Prometheus 통합

```cpp
// 메트릭 엔드포인트 노출
class MetricsHandler {
public:
    std::string get_metrics() {
        std::stringstream ss;
        ss << "# HELP network_connections_active Active connection count\n";
        ss << "# TYPE network_connections_active gauge\n";
        ss << "network_connections_active " << active_connections_ << "\n";

        ss << "# HELP network_requests_total Total request count\n";
        ss << "# TYPE network_requests_total counter\n";
        ss << "network_requests_total " << total_requests_ << "\n";

        ss << "# HELP network_response_time_seconds Response time histogram\n";
        ss << "# TYPE network_response_time_seconds histogram\n";
        ss << response_time_histogram_.to_prometheus();

        return ss.str();
    }
};
```

### 로깅 구성

```yaml
# 구조화된 로깅 구성
logging:
  outputs:
    - type: console
      level: INFO
      format: json
    - type: file
      level: DEBUG
      path: /var/log/network/server.log
      rotation:
        max_size: 100MB
        max_age: 30d
        max_backups: 10
    - type: syslog
      level: ERROR
      facility: local0
```

### 상태 확인

```cpp
// 상태 확인 엔드포인트 구현
class HealthCheck {
public:
    struct Status {
        bool healthy;
        std::string status;
        std::map<std::string, bool> checks;
    };

    Status check() {
        Status status;
        status.checks["database"] = check_database();
        status.checks["memory"] = check_memory();
        status.checks["disk"] = check_disk_space();
        status.checks["network"] = check_network();

        status.healthy = std::all_of(
            status.checks.begin(),
            status.checks.end(),
            [](const auto& check) { return check.second; }
        );

        status.status = status.healthy ? "healthy" : "unhealthy";
        return status;
    }
};
```

## 성능 튜닝

### 시스템 수준 최적화

#### Linux 커널 매개변수
```bash
# /etc/sysctl.conf
# 네트워크 스택 튜닝
net.core.somaxconn = 65535
net.core.netdev_max_backlog = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.ipv4.tcp_fin_timeout = 30
net.ipv4.tcp_keepalive_time = 300
net.ipv4.tcp_tw_reuse = 1
net.ipv4.ip_local_port_range = 10000 65000

# 파일 디스크립터 제한
fs.file-max = 2097152
fs.nr_open = 2097152

# 메모리 튜닝
vm.max_map_count = 262144
vm.swappiness = 10
```

#### 리소스 제한
```bash
# /etc/security/limits.conf
network_service soft nofile 1000000
network_service hard nofile 1000000
network_service soft nproc 32768
network_service hard nproc 32768
```

### 애플리케이션 수준 최적화

1. **연결 풀링**
```cpp
class ConnectionPool {
    size_t min_size = 10;
    size_t max_size = 100;
    std::chrono::seconds idle_timeout{60};
    std::chrono::seconds connection_timeout{5};
    bool validate_on_checkout = true;
};
```

2. **스레드 풀 튜닝**
```cpp
// 최적 스레드 수 계산
size_t calculate_thread_count() {
    size_t hw_threads = std::thread::hardware_concurrency();
    size_t min_threads = 4;
    size_t max_threads = 64;

    // I/O 바운드: 2 * CPU 코어
    // CPU 바운드: CPU 코어
    size_t optimal = hw_threads * 2;

    return std::clamp(optimal, min_threads, max_threads);
}
```

3. **메모리 관리**
```cpp
// 빈번한 할당을 위한 메모리 풀 사용
template<typename T>
class ObjectPool {
    std::queue<std::unique_ptr<T>> pool_;
    std::mutex mutex_;
    size_t max_size_ = 1000;

public:
    std::unique_ptr<T> acquire() {
        std::lock_guard lock(mutex_);
        if (!pool_.empty()) {
            auto obj = std::move(pool_.front());
            pool_.pop();
            return obj;
        }
        return std::make_unique<T>();
    }

    void release(std::unique_ptr<T> obj) {
        std::lock_guard lock(mutex_);
        if (pool_.size() < max_size_) {
            obj->reset();
            pool_.push(std::move(obj));
        }
    }
};
```

### 프로파일링 및 벤치마킹

```bash
# perf를 사용한 CPU 프로파일링
perf record -g ./network_server
perf report

# valgrind를 사용한 메모리 프로파일링
valgrind --leak-check=full --track-origins=yes ./network_server

# 네트워크 성능 테스트
iperf3 -c server_host -p 8080 -t 60

# 부하 테스트
wrk -t12 -c400 -d30s --latency http://localhost:8080/
```

## 백업 및 복구

### 백업 전략

1. **구성 백업**
```bash
#!/bin/bash
# backup_config.sh
BACKUP_DIR="/var/backups/network_system"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p "$BACKUP_DIR"
tar -czf "$BACKUP_DIR/config_$DATE.tar.gz" \
    /etc/network_system/ \
    /var/lib/network_system/

# 최근 30일 백업 유지
find "$BACKUP_DIR" -name "config_*.tar.gz" -mtime +30 -delete
```

2. **상태 백업**
```cpp
class StateManager {
public:
    void save_checkpoint(const std::string& path) {
        std::ofstream file(path, std::ios::binary);
        serialize_state(file);
        file.close();

        // 일관성을 위한 원자적 이름 변경
        std::filesystem::rename(
            path + ".tmp",
            path
        );
    }

    void restore_checkpoint(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        deserialize_state(file);
    }
};
```

### 재해 복구

1. **복구 시간 목표 (RTO)**
   - Cold standby: < 4시간
   - Warm standby: < 30분
   - Hot standby: < 5분

2. **복구 시점 목표 (RPO)**
   - 구성: 일일 백업
   - 상태 데이터: 시간별 스냅샷
   - 트랜잭션 로그: 실시간 복제

3. **장애 조치 절차**
```bash
#!/bin/bash
# failover.sh
PRIMARY_HOST="primary.example.com"
SECONDARY_HOST="secondary.example.com"

# 주 서버 상태 확인
if ! curl -f http://$PRIMARY_HOST:8080/health; then
    echo "Primary is down, initiating failover"

    # DNS 또는 로드 밸런서 업데이트
    update_dns_record $SECONDARY_HOST

    # 보조 서버 승격
    ssh $SECONDARY_HOST "systemctl start network_service_primary"

    # 운영 팀에 알림
    send_alert "Failover completed to $SECONDARY_HOST"
fi
```

## 보안 고려사항

### 네트워크 보안

1. **TLS 구성**
```cpp
SSL_CTX* create_ssl_context() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());

    // TLS 1.2 이상 사용
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    // 강력한 암호화 제품군
    SSL_CTX_set_cipher_list(ctx,
        "ECDHE+AESGCM:ECDHE+AES256:!aNULL:!MD5:!DSS");

    // OCSP 스테이플링 활성화
    SSL_CTX_set_tlsext_status_type(ctx, TLSEXT_STATUSTYPE_ocsp);

    return ctx;
}
```

2. **방화벽 규칙**
```bash
# iptables 구성
iptables -A INPUT -p tcp --dport 8080 -m conntrack --ctstate NEW,ESTABLISHED -j ACCEPT
iptables -A INPUT -p tcp --dport 8443 -m conntrack --ctstate NEW,ESTABLISHED -j ACCEPT
iptables -A INPUT -p tcp --dport 9090 -s 10.0.0.0/8 -j ACCEPT  # 내부에서 메트릭
iptables -A INPUT -j DROP
```

### 접근 제어

1. **인증**
```cpp
class Authenticator {
    bool validate_token(const std::string& token) {
        // JWT 토큰 검증
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret_})
            .with_issuer("network_system")
            .with_claim("exp", jwt::claim(std::chrono::system_clock::now()));

        verifier.verify(decoded);
        return true;
    }
};
```

2. **속도 제한**
```cpp
class RateLimiter {
    std::unordered_map<std::string, TokenBucket> buckets_;

public:
    bool allow_request(const std::string& client_id) {
        auto& bucket = buckets_[client_id];
        return bucket.consume(1);
    }
};
```

### 보안 감사

```cpp
class AuditLogger {
    void log_security_event(const SecurityEvent& event) {
        json log_entry = {
            {"timestamp", std::chrono::system_clock::now()},
            {"event_type", event.type},
            {"client_ip", event.client_ip},
            {"user_id", event.user_id},
            {"action", event.action},
            {"result", event.result},
            {"details", event.details}
        };

        security_log_ << log_entry.dump() << std::endl;
    }
};
```

## 확장 전략

### 수직 확장

1. **리소스 할당**
   - 계산 집약적 워크로드를 위한 CPU 코어 증가
   - 캐싱 및 버퍼링을 위한 메모리 추가
   - I/O 작업을 위한 더 빠른 스토리지 사용 (NVMe SSD)
   - 네트워크 인터페이스 업그레이드 (10Gbps, 25Gbps)

2. **구성 조정**
```yaml
# 확장 구성
server:
  worker_threads: 32  # 4에서 증가
  max_connections: 10000  # 1000에서 증가

performance:
  connection_pool_size: 1000  # 100에서 증가
  buffer_size: 1048576  # 1MB 버퍼
```

### 수평 확장

1. **로드 밸런싱**
```nginx
# nginx 로드 밸런서 구성
upstream network_backend {
    least_conn;
    server backend1.example.com:8080 weight=1;
    server backend2.example.com:8080 weight=1;
    server backend3.example.com:8080 weight=1;

    keepalive 32;
}

server {
    listen 80;
    location / {
        proxy_pass http://network_backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
    }
}
```

2. **서비스 메시 아키텍처**
```yaml
# Kubernetes 배포
apiVersion: apps/v1
kind: Deployment
metadata:
  name: network-service
spec:
  replicas: 3
  selector:
    matchLabels:
      app: network-service
  template:
    metadata:
      labels:
        app: network-service
    spec:
      containers:
      - name: network-server
        image: network-system:latest
        ports:
        - containerPort: 8080
        resources:
          requests:
            memory: "512Mi"
            cpu: "500m"
          limits:
            memory: "1Gi"
            cpu: "1000m"
```

3. **자동 확장 규칙**
```yaml
# Horizontal Pod Autoscaler
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: network-service-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: network-service
  minReplicas: 2
  maxReplicas: 10
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
  - type: Resource
    resource:
      name: memory
      target:
        type: Utilization
        averageUtilization: 80
```

### 데이터베이스 확장

1. **연결 풀링**
```cpp
class DatabasePool {
    HikariConfig config;
    config.setJdbcUrl("jdbc:postgresql://localhost/network");
    config.setMaximumPoolSize(20);
    config.setMinimumIdle(5);
    config.setIdleTimeout(300000);
    config.setConnectionTimeout(30000);
};
```

2. **읽기 복제본**
```cpp
class DatabaseManager {
    std::vector<Database> read_replicas_;
    Database write_master_;

    Database& get_read_connection() {
        // 라운드 로빈 선택
        static std::atomic<size_t> index{0};
        return read_replicas_[index++ % read_replicas_.size()];
    }

    Database& get_write_connection() {
        return write_master_;
    }
};
```

### 캐싱 전략

```cpp
class CacheManager {
    LRUCache<std::string, Response> response_cache_{10000};
    std::chrono::seconds ttl_{60};

    std::optional<Response> get(const Request& request) {
        auto key = generate_cache_key(request);
        return response_cache_.get(key);
    }

    void put(const Request& request, const Response& response) {
        auto key = generate_cache_key(request);
        response_cache_.put(key, response, ttl_);
    }
};
```

## 유지보수 창

### 계획된 유지보수

1. **알림 일정**
   - 2주 전: 초기 알림
   - 1주 전: 상세 유지보수 계획
   - 1일 전: 최종 알림
   - 1시간 전: 마지막 경고

2. **유지보수 절차**
```bash
#!/bin/bash
# maintenance.sh

# 유지보수 모드 활성화
echo "true" > /var/run/network_service/maintenance

# 활성 연결이 종료될 때까지 대기
sleep 30

# 유지보수 작업 수행
backup_configuration
update_software
run_migrations
verify_configuration

# 유지보수 모드 비활성화
echo "false" > /var/run/network_service/maintenance

# 서비스 상태 확인
check_service_health
```

### 롤링 업데이트

```bash
#!/bin/bash
# rolling_update.sh

SERVERS=("server1" "server2" "server3")
LOAD_BALANCER="lb.example.com"

for server in "${SERVERS[@]}"; do
    echo "Updating $server"

    # 로드 밸런서에서 제거
    remove_from_lb $LOAD_BALANCER $server

    # 연결이 종료될 때까지 대기
    sleep 60

    # 서버 업데이트
    ssh $server "sudo systemctl stop network_service"
    ssh $server "sudo apt-get update && sudo apt-get upgrade network-system"
    ssh $server "sudo systemctl start network_service"

    # 상태 확인
    wait_for_health $server

    # 로드 밸런서에 다시 추가
    add_to_lb $LOAD_BALANCER $server

    # 다음 서버 전 대기
    sleep 120
done
```

## 규정 준수 및 감사

### 감사 요구사항

1. **보안 감사**
   - 인증 시도
   - 권한 부여 실패
   - 구성 변경
   - 데이터 액세스 패턴
   - 네트워크 연결

2. **규정 준수 로깅**
```cpp
class ComplianceLogger {
    void log_data_access(const DataAccessEvent& event) {
        // GDPR/CCPA 규정 준수 로깅
        json entry = {
            {"timestamp", event.timestamp},
            {"user_id", hash_user_id(event.user_id)},
            {"data_type", event.data_type},
            {"operation", event.operation},
            {"purpose", event.purpose},
            {"legal_basis", event.legal_basis}
        };

        compliance_log_ << entry.dump() << std::endl;
    }
};
```

### 보존 정책

```yaml
# 로그 보존 구성
retention:
  access_logs:
    duration: 90d
    compress_after: 7d
  error_logs:
    duration: 180d
    compress_after: 30d
  security_logs:
    duration: 365d
    compress_after: 30d
    archive_to: s3://backup/security/
  compliance_logs:
    duration: 2555d  # 7년
    compress_after: 90d
    archive_to: s3://backup/compliance/
```

## 결론

이 운영 가이드는 Network System의 배포, 모니터링, 성능 튜닝, 보안 및 확장 전략에 대한 포괄적인 내용을 제공합니다. 이러한 절차를 정기적으로 검토하고 업데이트하면 최적의 시스템 성능과 안정성을 보장할 수 있습니다.

특정 문제 해결 시나리오는 [문제 해결 가이드](TROUBLESHOOTING_KO.md)를 참조하십시오.

---

*Last Updated: 2025-10-20*
