# Dependency & Testing

> **Part 2 of 4** | [📑 Index](README.md) | [← Previous: Architecture](01-architecture-and-components.md) | [Next: Performance →](03-performance-and-resources.md)

> **Language:** **English** | [한국어](02-dependency-and-testing_KO.md)

This document covers dependency management (CMake, pkg-config) and testing strategies for the network_system.

---

## 🔗 Dependency Management

### 1. CMake Module Files

```cmake
# cmake/FindNetworkSystem.cmake
find_package(PkgConfig QUIET)

# ASIO dependency
find_package(asio CONFIG REQUIRED)
if(NOT asio_FOUND)
    message(FATAL_ERROR "ASIO library is required for NetworkSystem")
endif()

# fmt dependency
find_package(fmt CONFIG REQUIRED)
if(NOT fmt_FOUND)
    message(FATAL_ERROR "fmt library is required for NetworkSystem")
endif()

# Check conditional dependencies
if(BUILD_WITH_CONTAINER_SYSTEM)
    find_package(ContainerSystem CONFIG)
    if(NOT ContainerSystem_FOUND)
        message(WARNING "ContainerSystem not found, container integration disabled")
        set(BUILD_WITH_CONTAINER_SYSTEM OFF)
    endif()
endif()

if(BUILD_WITH_THREAD_SYSTEM)
    find_package(ThreadSystem CONFIG)
    if(NOT ThreadSystem_FOUND)
        message(WARNING "ThreadSystem not found, thread integration disabled")
        set(BUILD_WITH_THREAD_SYSTEM OFF)
    endif()
endif()

# Define library target
if(NOT TARGET NetworkSystem::Core)
    add_library(NetworkSystem::Core INTERFACE)
    target_include_directories(NetworkSystem::Core INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/../include
    )
    target_link_libraries(NetworkSystem::Core INTERFACE
        asio::asio
        fmt::fmt
    )

    if(BUILD_WITH_CONTAINER_SYSTEM)
        target_link_libraries(NetworkSystem::Core INTERFACE
            ContainerSystem::Core
        )
        target_compile_definitions(NetworkSystem::Core INTERFACE
            BUILD_WITH_CONTAINER_SYSTEM
        )
    endif()

    if(BUILD_WITH_THREAD_SYSTEM)
        target_link_libraries(NetworkSystem::Core INTERFACE
            ThreadSystem::Core
        )
        target_compile_definitions(NetworkSystem::Core INTERFACE
            BUILD_WITH_THREAD_SYSTEM
        )
    endif()
endif()

# Set variables
set(NetworkSystem_FOUND TRUE)
set(NetworkSystem_VERSION "2.0.0")
set(NetworkSystem_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../include)
```

### 2. pkg-config File

```ini
# network-system.pc.in
prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@

Name: NetworkSystem
Description: High-performance modular network system
Version: @PROJECT_VERSION@
Requires: asio fmt
Requires.private: @PRIVATE_DEPENDENCIES@
Libs: -L${libdir} -lNetworkSystem
Libs.private: @PRIVATE_LIBS@
Cflags: -I${includedir}
```

---

## 🧪 Test Framework

### 1. Unit Test Structure

