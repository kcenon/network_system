/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#include "kcenon/network/core/session_concept.h"
#include "kcenon/network/core/session_handle.h"
#include "kcenon/network/core/session_model.h"
#include "kcenon/network/core/session_traits.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::core;
using namespace kcenon::network;
using namespace std::chrono_literals;

/**
 * @file session_type_erasure_test.cpp
 * @brief Unit tests for Type Erasure infrastructure (Phase 1 of #522)
 *
 * Tests validate:
 * - session_concept interface contract
 * - session_model template wrapping
 * - session_handle value semantics
 * - Type recovery via as<T>()
 * - Activity tracking functionality
 * - SFINAE method detection
 */

// ============================================================================
// Test Session Types
// ============================================================================

namespace {

/**
 * @brief Minimal session for basic testing
 */
class minimal_session {
public:
    explicit minimal_session(std::string id = "minimal")
        : id_(std::move(id)) {}

    [[nodiscard]] auto id() const -> std::string_view { return id_; }
    [[nodiscard]] auto is_connected() const -> bool { return connected_; }

    auto close() -> void { connected_ = false; }

    [[nodiscard]] auto send(std::vector<uint8_t>&& /*data*/) -> VoidResult {
        if (!connected_) {
            return VoidResult::err(-1, "Not connected");
        }
        return VoidResult::ok(std::monostate{});
    }

private:
    std::string id_;
    bool connected_{true};
};

/**
 * @brief Session with stop_session() method (like messaging_session)
 */
class stoppable_session {
public:
    explicit stoppable_session(std::string id = "stoppable")
        : id_(std::move(id)) {}

    [[nodiscard]] auto server_id() const -> const std::string& { return id_; }
    [[nodiscard]] auto is_stopped() const -> bool { return stopped_; }

    auto stop_session() -> void { stopped_ = true; }

    auto send_packet(std::vector<uint8_t>&& /*data*/) -> void {
        // Legacy send method
    }

private:
    std::string id_;
    bool stopped_{false};
};

/**
 * @brief Session with custom data for type recovery testing
 */
class custom_data_session {
public:
    explicit custom_data_session(int custom_value)
        : custom_value_(custom_value) {}

    [[nodiscard]] auto id() const -> std::string_view { return "custom"; }
    [[nodiscard]] auto is_connected() const -> bool { return true; }
    auto close() -> void {}
    [[nodiscard]] auto send(std::vector<uint8_t>&&) -> VoidResult { return VoidResult::ok(std::monostate{}); }

    [[nodiscard]] auto get_custom_value() const -> int { return custom_value_; }
    auto set_custom_value(int value) -> void { custom_value_ = value; }

private:
    int custom_value_;
};

} // namespace

// ============================================================================
// Session Traits Specializations
// ============================================================================

namespace kcenon::network::core {

template <>
struct session_traits<minimal_session> {
    static constexpr bool has_activity_tracking = false;
    static constexpr bool stop_on_clear = false;
    static constexpr const char* id_prefix = "minimal_";
};

template <>
struct session_traits<stoppable_session> {
    static constexpr bool has_activity_tracking = true;
    static constexpr bool stop_on_clear = true;
    static constexpr const char* id_prefix = "stoppable_";
};

template <>
struct session_traits<custom_data_session> {
    static constexpr bool has_activity_tracking = true;
    static constexpr bool stop_on_clear = false;
    static constexpr const char* id_prefix = "custom_";
};

} // namespace kcenon::network::core

// ============================================================================
// session_model Tests
// ============================================================================

class SessionModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        minimal_ = std::make_shared<minimal_session>("test_minimal");
        stoppable_ = std::make_shared<stoppable_session>("test_stoppable");
        custom_ = std::make_shared<custom_data_session>(42);
    }

    std::shared_ptr<minimal_session> minimal_;
    std::shared_ptr<stoppable_session> stoppable_;
    std::shared_ptr<custom_data_session> custom_;
};

TEST_F(SessionModelTest, CreateFromMinimalSession) {
    auto model = make_session_model(minimal_);

    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->id(), "test_minimal");
    EXPECT_TRUE(model->is_connected());
    EXPECT_EQ(model->type(), typeid(minimal_session));
}

TEST_F(SessionModelTest, CreateFromStoppableSession) {
    auto model = make_session_model(stoppable_);

    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->id(), "test_stoppable");
    EXPECT_TRUE(model->is_connected());  // is_stopped() == false -> connected
    EXPECT_EQ(model->type(), typeid(stoppable_session));
}

TEST_F(SessionModelTest, CloseMinimalSession) {
    auto model = make_session_model(minimal_);

    EXPECT_TRUE(model->is_connected());
    model->close();
    EXPECT_FALSE(model->is_connected());
    EXPECT_FALSE(minimal_->is_connected());
}

