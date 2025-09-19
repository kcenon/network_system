# Network System Troubleshooting Guide

## Table of Contents
1. [Common Issues and Solutions](#common-issues-and-solutions)
2. [Debugging Techniques](#debugging-techniques)
3. [Performance Problems](#performance-problems)
4. [Connection Issues](#connection-issues)
5. [Memory Leaks](#memory-leaks)
6. [Build Problems](#build-problems)
7. [Platform-Specific Issues](#platform-specific-issues)
8. [Error Reference](#error-reference)

## Common Issues and Solutions

### Server Fails to Start

#### Problem: Port Already in Use
```
Error: Failed to bind to port 8080: Address already in use
```

**Solution:**
```bash
# Find process using the port
lsof -i :8080  # Linux/macOS
netstat -ano | findstr :8080  # Windows

# Kill the process
kill -9 <PID>  # Linux/macOS
taskkill /PID <PID> /F  # Windows

# Or change the port in configuration
server->start_server(8081);
```

#### Problem: Insufficient Permissions
```
Error: Permission denied: Cannot bind to port 80
```

**Solution:**
```bash
# Use a port above 1024, or run with elevated privileges
sudo ./network_server  # Not recommended for production

# Better: Use capabilities on Linux
sudo setcap 'cap_net_bind_service=+ep' ./network_server
```

#### Problem: Missing Dependencies
```
Error: libNetworkSystem.so: cannot open shared object file
```

**Solution:**
```bash
# Update library path
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH  # Linux
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH  # macOS

# Or reinstall
sudo ldconfig  # Linux
```

### Client Connection Failures

#### Problem: Connection Refused
```cpp
std::runtime_error: Connection refused: localhost:8080
```

**Solution:**
```bash
# Verify server is running
ps aux | grep network_server
systemctl status network_system

# Check firewall rules
sudo iptables -L -n  # Linux
sudo pfctl -s rules  # macOS
netsh advfirewall show all  # Windows

# Allow port in firewall
sudo ufw allow 8080  # Ubuntu
sudo firewall-cmd --add-port=8080/tcp --permanent  # CentOS
```

#### Problem: Connection Timeout
```cpp
timeout_error: Connection timeout after 30 seconds
```

**Solution:**
```cpp
// Increase timeout
client->set_connect_timeout(std::chrono::seconds(60));

// Check network connectivity
ping server_host
traceroute server_host  # or tracert on Windows

// Verify MTU settings
ping -M do -s 1472 server_host  # Linux
ping -f -l 1472 server_host  # Windows
```

### High CPU Usage

#### Problem: CPU at 100%
**Diagnosis:**
```bash
# Identify high CPU threads
top -H -p $(pidof network_server)
htop  # Better visualization

# Profile CPU usage
perf top -p $(pidof network_server)
```

**Solution:**
```cpp
// Reduce thread pool size
server->set_thread_pool_size(4);  // Instead of auto

// Enable CPU throttling
server->set_max_cpu_usage(80);  // Limit to 80%

// Optimize message processing
server->enable_batch_processing(true);
server->set_batch_size(1000);
```

### Memory Issues

#### Problem: Out of Memory
```
std::bad_alloc: Cannot allocate memory
```

**Solution:**
```cpp
// Limit connection count
server->set_max_connections(1000);

// Enable memory limits
server->set_max_memory_usage(1024 * 1024 * 1024);  // 1GB

// Use memory pool
auto pool = std::make_shared<memory_pool>(100 * 1024 * 1024);  // 100MB pool
server->set_memory_pool(pool);
```

## Debugging Techniques

### Enable Debug Logging

```cpp
// Set log level to debug
network_system::logger::set_level(network_system::log_level::debug);

// Enable component-specific debugging
server->enable_debug_mode({
    .log_packets = true,
    .log_connections = true,
    .log_errors = true,
    .log_performance = true
});
```

### Core Dump Analysis

#### Enable Core Dumps
```bash
# Linux
ulimit -c unlimited
echo "/tmp/core-%e-%p-%t" | sudo tee /proc/sys/kernel/core_pattern

# macOS
ulimit -c unlimited
sudo sysctl kern.corefile=/cores/core.%P
```

#### Analyze Core Dump
```bash
# Using GDB
gdb ./network_server core.12345
(gdb) bt  # Backtrace
(gdb) thread apply all bt  # All threads
(gdb) info registers
(gdb) info locals
(gdb) list

# Using LLDB (macOS)
lldb ./network_server -c core.12345
(lldb) bt all
(lldb) thread list
(lldb) frame variable
```

### Network Traffic Analysis

#### Using tcpdump
```bash
# Capture all traffic on port 8080
sudo tcpdump -i any -n port 8080 -w capture.pcap

# Real-time monitoring
sudo tcpdump -i eth0 -n port 8080 -A  # ASCII output

# Filter specific IP
sudo tcpdump -i any host 192.168.1.100 and port 8080
```

#### Using Wireshark
```bash
# Capture with dumpcap (Wireshark's capture tool)
dumpcap -i eth0 -f "tcp port 8080" -w capture.pcapng

# Analyze in Wireshark
wireshark capture.pcapng
# Apply display filter: tcp.port == 8080
```

### Debugging with Sanitizers

#### AddressSanitizer
```bash
# Build with ASAN
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..
./network_server

# Interpret ASAN output
==12345==ERROR: AddressSanitizer: heap-buffer-overflow
    #0 0x7f8b in process_message() messaging_session.cpp:123
```

#### ThreadSanitizer
```bash
# Build with TSAN
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON ..
./network_server

# Detect race conditions
WARNING: ThreadSanitizer: data race
  Write of size 8 at 0x7b04 by thread T2:
    #0 update_counter() metrics.cpp:45
```

## Performance Problems

### Slow Message Processing

#### Diagnosis
```cpp
// Enable performance metrics
auto metrics = server->get_performance_metrics();
std::cout << "Avg latency: " << metrics.avg_latency_ms << "ms\n";
std::cout << "P99 latency: " << metrics.p99_latency_ms << "ms\n";
```

#### Solutions
```cpp
// 1. Optimize serialization
server->use_binary_protocol();  // Instead of JSON

// 2. Enable zero-copy
server->enable_zero_copy(true);

// 3. Reduce allocations
server->set_buffer_reuse(true);
server->set_initial_buffer_size(4096);
```

### Low Throughput

#### Diagnosis
```bash
# Monitor network utilization
iftop -i eth0
nethogs  # Per-process bandwidth

# Check TCP statistics
ss -s
netstat -s | grep -i retrans
```

#### Solutions
```cpp
// 1. Increase buffer sizes
server->set_receive_buffer_size(256 * 1024);
server->set_send_buffer_size(256 * 1024);

// 2. Enable TCP optimizations
server->set_tcp_nodelay(true);  // Disable Nagle
server->set_tcp_quickack(true);  // Quick ACKs

// 3. Use multiple acceptor threads
server->set_acceptor_threads(4);
```

### High Latency

#### Network Latency Check
```bash
# Measure RTT
ping -c 100 server_host | tail -1

# TCP connection time
time nc -zv server_host 8080

# Full transaction time
curl -w "@curl-format.txt" -o /dev/null -s http://server:8080/health
```

#### Application Latency
```cpp
// Profile message handling
server->enable_profiling({
    .sample_rate = 0.01,  // 1% of messages
    .output_file = "profile.json"
});

// Analyze hotspots
auto profile = server->get_profile_data();
for (const auto& [function, time] : profile.hotspots) {
    std::cout << function << ": " << time << "ms\n";
}
```

## Connection Issues

### Connection Drops

#### Diagnosis
```bash
# Check connection state
ss -tan | grep 8080
netstat -an | grep 8080

# Monitor connection drops
watch 'ss -s'
```

#### Common Causes and Solutions

**1. Timeout Issues**
```cpp
// Increase timeouts
server->set_idle_timeout(std::chrono::minutes(30));
server->set_keepalive_interval(std::chrono::seconds(30));
```

**2. Resource Limits**
```bash
# Check system limits
ulimit -n  # File descriptors
sysctl net.core.somaxconn  # Listen backlog

# Increase limits
ulimit -n 65535
sudo sysctl -w net.core.somaxconn=8192
```

**3. Network Issues**
```bash
# Check for packet loss
mtr server_host  # Continuous traceroute
iperf3 -c server_host  # Bandwidth test
```

### SSL/TLS Issues

#### Certificate Problems
```bash
# Verify certificate
openssl s_client -connect server:8443 -showcerts

# Check certificate expiry
openssl x509 -in cert.pem -noout -dates

# Verify certificate chain
openssl verify -CAfile ca.pem cert.pem
```

#### Cipher Suite Issues
```cpp
// Enable all secure ciphers
server->set_tls_ciphers("HIGH:!aNULL:!MD5:!RC4");

// Debug TLS handshake
server->enable_tls_debug(true);
```

## Memory Leaks

### Detection

#### Using Valgrind
```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --log-file=valgrind.log \
         ./network_server

# Analyze results
grep "definitely lost" valgrind.log
grep "indirectly lost" valgrind.log
```

#### Using Built-in Tracking
```cpp
// Enable memory tracking
server->enable_memory_tracking(true);

// Get memory report
auto report = server->get_memory_report();
std::cout << "Allocated: " << report.allocated_bytes << "\n";
std::cout << "Freed: " << report.freed_bytes << "\n";
std::cout << "Leaked: " << report.leaked_bytes << "\n";

// Dump allocation sites
for (const auto& [location, size] : report.allocations) {
    std::cout << location << ": " << size << " bytes\n";
}
```

### Common Leak Patterns

**1. Circular References**
```cpp
// Problem: Shared pointers creating cycles
class Session {
    std::shared_ptr<Connection> connection;  // Connection holds Session
};

// Solution: Use weak_ptr
class Session {
    std::weak_ptr<Connection> connection;
};
```

**2. Forgotten Callbacks**
```cpp
// Problem: Callbacks holding references
server->on_message([this, large_object](auto msg) {
    // large_object never freed
});

// Solution: Capture by weak_ptr
server->on_message([weak_this = weak_from_this()](auto msg) {
    if (auto self = weak_this.lock()) {
        // Process message
    }
});
```

## Build Problems

### CMake Configuration Errors

#### Missing Dependencies
```
CMake Error: Could NOT find ASIO (missing: ASIO_INCLUDE_DIR)
```

**Solution:**
```bash
# Install ASIO
sudo apt-get install libasio-dev  # Ubuntu/Debian
brew install asio  # macOS
vcpkg install asio  # Windows

# Specify path manually
cmake -DASIO_ROOT=/usr/local/include ..
```

#### Compiler Version Issues
```
CMake Error: C++20 support required
```

**Solution:**
```bash
# Update compiler
sudo apt-get install g++-11  # Ubuntu
brew install gcc@11  # macOS

# Specify compiler
cmake -DCMAKE_CXX_COMPILER=g++-11 ..
```

### Compilation Errors

#### Template Errors
```cpp
error: template argument deduction/substitution failed
```

**Solution:**
```cpp
// Be explicit with template parameters
auto result = function<int, std::string>(42, "test");
// Instead of: auto result = function(42, "test");
```

#### Linking Errors
```
undefined reference to `network_system::core::messaging_server::start_server(unsigned short)'
```

**Solution:**
```bash
# Ensure library is linked
target_link_libraries(your_app NetworkSystem::NetworkSystem)

# Check library path
ldd your_app | grep NetworkSystem
```

## Platform-Specific Issues

### Linux

#### epoll Errors
```
Error: epoll_wait failed: Bad file descriptor
```

**Solution:**
```cpp
// Recreate epoll instance
server->restart_event_loop();

// Or increase epoll limits
echo 65535 | sudo tee /proc/sys/fs/epoll/max_user_watches
```

#### Systemd Service Issues
```bash
# Service fails to start
journalctl -u network_system -n 50

# Common fixes
# 1. Fix permissions
sudo chown -R network_system:network_system /opt/network_system

# 2. Fix path
systemctl edit network_system
# Add: Environment="LD_LIBRARY_PATH=/usr/local/lib"
```

### macOS

#### kqueue Errors
```
Error: kqueue failed: Too many open files
```

**Solution:**
```bash
# Increase limits
sudo launchctl limit maxfiles 65536 200000
ulimit -n 65536

# Permanent fix in /etc/sysctl.conf
kern.maxfiles=65536
kern.maxfilesperproc=65536
```

#### Code Signing Issues
```
killed: 9
```

**Solution:**
```bash
# Sign the binary
codesign -s - ./network_server

# Or disable library validation
sudo spctl --master-disable  # Not recommended
```

### Windows

#### Winsock Errors
```
Error: WSAStartup failed: 10093
```

**Solution:**
```cpp
// Initialize Winsock properly
WSADATA wsaData;
int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
if (result != 0) {
    std::cerr << "WSAStartup failed: " << result << std::endl;
}
```

#### Firewall Blocking
```powershell
# Add firewall rule
New-NetFirewallRule -DisplayName "Network System" `
    -Direction Inbound `
    -Protocol TCP `
    -LocalPort 8080 `
    -Action Allow
```

## Error Reference

### Error Codes

| Code | Name | Description | Solution |
|------|------|-------------|----------|
| 1001 | CONNECTION_REFUSED | Server not accepting connections | Check server status |
| 1002 | CONNECTION_TIMEOUT | Connection attempt timed out | Increase timeout, check network |
| 1003 | CONNECTION_RESET | Connection reset by peer | Check for crashes, review logs |
| 2001 | INVALID_MESSAGE | Message format invalid | Validate message structure |
| 2002 | MESSAGE_TOO_LARGE | Message exceeds size limit | Increase limit or split message |
| 3001 | AUTH_FAILED | Authentication failure | Verify credentials |
| 3002 | PERMISSION_DENIED | Insufficient permissions | Check access rights |
| 4001 | RESOURCE_EXHAUSTED | System resources depleted | Scale resources or limit usage |
| 4002 | MEMORY_LIMIT | Memory limit exceeded | Increase limit or optimize usage |
| 5001 | INTERNAL_ERROR | Unexpected server error | Check logs, report bug |

### Common Log Messages

#### INFO Level
```
[INFO] Server started on port 8080
[INFO] Client connected from 192.168.1.100:45678
[INFO] Session sess-abc123 established
```

#### WARNING Level
```
[WARN] Connection pool near capacity (90% used)
[WARN] Message processing slow: 500ms for msg-xyz789
[WARN] Memory usage high: 1.8GB of 2GB limit
```

#### ERROR Level
```
[ERROR] Failed to bind to port: Address in use
[ERROR] Session timeout after 30 seconds
[ERROR] Memory allocation failed: std::bad_alloc
```

### Getting Help

If issues persist after trying these solutions:

1. **Collect Diagnostic Information**
```bash
./scripts/collect_diagnostics.sh
# Creates diagnostic_bundle.tar.gz with:
# - System info
# - Configuration
# - Recent logs
# - Performance metrics
```

2. **Check Documentation**
- [API Reference](API_REFERENCE.md)
- [Operations Guide](OPERATIONS.md)
- [GitHub Issues](https://github.com/your-org/network_system/issues)

3. **Contact Support**
- Email: support@network-system.io
- Slack: #network-system-help
- Include diagnostic bundle and steps to reproduce

---

Remember: Most issues can be diagnosed by checking logs, monitoring metrics, and understanding the system's behavior under load.