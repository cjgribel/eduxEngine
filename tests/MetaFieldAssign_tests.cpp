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
            .data<&ElementType::m>("m"_hs)
            ;

        entt::meta_factory<MockUVcoords>()
            .type("MockUVcoords"_hs)
            .data<&MockUVcoords::u>("u"_hs)
            .data<&MockUVcoords::v>("v"_hs)
            ;

        entt::meta_factory<MockVec3>()
            .type("MockVec3"_hs)
            .data<&MockVec3::x>("x"_hs)
            .data<&MockVec3::y>("y"_hs)
            .data<&MockVec3::z>("z"_hs)
            .data<&MockVec3::uv_coords>("uv_coords"_hs)
            ;

        entt::meta_factory<AnEnum>()
            //.type("AnEnum"_hs)
            // Fields not needed - only used as key 
            ;

        entt::meta_factory<MockMixComponent>()
            .type("MockMixComponent"_hs)
            .data<&MockMixComponent::float_scalar>("float_scalar"_hs)
            .data<&MockMixComponent::int_scalar>("int_scalar"_hs)
            .data<&MockMixComponent::int_scalar_2>("int_scalar_2"_hs)
            .data<&MockMixComponent::bool_flag>("bool_flag"_hs)
            .data<&MockMixComponent::position>("position"_hs)
            .data<&MockMixComponent::string_value>("string_value"_hs)
            .data<&MockMixComponent::int_array3>("int_array3"_hs)
            .data<&MockMixComponent::element_vector>("element_vector"_hs)
            .data<&MockMixComponent::int_float_map>("int_float_map"_hs)
            .data<&MockMixComponent::int_element_map>("int_element_map"_hs)
            .data<&MockMixComponent::element_int_map>("element_int_map"_hs)
            .data<&MockMixComponent::int_set>("int_set"_hs)
            .data<&MockMixComponent::enum_value>("enum_value"_hs)
            .data<&MockMixComponent::nested_int_vectors>("nested_int_vectors"_hs)
            .data<&MockMixComponent::enum_vector>("enum_vector"_hs)
            .data<&MockMixComponent::enum_int_map>("enum_int_map"_hs)
            .data<&MockMixComponent::glm_vec2>("glm_vec2"_hs)
            .data<&MockMixComponent::glm_vec3>("glm_vec3"_hs)
            .data<&MockMixComponent::glm_vec4>("glm_vec4"_hs)
            .data<&MockMixComponent::glm_ivec2>("glm_ivec2"_hs)
            .data<&MockMixComponent::glm_ivec3>("glm_ivec3"_hs)
            .data<&MockMixComponent::glm_ivec4>("glm_ivec4"_hs)
            .data<&MockMixComponent::glm_quat>("glm_quat"_hs)
            .data<&MockMixComponent::glm_mat2>("glm_mat2"_hs)
            .data<&MockMixComponent::glm_mat3>("glm_mat3"_hs)
            .data<&MockMixComponent::glm_mat4>("glm_mat4"_hs)
            .data<&MockMixComponent::glm_vec3_vector>("glm_vec3_vector"_hs)
            ;
    }
};

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

TEST_F(MetaFieldAssignTest, Assign_SimpleField_A)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    obj.float_scalar = 1.0f;

    // Make root an alias to obj (mirrors registry usage)
    entt::meta_any root = entt::forward_as_meta(obj); // ref policy

    auto path =
        MetaFieldPathBuilder{}
        .data("float_scalar")
        .build();

    entt::meta_any new_val = 42.0f;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    EXPECT_FLOAT_EQ(obj.float_scalar, 42.0f);
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

TEST_F(MetaFieldAssignTest, Assign_ArrayElement_IntArray3_Index)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    // int_array3 = {1, 2, 3}
    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("int_array3")
        .index(2)   // third element
        .build();

    entt::meta_any new_val = 999;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    ASSERT_EQ(obj.int_array3.size(), 3u);
    EXPECT_EQ(obj.int_array3[0], 1);
    EXPECT_EQ(obj.int_array3[1], 2);
    EXPECT_EQ(obj.int_array3[2], 999);
}

