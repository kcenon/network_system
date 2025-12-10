/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Network System Project
 *
 * @file concepts_example.cpp
 * @brief Demonstrates usage of C++20 concepts in network_system
 *
 * This example shows how to use network_system concepts for:
 * - Compile-time type validation
 * - Better error messages
 * - Self-documenting template interfaces
 *
 * Requirements:
 * - C++20 compiler (GCC 10+, Clang 10+, MSVC 2022+)
 */

#include <kcenon/network/concepts/concepts.h>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

using namespace network_system::concepts;

// ============================================================================
// Example 1: Using ByteBuffer concept
// ============================================================================

/**
 * @brief Send data from any buffer type satisfying ByteBuffer concept
 * @tparam Buffer Must satisfy ByteBuffer (has data() and size())
 */
template<ByteBuffer Buffer>
void send_buffer(const Buffer& buffer) {
    std::cout << "Sending " << buffer.size() << " bytes from buffer\n";
    // Access is compile-time guaranteed
    const void* ptr = buffer.data();
    (void)ptr;  // Suppress unused warning in example
}

void demonstrate_byte_buffer() {
    std::cout << "=== ByteBuffer Concept ===\n";

    // Works with std::vector<uint8_t>
    std::vector<uint8_t> vec_buffer = {1, 2, 3, 4, 5};
    send_buffer(vec_buffer);

    // Works with std::string
    std::string str_buffer = "Hello, Network!";
    send_buffer(str_buffer);

    // Works with std::array
    std::array<uint8_t, 4> arr_buffer = {0xDE, 0xAD, 0xBE, 0xEF};
    send_buffer(arr_buffer);

    // The following would NOT compile (good!):
    // int x = 42;
    // send_buffer(x);  // Error: 'int' does not satisfy 'ByteBuffer'

    std::cout << "\n";
}

// ============================================================================
// Example 2: Using MutableByteBuffer concept
// ============================================================================

/**
 * @brief Receive data into a resizable buffer
 * @tparam Buffer Must satisfy MutableByteBuffer (ByteBuffer + resize)
 */
template<MutableByteBuffer Buffer>
void receive_into(Buffer& buffer, std::size_t expected_size) {
    buffer.resize(expected_size);
    std::cout << "Buffer resized to " << buffer.size() << " bytes\n";
    // Fill with dummy data
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        static_cast<uint8_t*>(buffer.data())[i] = static_cast<uint8_t>(i % 256);
    }
}

void demonstrate_mutable_buffer() {
    std::cout << "=== MutableByteBuffer Concept ===\n";

    std::vector<uint8_t> buffer;
    receive_into(buffer, 100);
    std::cout << "Received data, first byte: " << static_cast<int>(buffer[0]) << "\n";

    // std::array does NOT satisfy MutableByteBuffer (no resize)
    // std::array<uint8_t, 10> arr;
    // receive_into(arr, 5);  // Error: no resize() method

    std::cout << "\n";
}

// ============================================================================
// Example 3: Using callback concepts
// ============================================================================

/**
 * @brief A client class demonstrating concept-constrained callbacks
 */
class ConceptClient {
public:
    /**
     * @brief Set data receive handler
     * @tparam Handler Must be callable with const std::vector<uint8_t>&
     */
    template<DataReceiveHandler Handler>
    void on_data(Handler&& handler) {
        data_handler_ = std::forward<Handler>(handler);
        std::cout << "Data handler registered\n";
    }

    /**
     * @brief Set error handler
     * @tparam Handler Must be callable with std::error_code
     */
    template<ErrorHandler Handler>
    void on_error(Handler&& handler) {
        error_handler_ = std::forward<Handler>(handler);
        std::cout << "Error handler registered\n";
    }

    /**
     * @brief Set connection handler
     * @tparam Handler Must be callable with no arguments
     */
    template<ConnectionHandler Handler>
    void on_connect(Handler&& handler) {
        connect_handler_ = std::forward<Handler>(handler);
        std::cout << "Connect handler registered\n";
    }

    /**
     * @brief Set retry callback
     * @tparam Handler Must be callable with std::size_t (attempt number)
     */
    template<RetryCallback Handler>
    void on_retry(Handler&& handler) {
        retry_handler_ = std::forward<Handler>(handler);
        std::cout << "Retry handler registered\n";
    }

    // Simulate events for demonstration
    void simulate_events() {
        std::cout << "\nSimulating events:\n";

        if (connect_handler_) {
            std::cout << "  -> Connection event: ";
            connect_handler_();
        }

        if (data_handler_) {
            std::vector<uint8_t> data = {72, 101, 108, 108, 111};  // "Hello"
            std::cout << "  -> Data event: ";
            data_handler_(data);
        }

        if (retry_handler_) {
            std::cout << "  -> Retry event: ";
            retry_handler_(3);
        }

        if (error_handler_) {
            std::cout << "  -> Error event: ";
            error_handler_(std::make_error_code(std::errc::connection_refused));
        }
    }

private:
    std::function<void(const std::vector<uint8_t>&)> data_handler_;
    std::function<void(std::error_code)> error_handler_;
    std::function<void()> connect_handler_;
    std::function<void(std::size_t)> retry_handler_;
};

