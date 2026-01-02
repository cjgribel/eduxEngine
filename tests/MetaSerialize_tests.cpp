// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include <gtest/gtest.h>
// #include <gmock/gmock.h>
#include <vector>
#include <array>
#include <thread>
#include <atomic>
#include <optional>
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

namespace eeng {
    struct EngineContext;
}

namespace
{
    struct MockEntityManager : eeng::IEntityManager
    {
        MockEntityManager()
            : registry_(std::make_shared<entt::registry>()) {
        }
        ~MockEntityManager() override = default;

        bool entity_valid(const ecs::Entity& entity) const override
        {
            return registry_->valid(entity);
            // return false; 
        }

        ecs::Entity create_entity_unregistered(const ecs::Entity& /*hint*/) override
        {
            auto e = registry_->create();
            return ecs::Entity{ e };
        }

        std::pair<Guid, ecs::Entity> create_entity_live_parent(
            const std::string& chunk_tag,
            const std::string& name,
            const ecs::Entity& entity_parent,
            const ecs::Entity& entity_hint) override
        {
            return { Guid::invalid(), ecs::Entity{} };
        }

        bool entity_parent_registered(
            const ecs::Entity& entity) const override {
            return false;
        }

        void reparent_entity(
            const ecs::Entity& entity, const ecs::Entity& parent_entity) override {
        }

        std::optional<ecs::Entity> get_entity_from_guid(const Guid& guid) const override
        {
            return std::nullopt;
        }

        // void set_entity_parent(
        //     const ecs::Entity& entity,
        //     const ecs::Entity& entity_parent) override {
        // }

        void queue_entity_for_destruction(
            const ecs::Entity& entity) override {
        }

        int destroy_pending_entities() override { return 0; }

        entt::registry& registry() noexcept override { return *registry_; }
        const entt::registry& registry() const noexcept override { return *registry_; }
        std::weak_ptr<entt::registry> registry_wptr() noexcept override { return registry_; }
        std::weak_ptr<const entt::registry> registry_wptr() const noexcept override { return registry_; }

    private:
        void register_entity_live_parent(const ecs::Entity& entity) override {}

        void register_entities_from_deserialization(const std::vector<ecs::Entity>& entities) override {};

        std::shared_ptr<entt::registry> registry_;
    };

    struct MockResourceManager : eeng::IResourceManager
    {
#if 1
        AssetStatus get_status(const Guid& guid) const override { return AssetStatus{}; }
        std::shared_future<TaskResult> scan_assets_async(const std::filesystem::path& root, EngineContext& ctx) override { return std::async(std::launch::deferred, [] { return TaskResult{}; }).share(); }
        std::shared_future<TaskResult> load_and_bind_async(std::deque<Guid> branch_guids, const BatchId& batch, EngineContext& ctx) override { return std::async(std::launch::deferred, [] { return TaskResult{}; }).share(); }
        std::shared_future<TaskResult> unbind_and_unload_async(std::deque<Guid> branch_guids, const BatchId& batch, EngineContext& ctx) override { return std::async(std::launch::deferred, [] { return TaskResult{}; }).share(); }
        std::shared_future<TaskResult> reload_and_rebind_async(std::deque<Guid> branch_guids, const BatchId& batch, EngineContext& ctx) override { return std::async(std::launch::deferred, [] { return TaskResult{}; }).share(); }

        // void retain_guid(const Guid& guid) override {}
        // void release_guid(const Guid& guid, eeng::EngineContext& ctx) override {}
#endif

        bool is_busy() const override { return false; }
        void wait_until_idle() override {}
        int  queued_tasks()   const noexcept override { return 0; }

        AssetIndexDataPtr get_index_data() const override { return nullptr; }
        std::vector<Guid> find_guids_by_name(std::string_view name) const override { return {}; }
        std::string to_string() const override { return std::string{}; }
    };

    class MockBatchRegistry : public eeng::IBatchRegistry
    {

    };

