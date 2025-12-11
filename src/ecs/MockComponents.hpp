#ifndef MockComponents_hpp
#define MockComponents_hpp

#include "ResourceTypes.hpp"
#include "Guid.h"
#include "LogGlobals.hpp" // DEBUG


#include "ecs/Entity.hpp"

// --- MockPlayerComponent -----------------------------------------------------
// --- MockCameraComponent -----------------------------------------------------

namespace eeng::ecs::mock
{
    struct MockPlayerComponent
    {
        float position{ 0.0f };
        float health{ 100.0f };
        ecs::EntityRef camera_ref;
        AssetRef<eeng::mock::Model> model_ref;
    };

    template<typename Visitor>
    void visit_asset_refs(MockPlayerComponent& t, Visitor&& visitor)
    {
        // LogGlobals::log("[visit_asset_refs<MockPlayerComponent>]");
        visitor(t.model_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(MockPlayerComponent& t, Visitor&& visitor)
    {
        visitor(t.camera_ref);
    }

    struct MockCameraComponent
    {
        float position{ 0.0f };
        float fov{ 60.0f };
        ecs::EntityRef target_ref;
        AssetRef<eeng::mock::Model> model_ref;
    };

    template<typename Visitor>
    void visit_asset_refs(MockCameraComponent& t, Visitor&& visitor)
    {
        // LogGlobals::log("[visit_asset_refs<MockCameraComponent>]");
        visitor(t.model_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(MockCameraComponent& t, Visitor&& visitor)
    {
        visitor(t.target_ref);
    }
} // namespace eeng::ecs::mock

// --- MockMixComponent --------------------------------------------------------

namespace eeng::ecs::mock
{
    // Nested types

    struct MockUVcoords
    {
        float u, v;

        bool operator==(const MockUVcoords& other) const {
            return u == other.u && v == other.v;
        }

        std::string to_string() const
        {
            std::ostringstream oss;
            oss << "MockUVcoords(u = " << u << ", v = " << v << ")";
            return oss.str();
        }
    };

    struct MockVec3
    {
        float x = 1, y = 2, z = 3;
        MockUVcoords uv_coords{ -1.0f, -2.0f };

        std::string to_string() const {
            std::ostringstream oss;
            oss << "MockVec3(" << x << ", " << y << ", " << z << ")";
            return oss.str();
        }

        bool operator==(const MockVec3& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    inline std::string MockVec3_to_string(const void* ptr)
    {
        return static_cast<const MockVec3*>(ptr)->to_string();
    }

    struct ElementType
    {
        float m;

        std::string to_string() const {
            std::ostringstream oss;
            oss << "ElementType(" << m << ")";
            return oss.str();
        }

        // For debugging
        bool operator==(const ElementType& other) const {
            return m == other.m;
        }

        // For std::map
        bool operator<(const ElementType& other) const {
            return m < other.m;
        }
    };

    enum class AnEnum : int { Hello = 5, Bye = 6, Hola = 8 };
} // namespace eeng::ecs::mock

// For unordered_map, in global namespace
namespace std {
    template <>
    struct hash<eeng::ecs::mock::ElementType> {
        std::size_t operator()(const eeng::ecs::mock::ElementType& e) const {
            // Hash the float 'm' member. You can combine this with more fields if needed.
            return std::hash<float>()(e.m);
        }
    };
}

namespace eeng::ecs::mock
{
    struct MockMixComponent
    {
        float a = 1; int b = 2, c = 3;
        bool flag = true;
        MockVec3 position;
        std::string somestring = "Hello";
        // std::vector<int> vector1 = { 1, 2, 3 };
        std::array<int, 3> vector1 = { 1, 2, 3 };
        std::vector<ElementType> vector2 = { {4.0f}, {5.0f}, {6.0f} };
        std::map<int, float> map1 = { {7, 7.5f}, {8, 8.5f} };
        std::map<int, ElementType> map2 = { {9, {9.5f}}, {10, {10.5f}} };
        std::map<ElementType, int> map3 = { {{9.5f}, 9}, {{10.5f}, 10} };
        std::set<int> set1 = { 11, 12 };
        //    enum class AnEnum: int { A = 1, B = 2 } anEnum;
        AnEnum anEnum = AnEnum::Hello;

        std::string to_string() const {
            std::ostringstream oss;
            oss << "MockMixComponent {\n"
                << "  a: " << a << "\n"
                << "  b: " << b << "\n"
                << "  c: " << c << "\n"
                << "  flag: " << std::boolalpha << flag << "\n"
                << "  position: " << position.to_string() << "\n"
                << "  somestring: \"" << somestring << "\"\n"
                << "  vector1: [";
            for (size_t i = 0; i < vector1.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << vector1[i];
            }
            oss << "]\n"
                << "  vector2: [";
            for (size_t i = 0; i < vector2.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << vector2[i].to_string();
            }
            oss << "]\n"
                << "  map1: {\n";
            for (auto it = map1.begin(); it != map1.end(); ++it) {
                oss << "    " << it->first << ": " << it->second << "\n";
            }
            oss << "  }\n"
                << "  map2: {\n";
            for (auto it = map2.begin(); it != map2.end(); ++it) {
                oss << "    " << it->first << ": " << it->second.to_string() << "\n";
            }
            oss << "  }\n"
                << "  set1: {";
            for (auto it = set1.begin(); it != set1.end(); ++it) {
                if (it != set1.begin()) oss << ", ";
                oss << *it;
            }
            oss << "}\n}";
            return oss.str();
        }

        bool operator==(const MockMixComponent& rhs) const
        {
            return a == rhs.a &&
                b == rhs.b &&
                c == rhs.c &&
                flag == rhs.flag &&
                position == rhs.position &&
                somestring == rhs.somestring &&
                vector1 == rhs.vector1 &&
                vector2 == rhs.vector2 &&
                map1 == rhs.map1 &&
                map2 == rhs.map2 &&
                set1 == rhs.set1;
        }
    };
} // namespace eeng::ecs::mock

#endif // MockComponents_hpp
