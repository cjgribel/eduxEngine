// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/Entity.hpp"

#include <format>
#include <string>

namespace eeng::ecs
{
    // Drives an AnimationGraph pose time from a piston extension value.
    struct PistonAnimSyncComponent
    {
        std::string name = "PistonAnimSync";
        bool enabled = true;

        // Entity that owns AnimationGraphComponent + ModelComponent.
        EntityRef target{};

        // Clip to scrub (normalized extension -> clip time). Empty = use graph clip.
        std::string clip_name = "";

        // If target is unbound, try to find a child with AnimationGraphComponent.
        bool auto_find_target = true;
    };

    inline std::string to_string(const PistonAnimSyncComponent& t)
    {
        return std::format("PistonAnimSyncComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(PistonAnimSyncComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(PistonAnimSyncComponent& comp, Visitor&& visitor)
    {
        visitor(comp.target);
    }
} // namespace eeng::ecs
