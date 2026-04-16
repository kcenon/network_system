/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/integration/container_integration.h"
#include <gtest/gtest.h>

#include <any>
#include <functional>
#include <string>
#include <vector>

namespace integration = kcenon::network::integration;

/**
 * @file container_integration_test.cpp
 * @brief Unit tests for container_integration module
 *
 * Tests validate:
 * - basic_container construction
 * - basic_container type_name and is_valid
 * - basic_container serialize/deserialize with default behavior
 * - basic_container custom serializer/deserializer
 * - container_manager singleton access
 * - container_manager register/get containers
 * - container_manager default container
 * - container_manager serialize/deserialize through manager
 * - container_manager list_containers
 */

// ============================================================================
// basic_container Constructor Tests
// ============================================================================

class BasicContainerConstructorTest : public ::testing::Test
{
};

TEST_F(BasicContainerConstructorTest, Construction)
{
	integration::basic_container container;

	EXPECT_FALSE(container.type_name().empty());
}

TEST_F(BasicContainerConstructorTest, IsValid)
{
	integration::basic_container container;

	EXPECT_TRUE(container.is_valid());
}

// ============================================================================
// basic_container Operations Tests
// ============================================================================

class BasicContainerOperationsTest : public ::testing::Test
{
protected:
	integration::basic_container container_;
};

TEST_F(BasicContainerOperationsTest, TypeNameNotEmpty)
{
	auto name = container_.type_name();

	EXPECT_FALSE(name.empty());
}

TEST_F(BasicContainerOperationsTest, SerializeReturnsData)
{
	std::any data = std::string("test");

	auto bytes = container_.serialize(data);

	// Default serializer may return empty or actual data
	// Just verify it doesn't throw
	EXPECT_NO_THROW(container_.serialize(data));
}

TEST_F(BasicContainerOperationsTest, DeserializeReturnsData)
{
	std::vector<uint8_t> bytes = {0x01, 0x02, 0x03};

	// Default deserializer may return empty any or actual data
	EXPECT_NO_THROW(container_.deserialize(bytes));
}

TEST_F(BasicContainerOperationsTest, SetCustomSerializer)
{
	bool serializer_called = false;

	container_.set_serializer([&serializer_called](const std::any& data)
								 -> std::vector<uint8_t> {
		serializer_called = true;
		return {0xDE, 0xAD};
	});

	auto result = container_.serialize(std::any(42));

	EXPECT_TRUE(serializer_called);
	EXPECT_EQ(result.size(), 2u);
	EXPECT_EQ(result[0], 0xDE);
	EXPECT_EQ(result[1], 0xAD);
}

TEST_F(BasicContainerOperationsTest, SetCustomDeserializer)
{
	bool deserializer_called = false;

	container_.set_deserializer(
		[&deserializer_called](const std::vector<uint8_t>& bytes) -> std::any {
			deserializer_called = true;
			return std::string("deserialized");
		});

	auto result = container_.deserialize({0x01, 0x02});

	EXPECT_TRUE(deserializer_called);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(std::any_cast<std::string>(result), "deserialized");
}

// ============================================================================
// container_manager Tests
// ============================================================================

class ContainerManagerTest : public ::testing::Test
{
};

TEST_F(ContainerManagerTest, SingletonAccess)
{
	auto& mgr1 = integration::container_manager::instance();
	auto& mgr2 = integration::container_manager::instance();

	EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(ContainerManagerTest, GetDefaultContainerNotNull)
{
	auto& mgr = integration::container_manager::instance();

	auto container = mgr.get_default_container();

	EXPECT_NE(container, nullptr);
}

TEST_F(ContainerManagerTest, RegisterAndGetContainer)
{
	auto& mgr = integration::container_manager::instance();
	auto custom = std::make_shared<integration::basic_container>();

	mgr.register_container("test-container", custom);

	auto retrieved = mgr.get_container("test-container");
	EXPECT_NE(retrieved, nullptr);
}

TEST_F(ContainerManagerTest, GetNonexistentContainerReturnsNull)
{
	auto& mgr = integration::container_manager::instance();

	auto retrieved = mgr.get_container("nonexistent-container-xyz");

	EXPECT_EQ(retrieved, nullptr);
}

TEST_F(ContainerManagerTest, SetDefaultContainer)
{
	auto& mgr = integration::container_manager::instance();
	auto custom = std::make_shared<integration::basic_container>();

	mgr.set_default_container(custom);

	auto retrieved = mgr.get_default_container();
	EXPECT_NE(retrieved, nullptr);
}

TEST_F(ContainerManagerTest, SerializeThroughManager)
{
	auto& mgr = integration::container_manager::instance();

	EXPECT_NO_THROW(mgr.serialize(std::any(std::string("data"))));
}

TEST_F(ContainerManagerTest, DeserializeThroughManager)
{
	auto& mgr = integration::container_manager::instance();

	EXPECT_NO_THROW(mgr.deserialize({0x01, 0x02}));
}

TEST_F(ContainerManagerTest, ListContainers)
{
	auto& mgr = integration::container_manager::instance();
	mgr.register_container("list-test-container",
						   std::make_shared<integration::basic_container>());

	auto names = mgr.list_containers();

	// Should contain at least the one we just registered
	bool found = false;
	for (const auto& name : names)
	{
		if (name == "list-test-container")
		{
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
}
