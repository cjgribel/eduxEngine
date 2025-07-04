// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include <gtest/gtest.h>
// #include <gmock/gmock.h>
#include <vector>
#include <array>
#include "entt/entt.hpp"
#include "hash_combine.h"
#include "MetaInfo.h"
#include "MetaSerialize.hpp"
#include "MetaLiterals.h"
#include "MetaAux.h"
#include "EngineContext.hpp"

using namespace eeng;

// namespace eeng
// {
//     class MockInputManager : public IInputManager
//     {
//     public:
//         MOCK_METHOD(bool, IsKeyPressed, (Key key), (const, override));
//         MOCK_METHOD(bool, IsMouseButtonDown, (int button), (const, override));
//         MOCK_METHOD(const MouseState&, GetMouseState, (), (const, override));
//         MOCK_METHOD(const ControllerState&, GetControllerState, (int controllerIndex), (const, override));
//         MOCK_METHOD(int, GetConnectedControllerCount, (), (const, override));
//         MOCK_METHOD(const ControllerMap&, GetControllers, (), (const, override));
//         MOCK_METHOD(void, HandleEvent, (const void* event), (override));
//         MOCK_METHOD(void, Update, (), (override));
//     };
// }

namespace
{
    struct MockEntityRegistry : eeng::IEntityManager
    {
        Entity create_entity() override { return Entity{ }; }
        void destroy_entity(Entity entity) override {}
    };

    struct MockResourceManager : eeng::IResourceManager
    {
    };

    struct MockGuiManager : eeng::IGuiManager
    {
        void init() override {}
        void release() override {}

        void draw_engine_info(EngineContext& ctx) const override {}

        void set_flag(GuiFlags flag, bool enabled) override {}
        bool is_flag_enabled(GuiFlags flag) const override { return false; }
    };

    class MockInputManager final : public IInputManager
    {
    public:
        bool IsKeyPressed(Key key) const override { return false; }
        bool IsMouseButtonDown(int button) const override { return false; }
        const MouseState& GetMouseState() const override { return m_state; }
        const ControllerState& GetControllerState(int controllerIndex) const override { return c_state; }
        int GetConnectedControllerCount() const override { return 0; }
        const ControllerMap& GetControllers() const override { return c_map; }

    private:
        MouseState m_state;
        ControllerState c_state;
        ControllerMap c_map;
    };

    struct vec2
    {
        float x, y;

        bool operator==(const vec2& other) const { return x == other.x && y == other.y; }
        bool operator<(const vec2& other) const { return x < other.x; }

        std::string to_string() const
        {
            std::ostringstream oss;
            oss << "vec2(x = " << x << ", y = " << y << ")";
            return oss.str();
        }
    };

    void serialize_vec2(
        nlohmann::json& j,
        const entt::meta_any& any)
    {
        auto ptr = any.try_cast<vec2>();
        assert(ptr && "serialize_vec2: could not cast meta_any to vec2");
        j =
        {
            {"x", ptr->x},
            {"y", ptr->y}
        };
    }

    void deserialize_vec2(
        const nlohmann::json& j,
        entt::meta_any& any,
        const Entity& entity,
        eeng::EngineContext& ctx
    )
    {
        auto ptr = any.try_cast<vec2>();
        assert(ptr && "serialize_vec2: could not cast meta_any to vec2");
        ptr->x = j["x"];
        ptr->y = j["y"];
    }

    struct vec3
    {
        vec2 xy;
        float z;

        bool operator==(const vec3& other) const { return xy == other.xy && z == other.z; }
        bool operator<(const vec3& other) const { return xy.x < other.xy.x; }

        std::string to_string() const {
            std::ostringstream oss;
            oss << "vec3(" << xy.to_string() << ", z = " << z << ")";
            return oss.str();
        }
    };
}

namespace std
{
    template<>
    struct hash<vec2>
    {
        size_t operator()(vec2 const& m) const noexcept
        {
            return ::hash_combine(m.x, m.y);
        }
    };

    template<>
    struct hash<vec3>
    {
        size_t operator()(vec3 const& m) const noexcept
        {
            return ::hash_combine(m.xy, m.z);
        }
    };
}

struct MockType2
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

    bool operator==(const MockType2& other) const
    {
        return
            x == other.x &&
            y == other.y;
    }
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
class MetaSerializationTest : public ::testing::Test
{
protected:
    eeng::EngineContext ctx{
        std::make_unique<MockEntityRegistry>(),
        std::make_unique<MockResourceManager>(),
        std::make_unique<MockGuiManager>(),
        std::make_unique<MockInputManager>()
    };

