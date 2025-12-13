// MetaFieldAssignTests.cpp
// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include <gtest/gtest.h>
#include <entt/entt.hpp>

#include "mock/MockTypes.hpp"
#include "editor/MetaFieldPath.hpp"
#include "editor/MetaFieldAssign.hpp"

using namespace eeng;
using namespace eeng::ecs::mock;

namespace eeng::editor
{
    struct MetaFieldPathBuilder
    {
        MetaFieldPath path;

        MetaFieldPathBuilder& data(const char* field_name)
        {
            MetaFieldPath::Entry entry{};
            entry.type = MetaFieldPath::Entry::Type::Data;
            entry.data_id = entt::hashed_string{ field_name }.value();
            entry.name = field_name;
            path.entries.push_back(std::move(entry));
            return *this;
        }

        MetaFieldPathBuilder& index(int idx)
        {
            MetaFieldPath::Entry entry{};
            entry.type = MetaFieldPath::Entry::Type::Index;
            entry.index = idx;
            entry.name = "[" + std::to_string(idx) + "]";
            path.entries.push_back(std::move(entry));
            return *this;
        }

        template<typename Key>
        MetaFieldPathBuilder& key(const Key& key, const std::string& label = {})
        {
            MetaFieldPath::Entry entry{};
            entry.type = MetaFieldPath::Entry::Type::Key;
            entry.key_any = entt::meta_any{ key };
            entry.name = !label.empty() ? label : std::string{ "key" };
            path.entries.push_back(std::move(entry));
            return *this;
        }

        MetaFieldPath build() const
        {
            return path;
        }
    };
} // namespace eeng::editor

// Convenience wrapper so tests don't have to pass idx = 0 everywhere
static bool assign_field(entt::meta_any& root,
    const eeng::editor::MetaFieldPath& path,
    const entt::meta_any& value)
{
    return eeng::editor::assign_meta_field_recursive(root, path, 0, value);
}

// -----------------------------------------------------------------------------
// Test fixture: registers meta for the mock types
// -----------------------------------------------------------------------------

class MetaFieldAssignTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        using namespace entt::literals;
        using namespace eeng::ecs::mock;

        entt::meta_factory<ElementType>()
            .type("ElementType"_hs)
            .data<&ElementType::m>("m"_hs);

        entt::meta_factory<MockUVcoords>()
            .type("MockUVcoords"_hs)
            .data<&MockUVcoords::u>("u"_hs)
            .data<&MockUVcoords::v>("v"_hs);

        entt::meta_factory<MockVec3>()
            .type("MockVec3"_hs)
            .data<&MockVec3::x>("x"_hs)
            .data<&MockVec3::y>("y"_hs)
            .data<&MockVec3::z>("z"_hs)
            .data<&MockVec3::uv_coords>("uv_coords"_hs);

        entt::meta_factory<MockMixComponent>()
            .type("MockMixComponent"_hs)
            .data<&MockMixComponent::a>("a"_hs)
            .data<&MockMixComponent::b>("b"_hs)
            .data<&MockMixComponent::c>("c"_hs)
            .data<&MockMixComponent::flag>("flag"_hs)
            .data<&MockMixComponent::position>("position"_hs)
            .data<&MockMixComponent::somestring>("somestring"_hs)
            .data<&MockMixComponent::vector1>("vector1"_hs)
            .data<&MockMixComponent::vector2>("vector2"_hs)
            .data<&MockMixComponent::map1>("map1"_hs)
            .data<&MockMixComponent::map2>("map2"_hs)
            .data<&MockMixComponent::map3>("map3"_hs)
            .data<&MockMixComponent::set1>("set1"_hs)
            .data<&MockMixComponent::anEnum>("anEnum"_hs);
    }
};

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

TEST_F(MetaFieldAssignTest, Assign_SimpleField_A)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    obj.a = 1.0f;

    // Make root an alias to obj (mirrors registry usage)
    entt::meta_any root = entt::forward_as_meta(obj); // ref policy

    auto path =
        MetaFieldPathBuilder{}
        .data("a")
        .build();

    entt::meta_any new_val = 42.0f;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    EXPECT_FLOAT_EQ(obj.a, 42.0f);
}

TEST_F(MetaFieldAssignTest, Assign_Nested_PositionUVcoordsU)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    obj.position.uv_coords.u = -1.0f;

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("position")
        .data("uv_coords")
        .data("u")
        .build();

    entt::meta_any new_val = 123.0f;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    EXPECT_FLOAT_EQ(obj.position.uv_coords.u, 123.0f);
}

TEST_F(MetaFieldAssignTest, Assign_ArrayElement_Vector1_Index)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    // vector1 = {1, 2, 3}
    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("vector1")
        .index(2)   // third element
        .build();

    entt::meta_any new_val = 999;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    ASSERT_EQ(obj.vector1.size(), 3u);
    EXPECT_EQ(obj.vector1[0], 1);
    EXPECT_EQ(obj.vector1[1], 2);
    EXPECT_EQ(obj.vector1[2], 999);
}

TEST_F(MetaFieldAssignTest, Assign_VectorElement_Field_M)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    obj.vector2 = { {4.0f}, {5.0f}, {6.0f} };

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("vector2")
        .index(1)      // second ElementType
        .data("m")     // its 'm' field
        .build();

    entt::meta_any new_val = 99.0f;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    ASSERT_EQ(obj.vector2.size(), 3u);
    EXPECT_FLOAT_EQ(obj.vector2[0].m, 4.0f);
    EXPECT_FLOAT_EQ(obj.vector2[1].m, 99.0f);
    EXPECT_FLOAT_EQ(obj.vector2[2].m, 6.0f);
}

TEST_F(MetaFieldAssignTest, Assign_Map1_ValueByKey)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    // map1: std::map<int, float> = { {7, 7.5f}, {8, 8.5f} }

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("map1")
        .key(7, "7")
        .build();   // leaf is the mapped float itself

    entt::meta_any new_val = 123.25f;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    auto it7 = obj.map1.find(7);
    auto it8 = obj.map1.find(8);
    ASSERT_NE(it7, obj.map1.end());
    ASSERT_NE(it8, obj.map1.end());

    EXPECT_FLOAT_EQ(it7->second, 123.25f);
    EXPECT_FLOAT_EQ(it8->second, 8.5f);
}

TEST_F(MetaFieldAssignTest, Assign_Map2_ElementField_M)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    // map2: std::map<int, ElementType> = { {9, {9.5f}}, {10, {10.5f}} }

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("map2")
        .key(9, "9")
        .data("m")
        .build();

    entt::meta_any new_val = 777.0f;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    auto it9 = obj.map2.find(9);
    auto it10 = obj.map2.find(10);
    ASSERT_NE(it9, obj.map2.end());
    ASSERT_NE(it10, obj.map2.end());

    EXPECT_FLOAT_EQ(it9->second.m, 777.0f);
    EXPECT_FLOAT_EQ(it10->second.m, 10.5f);
}
#if 0
#endif