TEST_F(SessionModelTest, StopStoppableSession) {
    auto model = make_session_model(stoppable_);

    EXPECT_TRUE(model->is_connected());
    model->stop();
    EXPECT_FALSE(model->is_connected());
    EXPECT_TRUE(stoppable_->is_stopped());
}

TEST_F(SessionModelTest, SendToMinimalSession) {
    auto model = make_session_model(minimal_);

    std::vector<uint8_t> data{1, 2, 3, 4};
    auto result = model->send(std::move(data));
    EXPECT_TRUE(result.is_ok());
}

TEST_F(SessionModelTest, SendToClosedSession) {
    auto model = make_session_model(minimal_);

    model->close();
    std::vector<uint8_t> data{1, 2, 3, 4};
    auto result = model->send(std::move(data));
    EXPECT_FALSE(result.is_ok());
}

TEST_F(SessionModelTest, GetRawPointer) {
    auto model = make_session_model(minimal_);

    void* raw = model->get_raw();
    EXPECT_EQ(raw, minimal_.get());

    const void* const_raw = std::as_const(*model).get_raw();
    EXPECT_EQ(const_raw, minimal_.get());
}

TEST_F(SessionModelTest, TypeInfoCorrect) {
    auto minimal_model = make_session_model(minimal_);
    auto stoppable_model = make_session_model(stoppable_);
    auto custom_model = make_session_model(custom_);

    EXPECT_EQ(minimal_model->type(), typeid(minimal_session));
    EXPECT_EQ(stoppable_model->type(), typeid(stoppable_session));
    EXPECT_EQ(custom_model->type(), typeid(custom_data_session));

    EXPECT_NE(minimal_model->type(), typeid(stoppable_session));
}

// ============================================================================
// Activity Tracking Tests
// ============================================================================

TEST_F(SessionModelTest, ActivityTrackingDisabled) {
    auto model = make_session_model(minimal_);

    EXPECT_FALSE(model->has_activity_tracking());
    EXPECT_EQ(model->idle_duration(), 0ms);
}

TEST_F(SessionModelTest, ActivityTrackingEnabled) {
    auto model = make_session_model(stoppable_);

    EXPECT_TRUE(model->has_activity_tracking());

    auto before = model->idle_duration();
    EXPECT_GE(before.count(), 0);

    std::this_thread::sleep_for(20ms);

    auto after = model->idle_duration();
    EXPECT_GT(after.count(), before.count());
}

TEST_F(SessionModelTest, UpdateActivity) {
    auto model = make_session_model(stoppable_);

    std::this_thread::sleep_for(30ms);
    auto before_update = model->idle_duration();
    EXPECT_GT(before_update.count(), 20);

    model->update_activity();

    auto after_update = model->idle_duration();
    EXPECT_LT(after_update.count(), 10);
}

TEST_F(SessionModelTest, CreatedAtTimestamp) {
    auto before = std::chrono::steady_clock::now();
    auto model = make_session_model(stoppable_);
    auto after = std::chrono::steady_clock::now();

    auto created = model->created_at();
    EXPECT_GE(created, before);
    EXPECT_LE(created, after);
}

// ============================================================================
// session_handle Tests
// ============================================================================

class SessionHandleTest : public ::testing::Test {
protected:
    void SetUp() override {
        minimal_ = std::make_shared<minimal_session>("handle_test");
        custom_ = std::make_shared<custom_data_session>(123);
    }

    std::shared_ptr<minimal_session> minimal_;
    std::shared_ptr<custom_data_session> custom_;
};

TEST_F(SessionHandleTest, CreateEmptyHandle) {
    session_handle handle;

    EXPECT_FALSE(handle);
    EXPECT_FALSE(handle.valid());
    EXPECT_TRUE(handle.id().empty());
    EXPECT_FALSE(handle.is_connected());
}

TEST_F(SessionHandleTest, CreateFromSession) {
    session_handle handle(minimal_);

    EXPECT_TRUE(handle);
    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle.id(), "handle_test");
    EXPECT_TRUE(handle.is_connected());
}

TEST_F(SessionHandleTest, MoveSemantics) {
    session_handle handle1(minimal_);
    EXPECT_TRUE(handle1.valid());

    session_handle handle2 = std::move(handle1);
    EXPECT_FALSE(handle1.valid());  // NOLINT(bugprone-use-after-move)
    EXPECT_TRUE(handle2.valid());
    EXPECT_EQ(handle2.id(), "handle_test");
}

TEST_F(SessionHandleTest, CloseSession) {
    session_handle handle(minimal_);

    EXPECT_TRUE(handle.is_connected());
    handle.close();
    EXPECT_FALSE(handle.is_connected());
}

