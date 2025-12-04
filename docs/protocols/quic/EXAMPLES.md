# QUIC Examples

## Basic Echo Server and Client

### Echo Server

```cpp
#include "kcenon/network/core/messaging_quic_server.h"
#include <iostream>

using namespace network_system::core;

int main() {
    auto server = std::make_shared<messaging_quic_server>("echo_server");

    // Echo back received data
    server->set_receive_callback(
        [&server](auto session, const std::vector<uint8_t>& data) {
            std::cout << "Received: " << std::string(data.begin(), data.end()) << "\n";
            session->send(std::vector<uint8_t>(data));  // Echo back
        });

    server->set_connection_callback([](auto session) {
        std::cout << "Client connected: " << session->session_id() << "\n";
    });

    server->set_disconnection_callback([](auto session) {
        std::cout << "Client disconnected: " << session->session_id() << "\n";
    });

    if (auto result = server->start_server(4433); result.is_ok()) {
        std::cout << "Echo server started on port 4433\n";
        server->wait_for_stop();
    } else {
        std::cerr << "Failed to start: " << result.error().message << "\n";
        return 1;
    }

    return 0;
}
```

### Echo Client

```cpp
#include "kcenon/network/core/messaging_quic_client.h"
#include <iostream>
#include <thread>

using namespace network_system::core;

int main() {
    auto client = std::make_shared<messaging_quic_client>("echo_client");

    std::promise<void> connected_promise;
    std::promise<std::string> response_promise;

    client->set_connected_callback([&connected_promise]() {
        std::cout << "Connected!\n";
        connected_promise.set_value();
    });

    client->set_receive_callback([&response_promise](const std::vector<uint8_t>& data) {
        response_promise.set_value(std::string(data.begin(), data.end()));
    });

    quic_client_config config;
    config.verify_server = false;

    if (auto result = client->start_client("127.0.0.1", 4433, config); result.is_err()) {
        std::cerr << "Failed to connect: " << result.error().message << "\n";
        return 1;
    }

    // Wait for connection
    connected_promise.get_future().wait();

    // Send message
    client->send_packet("Hello, QUIC!");

    // Wait for response
    auto response = response_promise.get_future().get();
    std::cout << "Response: " << response << "\n";

    client->stop_client();
    return 0;
}
```

---

## Multi-Stream Communication

### Stream Multiplexing Client

```cpp
#include "kcenon/network/core/messaging_quic_client.h"
#include <iostream>
#include <map>
#include <mutex>

using namespace network_system::core;

class MultiStreamClient {
public:
    void run(const std::string& host, unsigned short port) {
        client_ = std::make_shared<messaging_quic_client>("multi_stream_client");

        // Track responses per stream
        client_->set_stream_receive_callback(
            [this](uint64_t stream_id, const std::vector<uint8_t>& data, bool fin) {
                std::lock_guard lock(mutex_);
                responses_[stream_id].append(data.begin(), data.end());
                if (fin) {
                    std::cout << "Stream " << stream_id << " complete: "
                              << responses_[stream_id] << "\n";
                }
            });

        std::promise<void> connected;
        client_->set_connected_callback([&connected]() {
            connected.set_value();
        });

        quic_client_config config;
        config.verify_server = false;
        config.initial_max_streams_bidi = 100;

        if (client_->start_client(host, port, config).is_err()) {
            return;
        }

        connected.get_future().wait();

        // Create multiple streams and send different requests
        for (int i = 0; i < 5; ++i) {
            auto stream_result = client_->create_stream();
            if (stream_result.is_ok()) {
                uint64_t stream_id = stream_result.value();
                std::string request = "Request #" + std::to_string(i);
                std::vector<uint8_t> data(request.begin(), request.end());
                client_->send_on_stream(stream_id, std::move(data), true);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        client_->stop_client();
    }

private:
    std::shared_ptr<messaging_quic_client> client_;
    std::mutex mutex_;
    std::map<uint64_t, std::string> responses_;
};

int main() {
    MultiStreamClient client;
    client.run("127.0.0.1", 4433);
    return 0;
}
```

### Multi-Stream Server

