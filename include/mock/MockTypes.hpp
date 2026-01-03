// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MockTypes_hpp
#define MockTypes_hpp

#include "LogGlobals.hpp" // DEBUG
#include <string>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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
            // Hash the float 'm' member. Combine with more fields if needed.
            return std::hash<float>()(e.m);
        }
    };
}

namespace eeng::ecs::mock
{
    struct MockMixComponent
    {
        float float_scalar = 1.0f;
        int int_scalar = 2;
        int int_scalar_2 = 3;
        bool bool_flag = true;
        MockVec3 position;
        std::string string_value = "Hello";
        std::array<int, 3> int_array3 = { 1, 2, 3 };
        std::vector<ElementType> element_vector = { {4.0f}, {5.0f}, {6.0f} };
        std::map<int, float> int_float_map = { {7, 7.5f}, {8, 8.5f} };
        std::map<int, ElementType> int_element_map = { {9, {9.5f}}, {10, {10.5f}} };
        std::map<ElementType, int> element_int_map = { {{9.5f}, 9}, {{10.5f}, 10} };
        std::set<int> int_set = { 11, 12 };
        AnEnum enum_value = AnEnum::Hello;

        // Nested containers
        std::vector<std::vector<int>> nested_int_vectors{ {1, 2, 3}, {4, 5, 6} };

        // Enum in containers
        std::vector<AnEnum> enum_vector{ AnEnum::Hello, AnEnum::Bye, AnEnum::Hola };
        std::map<AnEnum, int> enum_int_map{ { AnEnum::Hello, 10 }, { AnEnum::Bye,   20 } };

        glm::vec2 glm_vec2{ 1.0f, 2.0f };
        glm::vec3 glm_vec3{ 1.0f, 2.0f, 3.0f };
        glm::vec4 glm_vec4{ 1.0f, 2.0f, 3.0f, 4.0f };
        glm::ivec2 glm_ivec2{ 1, 2 };
        glm::ivec3 glm_ivec3{ 1, 2, 3 };
        glm::ivec4 glm_ivec4{ 1, 2, 3, 4 };
        glm::quat glm_quat{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::mat2 glm_mat2{ 1.0f };
        glm::mat3 glm_mat3{ 1.0f };
        glm::mat4 glm_mat4{ 1.0f };
        std::vector<glm::vec3> glm_vec3_vector{ {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f} };

        std::string to_string() const {
            std::ostringstream oss;
            oss << "MockMixComponent {\n"
                << "  float_scalar: " << float_scalar << "\n"
                << "  int_scalar: " << int_scalar << "\n"
                << "  int_scalar_2: " << int_scalar_2 << "\n"
                << "  bool_flag: " << std::boolalpha << bool_flag << "\n"
                << "  position: " << position.to_string() << "\n"
                << "  string_value: \"" << string_value << "\"\n"
                << "  int_array3: [";
            for (size_t i = 0; i < int_array3.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << int_array3[i];
            }
            oss << "]\n"
                << "  element_vector: [";
            for (size_t i = 0; i < element_vector.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << element_vector[i].to_string();
            }
            oss << "]\n"
                << "  int_float_map: {\n";
            for (auto it = int_float_map.begin(); it != int_float_map.end(); ++it) {
                oss << "    " << it->first << ": " << it->second << "\n";
            }
            oss << "  }\n"
                << "  int_element_map: {\n";
            for (auto it = int_element_map.begin(); it != int_element_map.end(); ++it) {
                oss << "    " << it->first << ": " << it->second.to_string() << "\n";
            }
            oss << "  }\n"
                << "  int_set: {";
            for (auto it = int_set.begin(); it != int_set.end(); ++it) {
                if (it != int_set.begin()) oss << ", ";
                oss << *it;
            }
            oss << "}\n}";
            return oss.str();
        }

        bool operator==(const MockMixComponent& rhs) const
        {
            return float_scalar == rhs.float_scalar &&
                int_scalar == rhs.int_scalar &&
                int_scalar_2 == rhs.int_scalar_2 &&
                bool_flag == rhs.bool_flag &&
                position == rhs.position &&
                string_value == rhs.string_value &&
                int_array3 == rhs.int_array3 &&
                element_vector == rhs.element_vector &&
                int_float_map == rhs.int_float_map &&
                int_element_map == rhs.int_element_map &&
                int_set == rhs.int_set;
        }
    };
} // namespace eeng::ecs::mock

#endif // MockComponents_hpp
