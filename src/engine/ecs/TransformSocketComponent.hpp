// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/Entity.hpp"

#include <cstdint>
#include <format>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eeng::ecs
{
    // Socket a transform to a target entity with a local offset.
    // Uses target's world transform + local offset/rotation to drive this transform.
    struct TransformSocketComponent
    {
        std::string name = "TransformSocket";
        bool enabled = true;

        // Target entity to follow.
        EntityRef target{};

        // Offset relative to target local space.
        glm::vec3 local_offset{ 0.0f };
        glm::quat local_rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

        bool follow_position = true;
        bool follow_rotation = true;
        bool preserve_scale = true;

        // Edit-mode behavior.
        bool update_in_edit = true;
        bool capture_offset_in_edit = true;

        // Runtime bookkeeping (not intended for authoring).
        std::uint32_t last_local_version = 0;
    };

    inline std::string to_string(const TransformSocketComponent& t)
    {
        return std::format("TransformSocketComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(TransformSocketComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(TransformSocketComponent& comp, Visitor&& visitor)
    {
        visitor(comp.target);
    }
} // namespace eeng::ecs