```cpp
#include "kcenon/network/core/messaging_quic_server.h"
#include <iostream>

using namespace network_system::core;

int main() {
    auto server = std::make_shared<messaging_quic_server>("multi_stream_server");

    // Handle per-stream data
    server->set_stream_receive_callback(
        [](auto session, uint64_t stream_id, const std::vector<uint8_t>& data, bool fin) {
            std::string request(data.begin(), data.end());
            std::cout << "Stream " << stream_id << " received: " << request << "\n";

            // Process and respond on the same stream
            std::string response = "Response to: " + request;
            std::vector<uint8_t> response_data(response.begin(), response.end());
            session->send_on_stream(stream_id, std::move(response_data), true);
        });

    quic_server_config config;
    config.initial_max_streams_bidi = 100;

    if (auto result = server->start_server(4433, config); result.is_ok()) {
        std::cout << "Multi-stream server started\n";
        server->wait_for_stop();
    }

    return 0;
}
```

---

## Broadcasting

### Chat Server Example

```cpp
#include "kcenon/network/core/messaging_quic_server.h"
#include <iostream>

using namespace network_system::core;

int main() {
    auto server = std::make_shared<messaging_quic_server>("chat_server");

    server->set_connection_callback([&server](auto session) {
        std::string msg = "User joined: " + session->session_id();
        std::vector<uint8_t> data(msg.begin(), msg.end());
        server->broadcast(std::move(data));
    });

    server->set_disconnection_callback([&server](auto session) {
        std::string msg = "User left: " + session->session_id();
        std::vector<uint8_t> data(msg.begin(), msg.end());
        server->broadcast(std::move(data));
    });

    server->set_receive_callback([&server](auto session, const std::vector<uint8_t>& data) {
        // Format: "[session_id] message"
        std::string formatted = "[" + session->session_id() + "] " +
                                std::string(data.begin(), data.end());
        std::vector<uint8_t> broadcast_data(formatted.begin(), formatted.end());
        server->broadcast(std::move(broadcast_data));
    });

    if (server->start_server(4433).is_ok()) {
        std::cout << "Chat server started on port 4433\n";
        server->wait_for_stop();
    }

    return 0;
}
```

---

## Connection Statistics

### Monitoring Client

```cpp
#include "kcenon/network/core/messaging_quic_client.h"
#include <iostream>
#include <chrono>

using namespace network_system::core;

void print_stats(const quic_connection_stats& stats) {
    std::cout << "=== Connection Statistics ===\n";
    std::cout << "Bytes sent:     " << stats.bytes_sent << "\n";
    std::cout << "Bytes received: " << stats.bytes_received << "\n";
    std::cout << "Packets sent:   " << stats.packets_sent << "\n";
    std::cout << "Packets recv:   " << stats.packets_received << "\n";
    std::cout << "Packets lost:   " << stats.packets_lost << "\n";
    std::cout << "Smoothed RTT:   " << stats.smoothed_rtt.count() << " us\n";
    std::cout << "Min RTT:        " << stats.min_rtt.count() << " us\n";
    std::cout << "CWND:           " << stats.cwnd << " bytes\n";
}

int main() {
    auto client = std::make_shared<messaging_quic_client>("stats_client");

    std::atomic<bool> connected{false};
    client->set_connected_callback([&connected]() {
        connected = true;
    });

    quic_client_config config;
    config.verify_server = false;

    if (client->start_client("127.0.0.1", 4433, config).is_err()) {
        return 1;
    }

    // Wait for connection
    while (!connected.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Send some data and monitor stats
    for (int i = 0; i < 10; ++i) {
        std::string msg = "Message " + std::to_string(i);
        client->send_packet(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    print_stats(client->stats());

    client->stop_client();
    return 0;
}
```

---

## Session Management

### Session Tracking Server

```cpp
#include "kcenon/network/core/messaging_quic_server.h"
#include <iostream>
#include <thread>

using namespace network_system::core;

int main() {
    auto server = std::make_shared<messaging_quic_server>("session_server");

    server->set_connection_callback([](auto session) {
        std::cout << "New session: " << session->session_id()
                  << " from " << session->remote_endpoint() << "\n";
    });

    if (server->start_server(4433).is_err()) {
        return 1;
    }

    std::cout << "Server started. Press Ctrl+C to stop.\n";

    // Periodically print session info
    while (server->is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto sessions = server->sessions();
        std::cout << "\n=== Active Sessions: " << sessions.size() << " ===\n";
        for (const auto& session : sessions) {
            auto stats = session->stats();
            std::cout << "  " << session->session_id()
                      << " - sent: " << stats.bytes_sent
                      << ", recv: " << stats.bytes_received << "\n";
        }
    }

    return 0;
}
```

### Selective Disconnect

