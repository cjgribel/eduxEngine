// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/PistonRigBuilder.hpp"

#include "LogMacros.h"
#include "engineapi/EngineContextHelpers.hpp"
#include "ecs/EntityManager.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/PistonConstraintDriveComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"
#include "ecs/TransformSocketComponent.hpp"
#include "ecs/TwoAnchorAlignComponent.hpp"
#include "meta/EntityMetaHelpers.hpp"
#include "meta/MetaSerialize.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>
namespace eeng::ecs
{
    namespace
    {
        bool has_ref_target(const ecs::EntityRef& ref)
        {
            return ref.is_bound() || ref.guid.valid();
        }

        glm::vec3 normalize_or_default(const glm::vec3& v, const glm::vec3& fallback)
        {
            const float len2 = glm::dot(v, v);
            if (len2 <= 1e-8f)
                return fallback;
            return v / std::sqrt(len2);
        }

        ecs::EntityRef create_entity(
            EntityManager& em,
            const std::string& chunk_tag,
            const std::string& name,
            const ecs::Entity& parent)
        {
            auto [guid, entity] = em.create_entity_live_parent(
                chunk_tag,
                name,
                parent,
                ecs::Entity::EntityNull);
            return ecs::EntityRef{ guid, entity };
        }

        void ensure_transform(
            entt::registry& registry,
            const ecs::Entity& entity,
            const glm::vec3& position,
            const glm::quat& rotation)
        {
            if (registry.all_of<ecs::TransformComponent>(entity))
                return;
            auto& tfm = registry.emplace<ecs::TransformComponent>(entity);
            tfm.set_position(position);
            tfm.set_rotation(rotation);
        }
    } // namespace