    struct MockGuiManager : eeng::IGuiManager
    {
        void init() override {}
        void release() override {}

        void draw(EngineContext& ctx) const override {}

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

    class MockLogManager : public ILogManager
    {
    public:
        void log(const char* fmt, ...) override {}
        void clear() override {}
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
        const ecs::Entity& entity,
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

    struct PurposeFilterType
    {
        int a = 1;
        int b = 2;
        int c = 3;
        int d = 4;
        int e = 5;
    };

    struct PurposeAwareType
    {
        int value = 7;
    };

    const char* purpose_to_string(meta::SerializationPurpose purpose)
    {
        switch (purpose)
        {
        case meta::SerializationPurpose::generic:
            return "generic";
        case meta::SerializationPurpose::file:
            return "file";
        case meta::SerializationPurpose::undo:
            return "undo";
        case meta::SerializationPurpose::display:
            return "display";
        default:
            return "unknown";
        }
    }

    void serialize_purpose_aware(
        nlohmann::json& j,
        const entt::meta_any& any,
        meta::SerializationPurpose purpose)
    {
        auto ptr = any.try_cast<PurposeAwareType>();
        assert(ptr && "serialize_purpose_aware: could not cast meta_any to PurposeAwareType");
        j = {
            { "value", ptr->value },
            { "purpose", purpose_to_string(purpose) }
        };
    }

    void register_purpose_meta_types()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;