    static void SetUpTestSuite()
    {
        // Register vec2
        entt::meta_factory<vec2>()
            .type("vec2"_hs)

            .data<&vec2::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "n/a" })
            .traits(MetaFlags::none)
            .data<&vec2::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "n/a" })
            .traits(MetaFlags::none)

            .template func<&serialize_vec2, entt::as_void_t>(literals::serialize_hs)
            .template func<&deserialize_vec2, entt::as_void_t >(literals::deserialize_hs)
            ;

        // Register vec3
        entt::meta_factory<vec3>()
            .type("vec3"_hs)

            .data<&vec3::xy>("xy"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "xy", "n/a" })
            .traits(MetaFlags::none)

            .data<&vec3::z>("z"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "z", "n/a" })
            .traits(MetaFlags::none)
            ;

        // Register MockType2::AnEnum
        auto enum_info = EnumMetaInfo{
            .display_name = "AnEnum",
            .tooltip = "AnEnum is a test enum with three values.",
            .underlying_type = entt::resolve<std::underlying_type_t<MockType2::AnEnum>>()
        };
        entt::meta_factory<MockType2::AnEnum>()
            .type("MockType2::AnEnum"_hs)
            .custom<EnumMetaInfo>(enum_info)

            .data<MockType2::AnEnum::Hello>("Hello"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Hello", "Greeting in English." })
            .traits(MetaFlags::none)

            .data<MockType2::AnEnum::Bye>("Bye"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Bye", "Farewell in English." })
            .traits(MetaFlags::none)

            .data<MockType2::AnEnum::Hola>("Hola"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Hola", "Greeting in Spanish." })
            .traits(MetaFlags::none);

        // Register MockType2
        entt::meta_factory<MockType2>{}
        .type("MockType2"_hs)
            .custom<TypeMetaInfo>(TypeMetaInfo{ "MockType2", "A mock resource type." })
            .traits(MetaFlags::none)

            .data<&MockType2::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "Integer member x." })
            .traits(MetaFlags::read_only)

            .data<&MockType2::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Float member y." })
            .traits(MetaFlags::read_only | MetaFlags::hidden)

            .data<&MockType2::an_enum>("an_enum"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "an_enum", "Enum member" })
            .traits(MetaFlags::read_only | MetaFlags::hidden)
            ;
    }
};

namespace
{
    template<class T>
    std::pair<nlohmann::json, T> test_type(const T& t, EngineContext& ctx)
    {
        // Serialize
        auto j = meta::serialize_any(t);

#if 0
        // Debug print json
        std::cout << meta_type_name(entt::resolve<T>()) << ":" << std::endl;
        std::cout << j.dump(4) << std::endl;
#endif
        // Deserialize
        entt::meta_any deserialized_any = T{};
        meta::deserialize_any(j, deserialized_any, Entity{}, ctx);
        T& deserialized_ref = deserialized_any.cast<T&>();

        // Compare
        EXPECT_EQ(t, deserialized_ref);

        return { j, deserialized_ref };
    }
}

TEST_F(MetaSerializationTest, SerializePrimitiveTypes)
{
    test_type(42, ctx);                    // int
    test_type(-42, ctx);                   // negative int
    test_type(42u, ctx);                   // unsigned int
    test_type(3.14f, ctx);                 // float
    test_type(-3.14f, ctx);                // negative float
    test_type(2.718, ctx);                 // double
    test_type(-2.718, ctx);                // negative double
    test_type(true, ctx);                  // bool (true)
    test_type(false, ctx);                 // bool (false)
    test_type('x', ctx);                   // char
}

TEST_F(MetaSerializationTest, SerializeString)
{
    test_type(std::string("hello"), ctx);
}

TEST_F(MetaSerializationTest, SerializeVectorOfInt)
{
    test_type(std::vector<int>{ 0, 1, 2, 3, 4, 5 }, ctx);
}

TEST_F(MetaSerializationTest, SerializeEnum)
{
    test_type(MockType2::AnEnum::Hello, ctx);
}

TEST_F(MetaSerializationTest, SerializeVec2WithCustomFunctions)
{
    test_type(vec2{ 1.0f, 2.0f }, ctx);
}

TEST_F(MetaSerializationTest, SerializeVec3)
{
    test_type(vec3{ vec2{1.0f, 2.0f}, 3.0f }, ctx);
}

TEST_F(MetaSerializationTest, SerializeVectorOfVec3)
{
    std::vector<vec3> t{
        { {1.0f, 2.0f}, 3.0f },
        { {10.0f, 20.0f}, 30.0f }
    };
    test_type(t, ctx);
}

TEST_F(MetaSerializationTest, SerializeArrayOfVec3)
{
    std::array<vec3, 2> t{ {
        { {1.0f, 2.0f}, 3.0f },
        { {10.0f, 20.0f}, 30.0f }
    } };
    test_type(t, ctx);
}

TEST_F(MetaSerializationTest, SerializeSetOfInt)
{
    test_type(std::set<int>{ 1, 2, 3, 4, 5 }, ctx);
}

TEST_F(MetaSerializationTest, SerializeSetOfVec3)
{
    std::set<vec3> t{
        { {1.0f, 2.0f}, 3.0f },
        { {5.0f, 6.0f}, 7.0f }
    };
    test_type(t, ctx);
}

TEST_F(MetaSerializationTest, SerializeMapOfIntToFloat)
{
    std::map<int, float> t{ {1, 2.0f}, {3, 4.0f} };
    test_type(t, ctx);
}

TEST_F(MetaSerializationTest, SerializeMapOfIntToVec3)
{
    std::map<int, vec3> t{
        { 1, { {2.0f, 3.0f}, 4.0f } },
        { 5, { {6.0f, 7.0f}, 8.0f } }
    };
    test_type(t, ctx);
}

TEST_F(MetaSerializationTest, SerializeMapOfVec3ToInt)
{
    std::map<vec3, int> t{
        { { {1.0f, 2.0f}, 3.0f }, 4 },
        { { {5.0f, 6.0f}, 7.0f }, 8 }
    };
    test_type(t, ctx);
}

TEST_F(MetaSerializationTest, SerializeMockType2)
{
    MockType2 t;
    test_type(t, ctx);
}