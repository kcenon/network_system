# Network System Operations Guide

## Table of Contents
1. [Deployment](#deployment)
2. [Configuration Management](#configuration-management)
3. [Monitoring and Metrics](#monitoring-and-metrics)
4. [Performance Tuning](#performance-tuning)
5. [Backup and Recovery](#backup-and-recovery)
6. [Security Considerations](#security-considerations)
7. [Scaling Strategies](#scaling-strategies)
8. [Maintenance Procedures](#maintenance-procedures)

## Deployment

### Prerequisites
- C++20 compatible system
- CMake 3.16 or later
- ASIO or Boost.ASIO 1.28+
- Minimum 2GB RAM for production workloads
- Network ports configured for application traffic

### Linux Deployment

#### System Preparation
```bash
# Update system
sudo apt-get update && sudo apt-get upgrade

# Install dependencies
sudo apt-get install -y build-essential cmake libasio-dev libfmt-dev

# Configure system limits
echo "* soft nofile 65535" | sudo tee -a /etc/security/limits.conf
echo "* hard nofile 65535" | sudo tee -a /etc/security/limits.conf

# Optimize network stack
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.wmem_max=134217728
sudo sysctl -w net.core.netdev_max_backlog=5000
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 134217728"
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 134217728"
```

#### Installation Steps
```bash
# Clone and build
git clone https://github.com/your-org/network_system.git
cd network_system
./scripts/install.sh --prefix /opt/network_system --build-type Release

# Create service user
sudo useradd -r -s /bin/false network_system

# Set permissions
sudo chown -R network_system:network_system /opt/network_system
```

#### Systemd Service
```ini
# /etc/systemd/system/network_system.service
[Unit]
Description=Network System Service
After=network.target

[Service]
Type=simple
User=network_system
Group=network_system
ExecStart=/opt/network_system/bin/network_server
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=10
StandardOutput=journal
StandardError=journal
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

### Docker Deployment

#### Dockerfile
```dockerfile
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git libasio-dev libfmt-dev

WORKDIR /build
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && cmake --install build --prefix /app

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libasio-dev libfmt9 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app /app
WORKDIR /app

USER 1000:1000
EXPOSE 8080
CMD ["/app/bin/network_server"]
```

#### Docker Compose
```yaml
version: '3.8'

services:
  network_system:
    image: network_system:latest
    ports:
      - "8080:8080"
    environment:
      - NETWORK_LOG_LEVEL=info
      - NETWORK_MAX_CONNECTIONS=10000
    volumes:
      - ./config:/app/config:ro
      - ./logs:/app/logs
    restart: unless-stopped
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 4G
        reservations:
          cpus: '2'
          memory: 2G
```

### Windows Deployment

#### PowerShell Installation
```powershell
# Run as Administrator
Set-ExecutionPolicy Bypass -Scope Process

# Install with script
.\scripts\install.ps1 -InstallPrefix "C:\NetworkSystem" -BuildType Release

# Create Windows Service
New-Service -Name "NetworkSystem" `
    -BinaryPath "C:\NetworkSystem\bin\network_server.exe" `
    -DisplayName "Network System Service" `
    -StartupType Automatic `
    -Description "High-performance network service"

Start-Service -Name "NetworkSystem"
```

## Configuration Management

### Configuration File Structure

#### config.yaml
```yaml
# Network System Configuration
server:
  host: "0.0.0.0"
  port: 8080
  max_connections: 10000
  connection_timeout: 30s
  keep_alive: true
  no_delay: true

performance:
  thread_pool_size: 0  # 0 = auto-detect
  receive_buffer_size: 65536
  send_buffer_size: 65536
  message_batch_size: 100
  flush_interval: 10ms

logging:
  level: info  # debug, info, warning, error
  file: /var/log/network_system/server.log
  max_size: 100MB
  max_files: 10
  format: json

security:
  tls_enabled: false
  cert_file: /etc/network_system/cert.pem
  key_file: /etc/network_system/key.pem
  allowed_ips:
    - 192.168.1.0/24
    - 10.0.0.0/8
```

### Environment Variables
```bash
# Override configuration with environment variables
export NETWORK_SERVER_PORT=9090
export NETWORK_LOG_LEVEL=debug
export NETWORK_MAX_CONNECTIONS=5000
export NETWORK_TLS_ENABLED=true
```

### Dynamic Configuration Updates
```cpp
// Runtime configuration changes
auto config = network_system::config::load("config.yaml");
server->apply_config(config);

// Hot reload on SIGHUP
signal(SIGHUP, [](int) {
    auto new_config = network_system::config::load("config.yaml");
    server->apply_config(new_config);
});
```

## Monitoring and Metrics

### Prometheus Integration

#### Metrics Endpoint
```cpp
// Expose metrics on /metrics endpoint
server->register_metrics_handler("/metrics", [](const auto& request) {
    return network_system::metrics::prometheus_format();
});
```

#### Key Metrics
```yaml
# Prometheus scrape configuration
scrape_configs:
  - job_name: 'network_system'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: '/metrics'
    scrape_interval: 15s
```

#### Available Metrics
- `network_connections_total` - Total connections handled
- `network_connections_active` - Current active connections
- `network_messages_sent_total` - Total messages sent
- `network_messages_received_total` - Total messages received
- `network_bytes_sent_total` - Total bytes sent
- `network_bytes_received_total` - Total bytes received
- `network_errors_total` - Total errors by type
- `network_message_latency_seconds` - Message processing latency
- `network_connection_duration_seconds` - Connection duration histogram

### Logging Configuration

#### Structured Logging
```cpp
// JSON format for log aggregation
{
    "timestamp": "2024-01-15T10:30:45.123Z",
    "level": "info",
    "thread": "worker-3",
    "session": "sess-abc123",
    "message": "Connection established",
    "client_ip": "192.168.1.100",
    "port": 45678,
    "latency_ms": 23
}
```

#### Log Aggregation
```yaml
# Fluentd configuration
<source>
  @type tail
  path /var/log/network_system/*.log
  pos_file /var/log/td-agent/network_system.pos
  tag network.system
  <parse>
    @type json
  </parse>
</source>

<match network.system>
  @type elasticsearch
  host elasticsearch.local
  port 9200
  index_name network_system
</match>
```

### Health Checks

#### HTTP Health Endpoint
```cpp
server->register_handler("/health", [](const auto& request) {
    auto health = network_system::health::check();
    return json{
        {"status", health.is_healthy ? "healthy" : "unhealthy"},
        {"uptime", health.uptime_seconds},
        {"connections", health.active_connections},
        {"memory_mb", health.memory_usage_mb}
    };
});
```

#### Kubernetes Probes
```yaml
livenessProbe:
  httpGet:
    path: /health
    port: 8080
  initialDelaySeconds: 30
  periodSeconds: 10

readinessProbe:
  httpGet:
    path: /ready
    port: 8080
  initialDelaySeconds: 5
  periodSeconds: 5
```

## Performance Tuning

### System-Level Optimization

#### Linux Kernel Parameters
```bash
# /etc/sysctl.d/99-network-system.conf
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.tcp_fin_timeout = 30
net.ipv4.tcp_keepalive_time = 300
net.ipv4.tcp_keepalive_probes = 5
net.ipv4.tcp_keepalive_intvl = 15
net.ipv4.ip_local_port_range = 1024 65535
net.ipv4.tcp_tw_reuse = 1
```

#### CPU Affinity
```cpp
// Pin worker threads to specific CPUs
server->set_thread_affinity({
    {0, {0, 1}},    // Thread 0 -> CPUs 0,1
    {1, {2, 3}},    // Thread 1 -> CPUs 2,3
    {2, {4, 5}},    // Thread 2 -> CPUs 4,5
    {3, {6, 7}}     // Thread 3 -> CPUs 6,7
});
```

### Application-Level Optimization

#### Connection Pooling
```cpp
auto pool = std::make_shared<network_system::connection_pool>();
pool->set_min_connections(10);
pool->set_max_connections(100);
pool->set_idle_timeout(std::chrono::minutes(5));
```

#### Message Batching
```cpp
server->set_batch_size(1000);
server->set_flush_interval(std::chrono::milliseconds(10));
```

#### Memory Pool
```cpp
// Pre-allocate message buffers
auto buffer_pool = std::make_shared<network_system::buffer_pool>();
buffer_pool->reserve(10000, 4096);  // 10000 buffers of 4KB each
server->set_buffer_pool(buffer_pool);
```

### Profiling and Analysis

#### CPU Profiling
```bash
# Using perf
sudo perf record -g ./network_server
sudo perf report

# Using Intel VTune
vtune -collect hotspots -app-working-dir=/app -- ./network_server
```

#### Memory Profiling
```bash
# Using Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./network_server

# Using AddressSanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..
./network_server
```

## Backup and Recovery

### Data Backup

#### Configuration Backup
```bash
#!/bin/bash
# Daily backup script
BACKUP_DIR="/backup/network_system"
DATE=$(date +%Y%m%d)

mkdir -p "$BACKUP_DIR"
tar czf "$BACKUP_DIR/config_$DATE.tar.gz" /etc/network_system/
tar czf "$BACKUP_DIR/logs_$DATE.tar.gz" /var/log/network_system/

# Keep only last 30 days
find "$BACKUP_DIR" -name "*.tar.gz" -mtime +30 -delete
```

### Disaster Recovery

#### Failover Configuration
```yaml
# HAProxy configuration for active-passive setup
global
    maxconn 50000

defaults
    mode tcp
    timeout connect 5s
    timeout client 30s
    timeout server 30s

backend network_system
    balance roundrobin
    server primary 192.168.1.10:8080 check
    server backup 192.168.1.11:8080 backup check
```

#### State Replication
```cpp
// Replicate session state to backup
server->enable_state_replication("192.168.1.11", 9090);
server->set_replication_interval(std::chrono::seconds(1));
```

## Security Considerations

### TLS Configuration

#### Certificate Management
```bash
# Generate self-signed certificate for testing
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes

# Production certificate with Let's Encrypt
certbot certonly --standalone -d network.example.com
```

#### TLS Settings
```cpp
server->enable_tls({
    .cert_file = "/etc/letsencrypt/live/network.example.com/fullchain.pem",
    .key_file = "/etc/letsencrypt/live/network.example.com/privkey.pem",
    .min_version = network_system::tls::version::tls_1_2,
    .ciphers = "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384"
});
```

### Access Control

#### IP Whitelisting
```cpp
server->set_ip_filter({
    .mode = network_system::ip_filter::whitelist,
    .addresses = {
        "192.168.1.0/24",
        "10.0.0.0/8",
        "172.16.0.0/12"
    }
});
```

#### Rate Limiting
```cpp
server->enable_rate_limiting({
    .max_requests_per_second = 1000,
    .burst_size = 5000,
    .block_duration = std::chrono::minutes(5)
});
```

### Audit Logging

```cpp
server->enable_audit_log({
    .file = "/var/log/network_system/audit.log",
    .events = {
        network_system::audit::connection_established,
        network_system::audit::connection_closed,
        network_system::audit::authentication_failed,
        network_system::audit::rate_limit_exceeded
    }
});
```

## Scaling Strategies

### Vertical Scaling

#### Resource Allocation
```yaml
# Kubernetes resource limits
resources:
  requests:
    memory: "2Gi"
    cpu: "2"
  limits:
    memory: "8Gi"
    cpu: "8"
```

### Horizontal Scaling

#### Load Balancing
```nginx
# Nginx load balancer configuration
upstream network_system {
    least_conn;
    server backend1.example.com:8080 weight=5;
    server backend2.example.com:8080 weight=5;
    server backend3.example.com:8080 weight=5;
    keepalive 32;
}

server {
    listen 80;
    location / {
        proxy_pass http://network_system;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
    }
}
```

#### Auto-Scaling
```yaml
# Kubernetes HPA
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: network-system-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: network-system
  minReplicas: 3
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

### Database Sharding

```cpp
// Connection routing based on shard key
auto shard_id = hash(session_id) % num_shards;
auto connection = shard_pools[shard_id]->get_connection();
```

## Maintenance Procedures

### Rolling Updates

```bash
#!/bin/bash
# Zero-downtime deployment
SERVERS=("server1" "server2" "server3")

for server in "${SERVERS[@]}"; do
    echo "Updating $server..."
    ssh $server "sudo systemctl stop network_system"
    scp build/network_server $server:/opt/network_system/bin/
    ssh $server "sudo systemctl start network_system"

    # Wait for health check
    until curl -f http://$server:8080/health; do
        sleep 5
    done
done
```

### Log Rotation

```bash
# /etc/logrotate.d/network_system
/var/log/network_system/*.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    create 0644 network_system network_system
    postrotate
        systemctl reload network_system
    endscript
}
```

### Performance Baseline

```bash
# Establish performance baseline
./tests/performance/benchmark --output baseline.json

# Compare after changes
./tests/performance/benchmark --output current.json
./scripts/compare_performance.py baseline.json current.json
```

---

For additional support, consult the [Troubleshooting Guide](TROUBLESHOOTING.md) or contact the development team.