        entt::meta_factory<PurposeFilterType>()
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "PurposeFilterType", .name = "PurposeFilterType", .tooltip = "Purpose-filtered fields." })
            .data<&PurposeFilterType::a>("a"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "a", "a", "" })
            .traits(MetaFlags::none)
            .data<&PurposeFilterType::b>("b"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "b", "b", "" })
            .traits(MetaFlags::no_serialize_file)
            .data<&PurposeFilterType::c>("c"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "c", "c", "" })
            .traits(MetaFlags::no_serialize_undo)
            .data<&PurposeFilterType::d>("d"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "d", "d", "" })
            .traits(MetaFlags::no_serialize_display)
            .data<&PurposeFilterType::e>("e"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "e", "e", "" })
            .traits(MetaFlags::no_serialize)
            ;

        entt::meta_factory<PurposeAwareType>()
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "PurposeAwareType", .name = "PurposeAwareType", .tooltip = "Purpose-aware serializer." })
            .data<&PurposeAwareType::value>("value"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "value", "value", "" })
            .traits(MetaFlags::none)
            .template func<&serialize_purpose_aware, entt::as_void_t>(literals::serialize_hs)
            ;
    }

    template<typename T>
    void assure_type_storage(entt::registry& registry)
    {
        (void)registry.storage<T>();
    }
} // namespace

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
        std::make_unique<MockEntityManager>(),
        std::make_shared<MockResourceManager>(),
        std::make_unique<MockBatchRegistry>(),
        std::make_unique<MockGuiManager>(),
        std::make_unique<MockInputManager>(),
        std::make_unique<MockLogManager>()
    };

    static void SetUpTestSuite()
    {
        // Register vec2
        entt::meta_factory<vec2>()
            // .type("vec2"_hs)
            .custom<eeng::TypeMetaInfo>(eeng::TypeMetaInfo{ .id = "vec2", .name = "vec2", .tooltip = "A 2D vector type." })

            .data<&vec2::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "n/a" })
            .traits(MetaFlags::none)
            .data<&vec2::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "n/a" })
            .traits(MetaFlags::none)

            .template func<&serialize_vec2, entt::as_void_t>(literals::serialize_hs)
            .template func<&deserialize_vec2, entt::as_void_t >(literals::deserialize_hs)

            .template func<&assure_type_storage<vec2>, entt::as_void_t>(literals::assure_component_storage_hs)
            ;
        meta::register_type<vec2>(); // -> Can be used as a component directly
        // meta::type_id_map()["vec2"] = entt::resolve<vec2>().id();

        // Register vec3
        entt::meta_factory<vec3>()
            // .type("vec3"_hs)
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "vec3", .name = "vec3", .tooltip = "A 3D vector type." })

            .data<&vec3::xy>("xy"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "xy", "XY", "n/a" })
            .traits(MetaFlags::none)

            .data<&vec3::z>("z"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "z", "Z", "n/a" })
            .traits(MetaFlags::none)

            .template func<&assure_type_storage<vec3>, entt::as_void_t>(literals::assure_component_storage_hs)
            ;
        meta::register_type<vec3>(); // -> Can be used as a component directly
        // meta::type_id_map()["vec3"] = entt::resolve<vec3>().id();

        // Register MockType2::AnEnum
        auto enum_info = TypeMetaInfo{
            .id = "MockType2.AnEnum",
            .name = "AnEnum",
            .tooltip = "AnEnum is a test enum with three values.",
            .underlying_type = entt::resolve<std::underlying_type_t<MockType2::AnEnum>>()
        };
        entt::meta_factory<MockType2::AnEnum>()
            // .type("MockType2::AnEnum"_hs)
            .custom<TypeMetaInfo>(enum_info)

            .data<MockType2::AnEnum::Hello>("Hello"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Hello", "Greeting in English." })
            .traits(MetaFlags::none)

            .data<MockType2::AnEnum::Bye>("Bye"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Bye", "Farewell in English." })
            .traits(MetaFlags::none)

            .data<MockType2::AnEnum::Hola>("Hola"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Hola", "Greeting in Spanish." })
            .traits(MetaFlags::none)

            .template func<&assure_type_storage<MockType2::AnEnum>, entt::as_void_t>(literals::assure_component_storage_hs)
            ;

        // Register MockType2
        entt::meta_factory<MockType2>{}
        // .type("MockType2"_hs)
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "MockType2", .name = "MockType2", .tooltip = "A mock resource type." })
            .traits(MetaFlags::none)

            .data<&MockType2::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "Integer member x." })
            .traits(MetaFlags::readonly_inspection)

            .data<&MockType2::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Float member y." })
            .traits(MetaFlags::readonly_inspection | MetaFlags::no_inspection)

            .data<&MockType2::an_enum>("an_enum"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "an_enum", "AnEnum", "Enum member" })
            .traits(MetaFlags::readonly_inspection | MetaFlags::no_inspection)

            .template func<&assure_type_storage<MockType2>, entt::as_void_t>(literals::assure_component_storage_hs)
            ;
        meta::register_type<MockType2>(); // -> Can be used as a component directly
        // meta::type_id_map()["MockType2"] = entt::resolve<MockType2>().id();
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
        std::cout << get_meta_type_name(entt::resolve<T>()) << ":" << std::endl;
        std::cout << j.dump(4) << std::endl;
#endif
        // Deserialize
        entt::meta_any deserialized_any = T{};
        meta::deserialize_any(j, deserialized_any, ecs::Entity{}, ctx);
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

TEST_F(MetaSerializationTest, SerializePurposeFiltersFields)
{
    register_purpose_meta_types();
    PurposeFilterType t{};

    auto j_file = meta::serialize_any_for_file(t);
    EXPECT_TRUE(j_file.contains("a"));
    EXPECT_FALSE(j_file.contains("b"));
    EXPECT_TRUE(j_file.contains("c"));
    EXPECT_TRUE(j_file.contains("d"));
    EXPECT_FALSE(j_file.contains("e"));

    auto j_undo = meta::serialize_any_for_undo(t);
    EXPECT_TRUE(j_undo.contains("a"));
    EXPECT_TRUE(j_undo.contains("b"));
    EXPECT_FALSE(j_undo.contains("c"));
    EXPECT_TRUE(j_undo.contains("d"));
    EXPECT_FALSE(j_undo.contains("e"));

    auto j_display = meta::serialize_any_for_display(t);
    EXPECT_TRUE(j_display.contains("a"));
    EXPECT_TRUE(j_display.contains("b"));
    EXPECT_TRUE(j_display.contains("c"));
    EXPECT_FALSE(j_display.contains("d"));
    EXPECT_FALSE(j_display.contains("e"));

    auto j_generic = meta::serialize_any(t, meta::SerializationPurpose::generic);
    EXPECT_TRUE(j_generic.contains("a"));
    EXPECT_TRUE(j_generic.contains("b"));
    EXPECT_TRUE(j_generic.contains("c"));
    EXPECT_TRUE(j_generic.contains("d"));
    EXPECT_FALSE(j_generic.contains("e"));
}