    PistonRig build_piston_rig(EngineContext& ctx, const PistonRigSpec& spec)
    {
        PistonRig rig{};

        auto* em = eeng::try_get_entity_manager_ptr(ctx, "PistonRigBuilder");
        if (!em)
            return rig;

        auto& registry = em->registry();
        const std::string prefix = spec.name_prefix.empty() ? "Piston" : spec.name_prefix;
        const std::string chunk_tag = spec.chunk_tag.empty() ? "piston_rig" : spec.chunk_tag;

        const ecs::EntityRef body_a_ref = eeng::meta::resolve_entity_ref(*em, spec.body_a);
        const ecs::EntityRef body_b_ref = eeng::meta::resolve_entity_ref(*em, spec.body_b);

        ecs::Entity root_entity = spec.root.is_bound() ? spec.root.entity : ecs::Entity::EntityNull;
        if (root_entity.has_id() && !em->entity_valid(root_entity))
            root_entity = ecs::Entity::EntityNull;
        if (!root_entity.has_id())
        {
            rig.root = create_entity(*em, chunk_tag, prefix + "_Rig", ecs::Entity::EntityNull);
            root_entity = rig.root.entity;
        }
        else
        {
            rig.root = em->get_entity_ref(root_entity);
        }

        ensure_transform(registry, root_entity, spec.position, spec.rotation);

        // Anchors.
        rig.anchor_a = create_entity(*em, chunk_tag, prefix + "_AnchorA", root_entity);
        rig.anchor_b = create_entity(*em, chunk_tag, prefix + "_AnchorB", root_entity);
        ensure_transform(registry, rig.anchor_a.entity, spec.anchor_local_a, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        ensure_transform(registry, rig.anchor_b.entity, spec.anchor_local_b, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

        if (spec.use_sockets && has_ref_target(body_a_ref))
        {
            ecs::TransformSocketComponent socket{};
            socket.target = body_a_ref;
            socket.local_offset = spec.anchor_local_a;
            socket.follow_rotation = false;
            socket.capture_offset_in_edit = false;
            if (auto* tfm = registry.try_get<ecs::TransformComponent>(rig.anchor_a.entity))
                socket.last_local_version = tfm->local_version;
            registry.emplace<ecs::TransformSocketComponent>(rig.anchor_a.entity, socket);
        }
        if (spec.use_sockets && has_ref_target(body_b_ref))
        {
            ecs::TransformSocketComponent socket{};
            socket.target = body_b_ref;
            socket.local_offset = spec.anchor_local_b;
            socket.follow_rotation = false;
            socket.capture_offset_in_edit = false;
            if (auto* tfm = registry.try_get<ecs::TransformComponent>(rig.anchor_b.entity))
                socket.last_local_version = tfm->local_version;
            registry.emplace<ecs::TransformSocketComponent>(rig.anchor_b.entity, socket);
        }

        // Constraint entity.
        rig.constraint = create_entity(*em, chunk_tag, prefix + "_Constraint", root_entity);

        const glm::vec3 axis = normalize_or_default(spec.axis_local, glm::vec3(1.0f, 0.0f, 0.0f));
        // Large limits to emulate free lateral motion (Bullet requires finite ranges).
        const float lateral_limit = 1e5f;
        const float ang_limit = glm::pi<float>();

        ecs::SixDofSpringConstraintComponent sixdof{};
        sixdof.disable_collisions = spec.disable_collisions;
        sixdof.linear_limit_min = glm::vec3(spec.stroke_min, -lateral_limit, -lateral_limit);
        sixdof.linear_limit_max = glm::vec3(spec.stroke_max, lateral_limit, lateral_limit);
        sixdof.angular_limit_min = glm::vec3(-ang_limit);
        sixdof.angular_limit_max = glm::vec3(ang_limit);
        if (has_ref_target(body_a_ref))
            sixdof.entity_a = body_a_ref;
        if (has_ref_target(body_b_ref))
            sixdof.entity_b = body_b_ref;
        sixdof.local_anchor_a = spec.anchor_local_a;
        sixdof.local_anchor_b = spec.anchor_local_b;
        registry.emplace<ecs::SixDofSpringConstraintComponent>(rig.constraint.entity, sixdof);

        // Drive component (on root).
        ecs::PistonConstraintDriveComponent drive{};
        drive.constraint = rig.constraint;
        drive.anchor_a = rig.anchor_a;
        drive.anchor_b = rig.anchor_b;
        drive.axis_local = axis;
        drive.stroke_min = spec.stroke_min;
        drive.stroke_max = spec.stroke_max;
        drive.max_force = spec.max_force;
        drive.max_velocity = spec.max_velocity;
        drive.mode = spec.mode;
        drive.target_extension = spec.target_extension;
        drive.lock_when_idle = spec.lock_when_idle;
        registry.emplace_or_replace<ecs::PistonConstraintDriveComponent>(root_entity, drive);

        // Visual entity with align component.
        rig.visual = create_entity(*em, chunk_tag, prefix + "_Visual", root_entity);
        ensure_transform(registry, rig.visual.entity, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        ecs::TwoAnchorAlignComponent align{};
        align.anchor_a = rig.anchor_a;
        align.anchor_b = rig.anchor_b;
        if (has_ref_target(body_a_ref))
            align.up_reference = body_a_ref;
        align.local_forward_axis = axis;
        align.position_mode = 0; // anchor A
        registry.emplace<ecs::TwoAnchorAlignComponent>(rig.visual.entity, align);

        return rig;
    }

    nlohmann::json build_piston_rig_prefab_json(EngineContext& ctx, const PistonRigSpec& spec)
    {
        PistonRigSpec resolved_spec = spec;
        if (auto* em_live = eeng::try_get_entity_manager_ptr(ctx, "PistonRigBuilder"))
        {
            resolved_spec.body_a = eeng::meta::resolve_entity_ref(*em_live, resolved_spec.body_a);
            resolved_spec.body_b = eeng::meta::resolve_entity_ref(*em_live, resolved_spec.body_b);
        }

        // Build the rig in a scratch world so we can serialize it as a prefab.
        EngineContext scratch_ctx(
            std::make_unique<EntityManager>(),
            ctx.resource_manager,
            std::unique_ptr<eeng::IBatchRegistry>{},
            std::unique_ptr<eeng::IGuiManager>{},
            std::unique_ptr<eeng::IInputManager>{},
            ctx.log_manager);
        scratch_ctx.project_config = ctx.project_config;

        auto& em = static_cast<EntityManager&>(*scratch_ctx.entity_manager);
        auto& registry = em.registry();

        const std::string prefix = resolved_spec.name_prefix.empty() ? "Piston" : resolved_spec.name_prefix;
        const std::string chunk_tag = resolved_spec.chunk_tag.empty() ? "piston_rig" : resolved_spec.chunk_tag;

        const auto [root_guid, root_entity] = em.create_entity_live_parent(
            chunk_tag,
            prefix + "_Rig",
            ecs::Entity::EntityNull,
            ecs::Entity::EntityNull);
        (void)root_guid;

        auto& root_tfm = registry.emplace<ecs::TransformComponent>(root_entity);
        root_tfm.set_position(resolved_spec.position);
        root_tfm.set_rotation(resolved_spec.rotation);

        PistonRigSpec local_spec = resolved_spec;
        local_spec.root = em.get_entity_ref(root_entity);

        const auto rig = build_piston_rig(scratch_ctx, local_spec);
        if (!rig.root.is_bound())
            return nlohmann::json{};

        auto registry_sp = em.registry_wptr().lock();
        if (!registry_sp)
            return nlohmann::json{};

        auto& scenegraph = em.scene_graph();
        const auto branch = scenegraph.get_branch_topdown(rig.root.entity);

        nlohmann::json branch_json = nlohmann::json::array();
        for (const auto& entity : branch)
        {
            branch_json.push_back(meta::serialize_entity_for_file(
                em.get_entity_ref(entity),
                registry_sp));
        }

        return branch_json;
    }
} // namespace eeng::ecs
