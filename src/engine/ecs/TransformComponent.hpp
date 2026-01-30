// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef TransformComponent_hpp
#define TransformComponent_hpp

#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eeng::ecs
{
    struct TransformComponent
    {
        // Local (authoritative, serialized)
        glm::vec3 position{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f };

        // Runtime cache (derived)
        glm::mat4 local_matrix{ 1.0f };
        glm::mat4 world_matrix{ 1.0f };
        glm::quat world_rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::mat3 world_rotation_matrix{ 1.0f };

        // Version counters for cheap dirty propagation during traversal.
        std::uint32_t local_version{ 1 };
        std::uint32_t local_matrix_version{ 0 };
        std::uint32_t world_version{ 0 };
        std::uint32_t parent_world_version{ 0 };

        void mark_local_dirty() noexcept
        {
            ++local_version;
            if (local_version == 0)
                ++local_version;
        }

        void set_position(const glm::vec3& value) noexcept
        {
            position = value;
            mark_local_dirty();
        }

        void set_rotation(const glm::quat& value) noexcept
        {
            rotation = value;
            mark_local_dirty();
        }

        void set_scale(const glm::vec3& value) noexcept
        {
            scale = value;
            mark_local_dirty();
        }
    };

    std::string to_string(const TransformComponent& t);

    template<typename Visitor> 
    void visit_asset_refs(TransformComponent& t, Visitor&& visitor) {}

    template<typename Visitor> 
    void visit_entity_refs(TransformComponent& t, Visitor&& visitor) {}

#if 0

    // -> bool inspect(Transform& t, Editor::InspectorState& inspector)
    // Maybe needs #include "editor/InspectType.hpp" + InspectorState

    // + const (e.g. when used as key) ?
    bool inspect_Transform(void* ptr, Editor::InspectorState& inspector)
    {
        Transform* t = static_cast<Transform*>(ptr);
        bool mod = false;

        inspector.begin_leaf("position");
        mod |= Editor::inspect_type(t->position, inspector);
        inspector.end_leaf();

        inspector.begin_leaf("rotation");
        mod |= Editor::inspect_type(t->rotation, inspector);
        inspector.end_leaf();

        inspector.begin_leaf("scale");
        mod |= Editor::inspect_type(t->scale, inspector);
        inspector.end_leaf();

        return mod;
}
#endif
}

#endif // TransformComponent_hpp