TEST_F(MetaSerializationTest, SerializePurposeAwareCustomFunction)
{
    register_purpose_meta_types();
    PurposeAwareType t{};
    t.value = 11;

    auto j_file = meta::serialize_any(t, meta::SerializationPurpose::file);
    EXPECT_EQ(j_file["purpose"].get<std::string>(), "file");

    auto j_display = meta::serialize_any(t, meta::SerializationPurpose::display);
    EXPECT_EQ(j_display["purpose"].get<std::string>(), "display");
}

TEST_F(MetaSerializationTest, SerializePurposeFallbackToLegacySignature)
{
    vec2 v{ 1.0f, 2.0f };
    auto j = meta::serialize_any(v, meta::SerializationPurpose::file);
    EXPECT_FLOAT_EQ(j["x"].get<float>(), v.x);
    EXPECT_FLOAT_EQ(j["y"].get<float>(), v.y);
}

TEST_F(MetaSerializationTest, ConcurrentSerializeAny) {
    constexpr int THREAD_COUNT = 50;
    constexpr int ITERATIONS = 1000;

    // Prepare a shared vec2 and its meta_any wrapper
    vec2 v{ 1.23f, 4.56f };
    entt::meta_any any = v;

    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);

    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                try {
                    // Invoke the serialization on the same any
                    auto j = meta::serialize_any(any);
                    // Check we got back the correct fields
                    if (j["x"].get<float>() != v.x
                        || j["y"].get<float>() != v.y) {
                        error.store(true, std::memory_order_relaxed);
                        return;
                    }
                }
                catch (...) {
                    error.store(true, std::memory_order_relaxed);
                    return;
                }
            }
            });
    }

    for (auto& th : threads) th.join();
    EXPECT_FALSE(error.load());
}

TEST_F(MetaSerializationTest, ConcurrentDeserializeAny) {
    constexpr int THREAD_COUNT = 50;
    constexpr int ITERATIONS = 1000;

    // Prepare JSON and a fresh any wrapping a vec2
    nlohmann::json j = { {"x", 7.89f}, {"y", 0.12f} };
    vec2 v0{ 0, 0 };
    entt::meta_any any = v0;

    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);

    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                try {
                    meta::deserialize_any(j, any, ecs::Entity{}, ctx);
                    auto& vv = any.cast<vec2&>();
                    // All threads write the same values, so vv.x/y should always match j
                    if (vv.x != j["x"].get<float>()
                        || vv.y != j["y"].get<float>()) {
                        error.store(true, std::memory_order_relaxed);
                        return;
                    }
                }
                catch (...) {
                    error.store(true, std::memory_order_relaxed);
                    return;
                }
            }
            });
    }

    for (auto& th : threads) th.join();
    EXPECT_FALSE(error.load());
}

