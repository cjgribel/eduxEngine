// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/MousePointConstraintSystem.hpp"

#include "EngineContext.hpp"
#include "engineapi/IInputManager.hpp"
#include "ecs/EntityManager.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"
#include "ecs/systems/PhysicsSystem.hpp"

namespace
{
    constexpr float kMinDistance = 0.1f;
    constexpr float kScrollSpeed = 0.5f;
    constexpr float kMaxPickDistance = 2000.0f;
}

namespace eeng::ecs::systems
{
    bool MousePointConstraintSystem::compute_mouse_ray(
        const EngineContext& ctx,
        glm_aux::Ray& out_ray) const
    {
        if (!ctx.input_manager || !ctx.overlay_view_state)
            return false;

        const auto& view_state = *ctx.overlay_view_state;

        const auto mouse = ctx.input_manager->GetMouseState();
        if (view_state.window_size.x <= 0 || view_state.window_size.y <= 0)
            return false;

        const glm::ivec2 window_pos(mouse.x, view_state.window_size.y - mouse.y);
        out_ray = glm_aux::world_ray_from_window_coords(
            window_pos,
            view_state.view,
            view_state.proj,
            view_state.viewport);
        return true;
    }

    bool MousePointConstraintSystem::create_picker_entities(
        entt::registry& registry,
        EngineContext& ctx)
    {
        if (!ctx.entity_manager)
            return false;

        auto& em = static_cast<eeng::EntityManager&>(*ctx.entity_manager);

        if (anchor_entity_.has_id() && registry.valid(anchor_entity_))
            ctx.entity_manager->destroy_entity_now(anchor_entity_);
        if (constraint_entity_.has_id() && registry.valid(constraint_entity_))
            ctx.entity_manager->destroy_entity_now(constraint_entity_);

        auto [anchor_guid, anchor_entity] = em.create_entity_live_parent(
            "runtime",
            "RMB_PickerAnchor",
            eeng::ecs::Entity::EntityNull,
            eeng::ecs::Entity::EntityNull);
        (void)anchor_guid;
        anchor_entity_ = anchor_entity;

        auto& anchor_tfm = registry.emplace<eeng::ecs::TransformComponent>(anchor_entity_);
        anchor_tfm.set_position(glm::vec3(0.0f));
        anchor_tfm.set_rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        anchor_tfm.set_scale(glm::vec3(1.0f));

        auto& anchor_rb = registry.emplace<eeng::ecs::RigidBodyComponent>(anchor_entity_);
        anchor_rb.motion = eeng::ecs::PhysicsMotionType::Kinematic;
        anchor_rb.auto_mass = false;
        anchor_rb.mass = 1.0f;
        anchor_rb.allow_sleep = false;

        eeng::ecs::ColliderComponent colliders{};
        eeng::ecs::ColliderDesc collider{};
        collider.type = eeng::ecs::ColliderType::Sphere;
        collider.radius = 0.15f;
        collider.is_trigger = true;
        colliders.colliders.push_back(collider);
        registry.emplace<eeng::ecs::ColliderComponent>(anchor_entity_, std::move(colliders));

        auto [constraint_guid, constraint_entity] = em.create_entity_live_parent(
            "runtime",
            "RMB_PickerConstraint",
            eeng::ecs::Entity::EntityNull,
            eeng::ecs::Entity::EntityNull);
        (void)constraint_guid;
        constraint_entity_ = constraint_entity;

        auto& constraint = registry.emplace<eeng::ecs::PointConstraintComponent>(constraint_entity_);
        constraint.disable_collisions = true;
        constraint.enabled = false;

        return true;
    }

    void MousePointConstraintSystem::disable_picker(entt::registry& registry, EngineContext& ctx)
    {
        active_ = false;
        target_entity_.set_null();
        distance_ = 0.0f;

        if (constraint_entity_.has_id() && registry.valid(constraint_entity_))
        {
            if (auto* constraint = registry.try_get<eeng::ecs::PointConstraintComponent>(constraint_entity_))
                constraint->enabled = false;
        }

        if (ctx.entity_manager)
        {
            if (constraint_entity_.has_id() && registry.valid(constraint_entity_))
                ctx.entity_manager->destroy_entity_now(constraint_entity_);
            if (anchor_entity_.has_id() && registry.valid(anchor_entity_))
                ctx.entity_manager->destroy_entity_now(anchor_entity_);
        }

        constraint_entity_.set_null();
        anchor_entity_.set_null();
    }

