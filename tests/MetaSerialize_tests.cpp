#include <gtest/gtest.h>
#include "entt/entt.hpp"
#include "MetaSerialize.hpp"
// #include "MetaLiterals.h"
// #include "MetaInfo.h"
// #include "MetaAux.h"
// #include "EngineContext.h"

using namespace eeng;

struct MockEntityRegistry : eeng::IEntityRegistry
{
    Entity create_entity() override { return Entity{ }; }
    void destroy_entity(Entity entity) override {}
};

struct MockResourceRegistry : eeng::IRes
{
};


// struct UVcoords
// {
//     float u, v;

//     bool operator==(const UVcoords& other) const {
//         return u == other.u && v == other.v;
//     }

//     std::string to_string() const 
//     {
//         std::ostringstream oss;
//         oss << "UVcoords(u = " << u << ", v = " << v << ")";
//         return oss.str();
//     }
// };

// struct debugvec3
// {
//     float x = 1, y = 2, z = 3;
//     UVcoords uv_coords{ -1.0f, -2.0f };

//     std::string to_string() const {
//         std::ostringstream oss;
//         oss << "debugvec3(" << x << ", " << y << ", " << z << ")";
//         return oss.str();
//     }

//     bool operator==(const debugvec3& other) const {
//         return x == other.x && y == other.y && z == other.z;
//     }
// };

// Free function test
// inline std::string debugvec3_to_string(const void* ptr)
// {
//     return static_cast<const debugvec3*>(ptr)->to_string();
// }

// struct ElementType
// {
//     float m;

//     std::string to_string() const {
//         std::ostringstream oss;
//         oss << "ElementType(" << m << ")";
//         return oss.str();
//     }

//     // For debugging
//     bool operator==(const ElementType& other) const {
//         return m == other.m;
//     }

//     // For std::map
//     bool operator<(const ElementType& other) const {
//         return m < other.m;
//     }
// };

struct MockType
{
    int x{ 2 };
    float y{ 3.0f };

    // TODO
    // bool flag = true;
    // debugvec3 position;
    // std::string somestring = "Hello";
    // // std::vector<int> vector1 = { 1, 2, 3 };
    // std::array<int, 3> vector1 = { 1, 2, 3 };
    // std::vector<ElementType> vector2 = { {4.0f}, {5.0f}, {6.0f} };
    // std::map<int, float> map1 = { {7, 7.5f}, {8, 8.5f} };
    // std::map<int, ElementType> map2 = { {9, {9.5f}}, {10, {10.5f}} };
    // std::map<ElementType, int> map3 = { {{9.5f}, 9}, {{10.5f}, 10} };
    // std::set<int> set1 = { 11, 12 };

    enum class AnEnum : int { Hello = 5, Bye = 6, Hola = 8 } an_enum{ AnEnum::Hello };

    // Function with value, reference, const reference, pointer, const pointer
    // int mutate_and_sum(int a, float& b, const float& c, double* d, const double* e) const
    // {
    //     b += 1.5f;        // mutate non-const ref
    //     *d += 2.5;        // mutate non-const pointer
    //     return a + static_cast<int>(b) + static_cast<int>(*d) + static_cast<int>(c) + static_cast<int>(*e);
    // }
};

// Print policy of entt::any_policy
inline std::string policy_to_string(entt::any_policy policy)
{
    switch (policy) {
    case entt::any_policy::embedded: return "embedded";
    case entt::any_policy::ref: return "ref";
    case entt::any_policy::cref: return "cref";
    case entt::any_policy::dynamic: return "dynamic";
    default: return "unknown";
    }
}

// Test Fixture
class MetaRegistrationTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        // Register MockType::AnEnum
        auto enum_info = EnumMetaInfo{
            .display_name = "AnEnum",
            .tooltip = "AnEnum is a test enum with three values.",
            .underlying_type = entt::resolve<std::underlying_type_t<MockType::AnEnum>>()
        };
        entt::meta_factory<MockType::AnEnum>()
            .type("AnEnum"_hs)
            .custom<EnumMetaInfo>(enum_info)

            .data<MockType::AnEnum::Hello>("Hello"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Hello", "Greeting in English." })
            .traits(MetaFlags::none)

            .data<MockType::AnEnum::Bye>("Bye"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Bye", "Farewell in English." })
            .traits(MetaFlags::none)

            .data<MockType::AnEnum::Hola>("Hola"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Hola", "Greeting in Spanish." })
            .traits(MetaFlags::none);

        // Register MockType
        entt::meta_factory<MockType>{}
        .type("MockType"_hs)
            .custom<TypeMetaInfo>(TypeMetaInfo{ "MockType", "A mock resource type." })
            .traits(MetaFlags::none)

            .data<&MockType::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "Integer member x." })
            .traits(MetaFlags::read_only)

            .data<&MockType::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Float member y." })
            .traits(MetaFlags::read_only | MetaFlags::hidden)

            .func<&MockType::mutate_and_sum>("mutate_and_sum"_hs)
            .custom<FuncMetaInfo>(FuncMetaInfo{ "mutate_and_sum", "Mutates ref and ptr args, and sums them." })
            .traits(MetaFlags::none);
    }
};

// TEST_F(MetaSerializationTest, Deserialize_WithMockContext)
// {
//     MockEntityRegistry entity_registry_mock;
//     MockScriptSystem script_system_mock;

//     eeng::EngineContext ctx{ &entity_registry_mock, &script_system_mock };

//     entt::meta_any deserialized;
//     Entity dummy_entity{ 123 };

//     deserialize_any(json_obj, deserialized, dummy_entity, ctx);

//     // Verify deserialization logic easily
// }