```cpp
// tests/unit/core/test_messaging_server.cpp
#include <gtest/gtest.h>
#include "network_system/core/messaging_server.h"
#include "network_system/core/messaging_client.h"
#include "test_utils/network_test_utils.h"

namespace network_system::test {

class MessagingServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_config config;
        config.port = test_utils::get_free_port();
        config.max_connections = 10;

        server_ = std::make_shared<core::messaging_server>("test_server", config);

        // Set test handlers
        server_->set_message_handler([this](auto session, const std::string& msg) {
            received_messages_.push_back(msg);
            last_session_ = session;
        });

        server_->set_connect_handler([this](auto session) {
            connected_sessions_.push_back(session->session_id());
        });

        server_->set_disconnect_handler([this](auto session, const std::string& reason) {
            disconnected_sessions_.push_back(session->session_id());
            disconnect_reasons_.push_back(reason);
        });
    }

    void TearDown() override {
        if (server_ && server_->is_running()) {
            server_->stop_server();
        }
        server_.reset();
    }

    std::shared_ptr<core::messaging_server> server_;
    std::vector<std::string> received_messages_;
    std::vector<std::string> connected_sessions_;
    std::vector<std::string> disconnected_sessions_;
    std::vector<std::string> disconnect_reasons_;
    std::shared_ptr<session::messaging_session> last_session_;
};

TEST_F(MessagingServerTest, ServerStartStop) {
    EXPECT_FALSE(server_->is_running());

    EXPECT_TRUE(server_->start_server());
    EXPECT_TRUE(server_->is_running());

    server_->stop_server();
    EXPECT_FALSE(server_->is_running());
}

TEST_F(MessagingServerTest, ClientConnection) {
    ASSERT_TRUE(server_->start_server());

    auto client = std::make_shared<core::messaging_client>("test_client");

    bool connected = false;
    client->set_connect_handler([&connected]() {
        connected = true;
    });

    EXPECT_TRUE(client->start_client("127.0.0.1", server_->port()));

    // Wait for connection
    test_utils::wait_for_condition([&connected]() { return connected; },
                                   std::chrono::seconds(5));

    EXPECT_TRUE(connected);
    EXPECT_EQ(1, connected_sessions_.size());
    EXPECT_EQ(1, server_->connection_count());
}

TEST_F(MessagingServerTest, MessageExchange) {
    ASSERT_TRUE(server_->start_server());

    auto client = std::make_shared<core::messaging_client>("test_client");

    std::string received_message;
    client->set_message_handler([&received_message](const std::string& msg) {
        received_message = msg;
    });

    ASSERT_TRUE(client->start_client("127.0.0.1", server_->port()));

    // Wait for connection
    test_utils::wait_for_condition([this]() {
        return !connected_sessions_.empty();
    }, std::chrono::seconds(5));

    // Send message from client to server
    const std::string test_message = "Hello, Server!";
    client->send_message(test_message);

    // Wait for message reception
    test_utils::wait_for_condition([this]() {
        return !received_messages_.empty();
    }, std::chrono::seconds(5));

    EXPECT_EQ(1, received_messages_.size());
    EXPECT_EQ(test_message, received_messages_[0]);

    // Send response from server to client
    const std::string response_message = "Hello, Client!";
    ASSERT_NE(nullptr, last_session_);
    last_session_->send_message(response_message);

    // Wait for response reception
    test_utils::wait_for_condition([&received_message, &response_message]() {
        return received_message == response_message;
    }, std::chrono::seconds(5));

    EXPECT_EQ(response_message, received_message);
}

TEST_F(MessagingServerTest, MaxConnectionsLimit) {
    server_config config = server_->get_config();
    config.max_connections = 2;
    server_->set_config(config);

    ASSERT_TRUE(server_->start_server());

    // Create clients up to max connections
    std::vector<std::shared_ptr<core::messaging_client>> clients;
    for (int i = 0; i < 3; ++i) {
        auto client = std::make_shared<core::messaging_client>(
            "test_client_" + std::to_string(i));
        clients.push_back(client);

        client->start_client("127.0.0.1", server_->port());
    }

    // Wait briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify max connections not exceeded
    EXPECT_LE(server_->connection_count(), 2);
}

} // namespace network_system::test
```

### 2. Integration Tests

```cpp
// tests/integration/test_container_integration.cpp
#ifdef BUILD_WITH_CONTAINER_SYSTEM

#include <gtest/gtest.h>
#include "network_system/integration/messaging_bridge.h"
#include "container_system/value_container.h"

namespace network_system::test {

class ContainerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        bridge_ = std::make_unique<integration::messaging_bridge>("test_bridge");

        // Set container_system factory
        auto factory = std::make_shared<container_system::value_factory>();
        bridge_->set_container_factory(factory);
        bridge_->enable_container_serialization(true);

        // Set container message handler
        bridge_->set_container_message_handler([this](auto container) {
            received_containers_.push_back(container);
        });

        bridge_->start();
    }

    void TearDown() override {
        if (bridge_) {
            bridge_->stop();
        }
        bridge_.reset();
    }

    std::unique_ptr<integration::messaging_bridge> bridge_;
    std::vector<std::shared_ptr<container_system::value_container>> received_containers_;
};

TEST_F(ContainerIntegrationTest, ContainerSerialization) {
    auto server = bridge_->create_server("test_server");
    auto client = bridge_->create_client("test_client");

    ASSERT_TRUE(server->start_server(0)); // Random port
    ASSERT_TRUE(client->start_client("127.0.0.1", server->port()));

    // Create container message
    auto container = std::make_shared<container_system::value_container>();
    container->set_source("test_client", "main");
    container->set_target("test_server", "handler");
    container->set_message_type("test_message");

    auto values = std::vector<std::shared_ptr<container_system::value>>{
        container_system::value_factory::create("text",
            container_system::string_value, "Hello, Container!"),
        container_system::value_factory::create("number",
            container_system::int32_value, "42")
    };
    container->set_values(values);

    // Send container
    std::string serialized = container->serialize();
    client->send_message(serialized);

    // Wait for reception
    test_utils::wait_for_condition([this]() {
        return !received_containers_.empty();
    }, std::chrono::seconds(5));

    ASSERT_EQ(1, received_containers_.size());

    auto received = received_containers_[0];
    EXPECT_EQ("test_message", received->message_type());
    EXPECT_EQ("test_client", received->source_id());
    EXPECT_EQ("test_server", received->target_id());

    auto text_value = received->get_value("text");
    ASSERT_NE(nullptr, text_value);
    EXPECT_EQ("Hello, Container!", text_value->to_string());

    auto number_value = received->get_value("number");
    ASSERT_NE(nullptr, number_value);
    EXPECT_EQ(42, number_value->to_int32());
}

} // namespace network_system::test

#endif // BUILD_WITH_CONTAINER_SYSTEM
```