void demonstrate_callbacks() {
    std::cout << "=== Callback Concepts ===\n";

    ConceptClient client;

    // Lambda callbacks - all types are validated at compile time
    client.on_data([](const std::vector<uint8_t>& data) {
        std::cout << "Received " << data.size() << " bytes\n";
    });

    client.on_error([](std::error_code ec) {
        std::cout << "Error: " << ec.message() << "\n";
    });

    client.on_connect([]() {
        std::cout << "Connected!\n";
    });

    client.on_retry([](std::size_t attempt) {
        std::cout << "Retry attempt #" << attempt << "\n";
    });

    client.simulate_events();

    // Invalid callbacks would fail at compile time:
    // client.on_data([](int x) {});           // Wrong parameter type
    // client.on_error([](std::string s) {});  // Wrong parameter type
    // client.on_connect([](int x) {});        // Handler should take no args

    std::cout << "\n";
}

// ============================================================================
// Example 4: Using NetworkClient concept
// ============================================================================

/**
 * @brief A mock client satisfying NetworkClient concept
 */
class MockNetworkClient {
public:
    bool is_connected() const { return connected_; }

    void send_packet(std::vector<uint8_t> data) {
        std::cout << "Mock: sent " << data.size() << " bytes\n";
    }

    void stop_client() {
        connected_ = false;
        std::cout << "Mock: client stopped\n";
    }

    void connect() {
        connected_ = true;
        std::cout << "Mock: client connected\n";
    }

private:
    bool connected_ = false;
};

/**
 * @brief Generic function that works with any NetworkClient
 * @tparam Client Must satisfy NetworkClient concept
 */
template<NetworkClient Client>
void use_client(Client& client, const std::string& message) {
    if (!client.is_connected()) {
        std::cout << "Client not connected, cannot send\n";
        return;
    }

    std::vector<uint8_t> data(message.begin(), message.end());
    client.send_packet(std::move(data));
}

void demonstrate_network_client() {
    std::cout << "=== NetworkClient Concept ===\n";

    MockNetworkClient client;

    // Try to send before connecting
    use_client(client, "Hello");

    // Connect and send
    client.connect();
    use_client(client, "Hello, World!");

    // Stop client
    client.stop_client();

    std::cout << "\n";
}

// ============================================================================
// Example 5: Using DataTransformer concept
// ============================================================================

/**
 * @brief A simple XOR "encryption" transformer
 */
class XorTransformer {
public:
    explicit XorTransformer(uint8_t key) : key_(key) {}

    bool transform(std::vector<uint8_t>& data) {
        for (auto& byte : data) {
            byte ^= key_;
        }
        return true;
    }

    bool reverse_transform(std::vector<uint8_t>& data) {
        // XOR is its own inverse
        return transform(data);
    }

private:
    uint8_t key_;
};

/**
 * @brief Apply a reversible transformer to data
 * @tparam T Must satisfy ReversibleDataTransformer
 */
template<ReversibleDataTransformer T>
void process_bidirectional(T& transformer, std::vector<uint8_t>& data) {
    std::cout << "Original: ";
    for (auto b : data) std::cout << static_cast<char>(b);
    std::cout << "\n";

    transformer.transform(data);
    std::cout << "Transformed: ";
    for (auto b : data) std::cout << std::hex << static_cast<int>(b) << " ";
    std::cout << std::dec << "\n";

    transformer.reverse_transform(data);
    std::cout << "Restored: ";
    for (auto b : data) std::cout << static_cast<char>(b);
    std::cout << "\n";
}

void demonstrate_transformer() {
    std::cout << "=== DataTransformer Concept ===\n";

    XorTransformer xor_transform(0x42);
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};

    process_bidirectional(xor_transform, data);

    std::cout << "\n";
}

// ============================================================================
// Example 6: Using Duration concept
// ============================================================================

/**
 * @brief Set a timeout using any duration type
 * @tparam D Must satisfy Duration concept
 */
template<Duration D>
void set_timeout(D duration) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    std::cout << "Timeout set to " << ms.count() << " milliseconds\n";
}

void demonstrate_duration() {
    std::cout << "=== Duration Concept ===\n";

    set_timeout(std::chrono::seconds(30));
    set_timeout(std::chrono::milliseconds(500));
    set_timeout(std::chrono::microseconds(100000));

    // Invalid: raw integers don't satisfy Duration
    // set_timeout(5);  // Error: 'int' does not satisfy 'Duration'

    std::cout << "\n";
}

// ============================================================================
// Example 7: Compile-time error demonstration (commented out)
// ============================================================================

/*
 * Uncomment any of these to see the improved error messages with concepts:
 *
 * void demonstrate_compile_errors() {
 *     // Error: 'int' does not satisfy 'ByteBuffer'
 *     // because it lacks data() and size() methods
 *     send_buffer(42);
 *
 *     // Error: 'std::array<uint8_t, 10>' does not satisfy 'MutableByteBuffer'
 *     // because it lacks resize() method
 *     std::array<uint8_t, 10> arr;
 *     receive_into(arr, 5);
 *
 *     // Error: lambda does not satisfy 'DataReceiveHandler'
 *     // because it's not invocable with const std::vector<uint8_t>&
 *     ConceptClient client;
 *     client.on_data([](int x) {});
 *
 *     // Error: 'int' does not satisfy 'Duration'
 *     // because it lacks rep and period type members
 *     set_timeout(100);
 * }
 */

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "==============================================\n";
    std::cout << "Network System - C++20 Concepts Example\n";
    std::cout << "==============================================\n\n";

    demonstrate_byte_buffer();
    demonstrate_mutable_buffer();
    demonstrate_callbacks();
    demonstrate_network_client();
    demonstrate_transformer();
    demonstrate_duration();

    std::cout << "==============================================\n";
    std::cout << "All examples completed successfully!\n";
    std::cout << "==============================================\n";

    return 0;
}