```cpp
#include "kcenon/network/core/messaging_quic_server.h"

using namespace network_system::core;

int main() {
    auto server = std::make_shared<messaging_quic_server>("admin_server");

    server->set_receive_callback([&server](auto session, const std::vector<uint8_t>& data) {
        std::string cmd(data.begin(), data.end());

        if (cmd == "KICK_ALL") {
            server->disconnect_all();
        } else if (cmd.find("KICK ") == 0) {
            std::string target_id = cmd.substr(5);
            server->disconnect_session(target_id);
        }
    });

    server->start_server(4433);
    server->wait_for_stop();
    return 0;
}
```

---

## Error Handling

### Robust Client

```cpp
#include "kcenon/network/core/messaging_quic_client.h"
#include <iostream>

using namespace network_system::core;

class RobustClient {
public:
    bool connect(const std::string& host, unsigned short port, int max_retries = 3) {
        client_ = std::make_shared<messaging_quic_client>("robust_client");

        client_->set_error_callback([this](std::error_code ec) {
            std::cerr << "Error: " << ec.message() << "\n";
            error_occurred_ = true;
        });

        client_->set_disconnected_callback([this]() {
            std::cout << "Disconnected\n";
            connected_ = false;
        });

        std::promise<bool> connected_promise;
        client_->set_connected_callback([&connected_promise, this]() {
            connected_ = true;
            connected_promise.set_value(true);
        });

        quic_client_config config;
        config.verify_server = false;

        for (int attempt = 1; attempt <= max_retries; ++attempt) {
            std::cout << "Connection attempt " << attempt << "/" << max_retries << "\n";

            auto result = client_->start_client(host, port, config);
            if (result.is_err()) {
                std::cerr << "Failed: " << result.error().message << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            auto future = connected_promise.get_future();
            if (future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
                return true;
            }

            client_->stop_client();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return false;
    }

    bool send_with_retry(const std::string& message, int max_retries = 3) {
        for (int attempt = 1; attempt <= max_retries; ++attempt) {
            auto result = client_->send_packet(message);
            if (result.is_ok()) {
                return true;
            }

            std::cerr << "Send failed (attempt " << attempt << "): "
                      << result.error().message << "\n";

            if (!connected_) {
                // Connection lost, try to reconnect
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

private:
    std::shared_ptr<messaging_quic_client> client_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> error_occurred_{false};
};

int main() {
    RobustClient client;

    if (!client.connect("127.0.0.1", 4433)) {
        std::cerr << "Failed to establish connection\n";
        return 1;
    }

    client.send_with_retry("Hello, QUIC!");

    return 0;
}
```

---

## ALPN Protocol Negotiation

```cpp
#include "kcenon/network/core/messaging_quic_client.h"
#include "kcenon/network/core/messaging_quic_server.h"
#include <iostream>

using namespace network_system::core;

int main() {
    // Server with ALPN
    auto server = std::make_shared<messaging_quic_server>("alpn_server");

    quic_server_config server_config;
    server_config.alpn_protocols = {"myprotocol-v2", "myprotocol-v1"};

    server->start_server(4433, server_config);

    // Client with ALPN
    auto client = std::make_shared<messaging_quic_client>("alpn_client");

    client->set_connected_callback([&client]() {
        auto alpn = client->alpn_protocol();
        if (alpn) {
            std::cout << "Negotiated protocol: " << *alpn << "\n";
        } else {
            std::cout << "No ALPN negotiated\n";
        }
    });

    quic_client_config client_config;
    client_config.verify_server = false;
    client_config.alpn_protocols = {"myprotocol-v2", "myprotocol-v1"};

    client->start_client("127.0.0.1", 4433, client_config);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    client->stop_client();
    server->stop_server();

    return 0;
}
```

---

## Integration with Existing Code

### Migrating from TCP Client

**Before (TCP):**
```cpp
auto client = std::make_shared<messaging_client>("tcp_client");
client->set_receive_callback([](auto data) { /* ... */ });
client->start_client("127.0.0.1", 8080);
client->send_packet(data);
```

**After (QUIC):**
```cpp
auto client = std::make_shared<messaging_quic_client>("quic_client");
client->set_receive_callback([](auto data) { /* ... */ });

quic_client_config config;
config.verify_server = false;  // If using self-signed cert
client->start_client("127.0.0.1", 4433, config);
client->send_packet(data);
```

Key differences:
1. Use `messaging_quic_client` instead of `messaging_client`
2. Configure TLS settings via `quic_client_config`
3. Use UDP port (default 443 for HTTPS, 4433 for testing)