TEST_F(MetaFieldAssignTest, Assign_VectorElement_Field_M)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    obj.element_vector = { {4.0f}, {5.0f}, {6.0f} };

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("element_vector")
        .index(1)      // second ElementType
        .data("m")     // its 'm' field
        .build();

    entt::meta_any new_val = 99.0f;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    ASSERT_EQ(obj.element_vector.size(), 3u);
    EXPECT_FLOAT_EQ(obj.element_vector[0].m, 4.0f);
    EXPECT_FLOAT_EQ(obj.element_vector[1].m, 99.0f);
    EXPECT_FLOAT_EQ(obj.element_vector[2].m, 6.0f);
}

TEST_F(MetaFieldAssignTest, Assign_Map1_ValueByKey)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    // int_float_map: std::map<int, float> = { {7, 7.5f}, {8, 8.5f} }

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("int_float_map")
        .key(7, "7")
        .build();   // leaf is the mapped float itself

    entt::meta_any new_val = 123.25f;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    auto it7 = obj.int_float_map.find(7);
    auto it8 = obj.int_float_map.find(8);
    ASSERT_NE(it7, obj.int_float_map.end());
    ASSERT_NE(it8, obj.int_float_map.end());

    EXPECT_FLOAT_EQ(it7->second, 123.25f);
    EXPECT_FLOAT_EQ(it8->second, 8.5f);
}

TEST_F(MetaFieldAssignTest, Assign_Map2_ElementField_M)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    // int_element_map: std::map<int, ElementType> = { {9, {9.5f}}, {10, {10.5f}} }

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("int_element_map")
        .key(9, "9")
        .data("m")
        .build();

    entt::meta_any new_val = 777.0f;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    auto it9 = obj.int_element_map.find(9);
    auto it10 = obj.int_element_map.find(10);
    ASSERT_NE(it9, obj.int_element_map.end());
    ASSERT_NE(it10, obj.int_element_map.end());

    EXPECT_FLOAT_EQ(it9->second.m, 777.0f);
    EXPECT_FLOAT_EQ(it10->second.m, 10.5f);
}

TEST_F(MetaFieldAssignTest, Assign_NestedVector_Element)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    // nested_int_vectors = { {1,2,3}, {4,5,6} };

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("nested_int_vectors")
        .index(1)   // second vector {4,5,6}
        .index(2)   // third element in that vector (value 6)
        .build();

    entt::meta_any new_val = 999;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    ASSERT_EQ(obj.nested_int_vectors.size(), 2u);
    ASSERT_EQ(obj.nested_int_vectors[1].size(), 3u);
    EXPECT_EQ(obj.nested_int_vectors[0][0], 1);
    EXPECT_EQ(obj.nested_int_vectors[0][1], 2);
    EXPECT_EQ(obj.nested_int_vectors[0][2], 3);
    EXPECT_EQ(obj.nested_int_vectors[1][0], 4);
    EXPECT_EQ(obj.nested_int_vectors[1][1], 5);
    EXPECT_EQ(obj.nested_int_vectors[1][2], 999);
}

TEST_F(MetaFieldAssignTest, Assign_EnumVector_Element)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    // enum_vector = { Hello, Bye, Hola };

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("enum_vector")
        .index(1)   // second element
        .build();   // leaf is the enum value itself

    entt::meta_any new_val = AnEnum::Hola;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    ASSERT_EQ(obj.enum_vector.size(), 3u);
    EXPECT_EQ(obj.enum_vector[0], AnEnum::Hello);
    EXPECT_EQ(obj.enum_vector[1], AnEnum::Hola);   // changed
    EXPECT_EQ(obj.enum_vector[2], AnEnum::Hola);
}

TEST_F(MetaFieldAssignTest, Assign_EnumKeyedMap_Value)
{
    using eeng::editor::MetaFieldPathBuilder;

    MockMixComponent obj{};
    // enum_int_map = { {Hello,10}, {Bye,20} };

    entt::meta_any root = entt::forward_as_meta(obj);

    auto path =
        MetaFieldPathBuilder{}
        .data("enum_int_map")
        .key(AnEnum::Bye, "Bye")
        .build();  // leaf is the mapped int

    entt::meta_any new_val = 123;

    bool ok = assign_field(root, path, new_val);
    ASSERT_TRUE(ok);

    auto it_hello = obj.enum_int_map.find(AnEnum::Hello);
    auto it_bye = obj.enum_int_map.find(AnEnum::Bye);
    ASSERT_NE(it_hello, obj.enum_int_map.end());
    ASSERT_NE(it_bye, obj.enum_int_map.end());

    EXPECT_EQ(it_hello->second, 10);
    EXPECT_EQ(it_bye->second, 123);
}