TEST(MetaReadOnly, ManyThreadsDifferentMetaAny) {
    struct Foo { int x; };
    entt::meta_factory<Foo>()
        .type("Foo"_hs)
        .ctor<int>()
        .data<&Foo::x>("x"_hs)
        .func<&Foo::x>("getX"_hs);

    constexpr int THREADS = 100;
    constexpr int ITERATIONS = 1000;

    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                Foo f{ i };
                auto any = entt::meta_any{ f };             // private to this thread
                auto type = entt::resolve<Foo>();         // read-only on registry
                auto fn = type.func("getX"_hs);
                int x = fn.invoke(any).cast<int>();
                if (x != i) error.store(true);
            }
            });
    }

    for (auto& th : threads) th.join();
    ASSERT_FALSE(error.load());
}

TEST_F(MetaSerializationTest, SerializeDeserialize_EntityRoundTrip)
{
    //
    // 1) Use the fixture context as SOURCE
    //
    auto* src_em = ctx.entity_manager.get();
    auto  src_reg_sp = src_em->registry_wptr().lock();
    ASSERT_TRUE(src_reg_sp);
    auto& src_reg = *src_reg_sp;

    // src_reg = entt::registry(); //.clear();
    // src_reg.storage<vec2>(entt::hashed_string::value("vec2"));
    // src_reg.storage<MockType2>(entt::hashed_string::value("MockType2"));

    // create an entity in the source registry
    Guid src_guid{ 0x12345678ULL };
    ecs::Entity src_entity = src_em->create_entity_unregistered(ecs::Entity::EntityNull);

    // attach components that were registered in SetUpTestSuite()
    {
        src_reg.emplace<vec2>(static_cast<entt::entity>(src_entity), vec2{ 1.0f, 2.5f });

        MockType2 mt;
        mt.x = 42;
        mt.y = 3.14f;
        mt.an_enum = MockType2::AnEnum::Hola;
        src_reg.emplace<MockType2>(static_cast<entt::entity>(src_entity), mt);
    }

    // wrap as EntityRef
    ecs::EntityRef src_entity_ref{ src_guid, src_entity };

    //
    // 2) serialize the entity from the SOURCE context
    //
    nlohmann::json json_entity = eeng::meta::serialize_entity(src_entity_ref, src_reg_sp);

    // debug print
    //std::cout << "Serialized entity JSON:\n" << json_entity.dump(4) << std::endl;

    //
    // 3) create a fresh DESTINATION context
    //    (same mocks as fixture, but with its own registry)
    //
    eeng::EngineContext dst_ctx{
        std::make_unique<MockEntityManager>(),
        std::make_unique<MockResourceManager>(),
        std::make_unique<MockBatchRegistry>(),
        std::make_unique<MockGuiManager>(),
        std::make_unique<MockInputManager>(),
        std::make_unique<MockLogManager>()
    };

    auto* dst_em = dst_ctx.entity_manager.get();
    auto  dst_reg_sp = dst_em->registry_wptr().lock();
    ASSERT_TRUE(dst_reg_sp);
    auto& dst_reg = *dst_reg_sp;

    //
    // 4) deserialize into the DESTINATION context
    //
    ecs::EntityRef dst_entity_ref = eeng::meta::deserialize_entity(json_entity, dst_ctx);

    // basic GUID check
    EXPECT_EQ(dst_entity_ref.guid.raw(), src_guid.raw());

    entt::entity dst_entt_entity = static_cast<entt::entity>(dst_entity_ref.entity);
    ASSERT_TRUE(dst_reg.valid(dst_entt_entity));

    //
    // 5) verify components were reconstructed
    //
    // vec2
    ASSERT_TRUE(dst_reg.any_of<vec2>(dst_entt_entity));
    const auto& v = dst_reg.get<vec2>(dst_entt_entity);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.5f);

    // MockType2
    ASSERT_TRUE(dst_reg.any_of<MockType2>(dst_entt_entity));
    const auto& m = dst_reg.get<MockType2>(dst_entt_entity);
    EXPECT_EQ(m.x, 42);
    EXPECT_FLOAT_EQ(m.y, 3.14f);
    EXPECT_EQ(m.an_enum, MockType2::AnEnum::Hola);
}
