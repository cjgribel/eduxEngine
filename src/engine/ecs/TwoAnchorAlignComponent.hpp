// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/Entity.hpp"

#include <format>
#include <string>

#include <glm/glm.hpp>

namespace eeng::ecs
{
    // Align a target transform between two anchor entities.
    //
    // Policy:
    // - Aligns position and/or rotation each frame.
    // - Uses a reference up vector to avoid flipping when the anchor vector crosses singularities.
    // - Local axes define how the target's model is oriented (forward/up).
    struct TwoAnchorAlignComponent
    {
        std::string name = "TwoAnchorAlign";
        bool enabled = true;

        // Anchor entities providing world-space endpoints.
        EntityRef anchor_a{};
        EntityRef anchor_b{};

        // Target entity to align (defaults to the entity that owns this component).
        EntityRef target{};

        // Optional reference entity for the up vector (e.g., the base body).
        EntityRef up_reference{};
        glm::vec3 up_axis_ref{ 0.0f, 1.0f, 0.0f }; // Local-space axis on up_reference.

        // Target local axes (model basis) to align with the anchor vector.
        glm::vec3 local_forward_axis{ 1.0f, 0.0f, 0.0f };
        glm::vec3 local_up_axis{ 0.0f, 1.0f, 0.0f };

        // Position mode: 0 = anchor A, 1 = anchor B, 2 = midpoint.
        int position_mode = 0;
        float position_offset = 0.0f; // Offset along the forward direction (world units).

        bool align_position = true;
        bool align_rotation = true;
        bool preserve_scale = true;

        // Runtime feedback.
        float current_length = 0.0f;
    };

    inline std::string to_string(const TwoAnchorAlignComponent& t)
    {
        return std::format("TwoAnchorAlignComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(TwoAnchorAlignComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(TwoAnchorAlignComponent& comp, Visitor&& visitor)
    {
        visitor(comp.anchor_a);
        visitor(comp.anchor_b);
        visitor(comp.target);
        visitor(comp.up_reference);
    }
} // namespace eeng::ecs
