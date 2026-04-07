// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "ecs/Entity.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace eeng::ecs
{
    struct PistonRigSpec
    {
        // Optional root entity (if unbound, a new root is created).
        EntityRef root{};

        // Naming / grouping.
        std::string name_prefix = "Piston";
        std::string chunk_tag = "piston_rig";

        // Root transform (world space).
        glm::vec3 position{ 0.0f, 1.0f, 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

        // Anchor positions in root local space.
        glm::vec3 anchor_local_a{ 0.0f, 0.0f, 0.0f };
        glm::vec3 anchor_local_b{ 1.0f, 0.0f, 0.0f };

        // Optional bodies to bind (for driving arbitrary entities).
        EntityRef body_a{};
        EntityRef body_b{};
        bool use_sockets = true;

        // Axis used by the drive component (root local space).
        glm::vec3 axis_local{ 1.0f, 0.0f, 0.0f };

        bool disable_collisions = true;

        // Optional animation clip override for PistonAnimSyncComponent.
        // Empty = use clip name from the AnimationGraph.
        std::string anim_clip_name{};

        // Drive defaults.
        float stroke_min = 0.0f;
        float stroke_max = 1.0f;
        float max_force = 2000.0f;
        float max_velocity = 1.0f;
        int mode = 0; // 0=Hold, 1=Extend, 2=Contract, 3=Position
        float target_extension = 0.0f;
        bool lock_when_idle = true;
    };

    struct PistonRig
    {
        EntityRef root{};
        EntityRef constraint{};
        EntityRef anchor_a{};
        EntityRef anchor_b{};
        EntityRef visual{};
    };

    // Build a piston rig in the live world.
    PistonRig build_piston_rig(EngineContext& ctx, const PistonRigSpec& spec);

    // Generate a prefab JSON (top-down entity array) for a piston rig using a scratch registry.
    nlohmann::json build_piston_rig_prefab_json(EngineContext& ctx, const PistonRigSpec& spec);
} // namespace eeng::ecs