    void MousePointConstraintSystem::update(
        entt::registry& registry,
        EngineContext& ctx,
        PhysicsSystem& physics_system)
    {
        if (ctx.services && !ctx.services->play_mode_active.load())
        {
            if (active_)
                disable_picker(registry, ctx);
            return;
        }

        if (!ctx.input_manager)
            return;

        const auto mouse = ctx.input_manager->GetMouseState();
        const bool rmb_down = mouse.rightButton;
        const bool rmb_pressed = rmb_down && !rmb_was_down_;
        const bool rmb_released = !rmb_down && rmb_was_down_;
        rmb_was_down_ = rmb_down;

        if (!rmb_down && !active_ && !rmb_pressed)
            return;

        glm_aux::Ray ray;
        if (!compute_mouse_ray(ctx, ray))
        {
            if (rmb_released || active_)
                disable_picker(registry, ctx);
            return;
        }

        if (rmb_pressed)
        {
            active_ = false;
            target_entity_.set_null();

            if (!create_picker_entities(registry, ctx))
                return;

            PhysicsSystem::RaycastHit hit{};
            PhysicsSystem::RaycastFilter filter{};
            filter.include_triggers = false;

            if (physics_system.raycast(ray.origin, ray.dir, kMaxPickDistance, hit, filter))
            {
                const entt::entity hit_entity = static_cast<entt::entity>(hit.entity);
                if (registry.valid(hit_entity))
                {
                    const auto* rb = registry.try_get<eeng::ecs::RigidBodyComponent>(hit_entity);
                    const auto* tfm = registry.try_get<eeng::ecs::TransformComponent>(hit_entity);
                    if (rb && tfm && rb->motion == eeng::ecs::PhysicsMotionType::Dynamic)
                    {
                        target_entity_ = hit.entity;
                        distance_ = std::max(kMinDistance, hit.distance);

                        const glm::vec3 local_anchor = glm::vec3(
                            glm::inverse(tfm->world_matrix) * glm::vec4(hit.point, 1.0f));

                        auto& anchor_tfm = registry.get<eeng::ecs::TransformComponent>(anchor_entity_);
                        anchor_tfm.set_position(hit.point);

                        auto& constraint = registry.get<eeng::ecs::PointConstraintComponent>(constraint_entity_);
                        auto& em = static_cast<eeng::EntityManager&>(*ctx.entity_manager);
                        constraint.entity_a = em.get_entity_ref(hit.entity);
                        constraint.entity_b = em.get_entity_ref(anchor_entity_);
                        constraint.local_anchor_a = local_anchor;
                        constraint.local_anchor_b = glm::vec3(0.0f);
                        constraint.disable_collisions = true;
                        constraint.enabled = true;

                        physics_system.wake_body(static_cast<entt::entity>(hit.entity));

                        active_ = true;
                    }
                }
            }

            if (!active_)
            {
                disable_picker(registry, ctx);
                return;
            }
        }

        if (!active_)
            return;

        const bool target_valid = target_entity_.has_id()
            && registry.valid(target_entity_);
        if (!target_valid)
        {
            disable_picker(registry, ctx);
            return;
        }

        const auto* target_rb = registry.try_get<eeng::ecs::RigidBodyComponent>(target_entity_);
        const auto* target_tfm = registry.try_get<eeng::ecs::TransformComponent>(target_entity_);
        if (!target_rb || !target_tfm || target_rb->motion != eeng::ecs::PhysicsMotionType::Dynamic)
        {
            disable_picker(registry, ctx);
            return;
        }

        if (rmb_released)
        {
            disable_picker(registry, ctx);
            return;
        }

        if (mouse.scroll_y != 0.0f)
        {
            distance_ -= mouse.scroll_y * kScrollSpeed;
            distance_ = std::max(distance_, kMinDistance);
        }

        auto& anchor_tfm = registry.get<eeng::ecs::TransformComponent>(anchor_entity_);
        anchor_tfm.set_position(ray.origin + ray.dir * distance_);

        if (auto* constraint = registry.try_get<eeng::ecs::PointConstraintComponent>(constraint_entity_))
            constraint->enabled = true;
    }

    void MousePointConstraintSystem::shutdown(EngineContext& ctx)
    {
        if (!ctx.entity_manager)
            return;

        auto& registry = ctx.entity_manager->registry();
        disable_picker(registry, ctx);
    }
} // namespace eeng::ecs::systems
