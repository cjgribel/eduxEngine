// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <format>
#include <string>

#include "ecs/Entity.hpp"

namespace eeng::ecs
{
    // Minimal script component placeholder.
    struct ScriptComponent
    {
        std::string script_id;
        bool enabled = true;
    };

    inline std::string to_string(const ScriptComponent& t)
    {
        return std::format("ScriptComponent(script_id = {}, enabled = {})",
            t.script_id,
            t.enabled ? "true" : "false");
    }

    // ScriptComponent does not reference assets/entities yet.
    template<typename Visitor>
    void visit_asset_refs(ScriptComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(ScriptComponent&, Visitor&&) {}
} // namespace eeng::ecs