TEST_F(SessionHandleTest, SendData) {
    session_handle handle(minimal_);

    std::vector<uint8_t> data{1, 2, 3};
    auto result = handle.send(std::move(data));
    EXPECT_TRUE(result.is_ok());
}

TEST_F(SessionHandleTest, SendToInvalidHandle) {
    session_handle handle;

    std::vector<uint8_t> data{1, 2, 3};
    auto result = handle.send(std::move(data));
    EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// Type Recovery Tests
// ============================================================================

TEST_F(SessionHandleTest, TypeRecoverySuccess) {
    session_handle handle(custom_);

    auto* recovered = handle.as<custom_data_session>();
    ASSERT_NE(recovered, nullptr);
    EXPECT_EQ(recovered->get_custom_value(), 123);
}

TEST_F(SessionHandleTest, TypeRecoveryFailure) {
    session_handle handle(minimal_);

    auto* wrong_type = handle.as<custom_data_session>();
    EXPECT_EQ(wrong_type, nullptr);
}

TEST_F(SessionHandleTest, ModifyThroughTypeRecovery) {
    session_handle handle(custom_);

    auto* recovered = handle.as<custom_data_session>();
    ASSERT_NE(recovered, nullptr);

    recovered->set_custom_value(456);
    EXPECT_EQ(custom_->get_custom_value(), 456);
}

TEST_F(SessionHandleTest, IsTypeCheck) {
    session_handle handle(minimal_);

    EXPECT_TRUE(handle.is_type<minimal_session>());
    EXPECT_FALSE(handle.is_type<custom_data_session>());
    EXPECT_FALSE(handle.is_type<stoppable_session>());
}

TEST_F(SessionHandleTest, TypeInfoFromHandle) {
    session_handle handle(custom_);
    EXPECT_EQ(handle.type(), typeid(custom_data_session));

    session_handle empty;
    EXPECT_EQ(empty.type(), typeid(void));
}

// ============================================================================
// Handle Activity Tracking Tests
// ============================================================================

TEST_F(SessionHandleTest, ActivityTrackingThroughHandle) {
    auto stoppable = std::make_shared<stoppable_session>();
    session_handle handle(stoppable);

    EXPECT_TRUE(handle.has_activity_tracking());

    std::this_thread::sleep_for(20ms);
    auto idle = handle.idle_duration();
    EXPECT_GE(idle.count(), 15);

    handle.update_activity();
    EXPECT_LT(handle.idle_duration().count(), 10);
}

TEST_F(SessionHandleTest, NoActivityTrackingThroughHandle) {
    session_handle handle(minimal_);

    EXPECT_FALSE(handle.has_activity_tracking());
    EXPECT_EQ(handle.idle_duration(), 0ms);
}

// ============================================================================
// Handle Release and Reset Tests
// ============================================================================

TEST_F(SessionHandleTest, ReleaseOwnership) {
    session_handle handle(minimal_);
    EXPECT_TRUE(handle.valid());

    auto released = handle.release();
    EXPECT_FALSE(handle.valid());
    EXPECT_NE(released, nullptr);
    EXPECT_EQ(released->id(), "handle_test");
}

TEST_F(SessionHandleTest, ResetHandle) {
    session_handle handle(minimal_);
    EXPECT_TRUE(handle.valid());

    handle.reset();
    EXPECT_FALSE(handle.valid());
}

// ============================================================================
// Factory Function Tests
// ============================================================================

TEST_F(SessionHandleTest, MakeSessionHandle) {
    auto handle = make_session_handle(minimal_);

    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle.id(), "handle_test");
}

TEST_F(SessionHandleTest, MakeSessionModel) {
    auto model = make_session_model(minimal_);

    EXPECT_NE(model, nullptr);
    EXPECT_EQ(model->id(), "handle_test");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SessionHandleTest, OperationsOnInvalidHandle) {
    session_handle handle;

    // All operations should be safe on invalid handle
    EXPECT_FALSE(handle.is_connected());
    EXPECT_TRUE(handle.id().empty());
    EXPECT_EQ(handle.idle_duration(), 0ms);
    EXPECT_FALSE(handle.has_activity_tracking());

    handle.close();  // Should not crash
    handle.stop();   // Should not crash
    handle.update_activity();  // Should not crash
}

TEST_F(SessionHandleTest, ConstAccess) {
    const session_handle handle(custom_);

    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle.id(), "custom");
    EXPECT_TRUE(handle.is_connected());

    const auto* recovered = handle.as<custom_data_session>();
    ASSERT_NE(recovered, nullptr);
    EXPECT_EQ(recovered->get_custom_value(), 123);
}
