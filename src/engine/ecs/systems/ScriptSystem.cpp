// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/ScriptSystem.hpp"

#include "EngineContext.hpp"
#include "ecs/ScriptComponent.hpp"
#include "engineapi/EngineContextHelpers.hpp"

namespace eeng::ecs::systems
{
    void ScriptSystem::init(EngineContext& ctx)
    {
        // Cache context for lifecycle hooks.
        ctx_ = &ctx;

        // Hook into ScriptComponent lifetime to mirror the physics lifecycle pattern.
        auto* registry = eeng::try_get_registry_ptr(ctx, "ScriptSystem");
        if (!registry)
            return;

        script_construct_conn_ = registry->on_construct<ecs::ScriptComponent>()
            .connect<&ScriptSystem::on_script_construct>(this);
        script_destroy_conn_ = registry->on_destroy<ecs::ScriptComponent>()
            .connect<&ScriptSystem::on_script_destroy>(this);
    }

    void ScriptSystem::shutdown()
    {
        // Clear runtime bookkeeping; real script state cleanup will live here later.
        live_scripts_.clear();
        ctx_ = nullptr;
    }

    void ScriptSystem::update(entt::registry&, EngineContext&, float)
    {
        // Placeholder: no script execution yet.
    }

    void ScriptSystem::on_script_construct(entt::registry&, entt::entity entity)
    {
        // Record the entity so future script runtime can bind per-entity tables.
        live_scripts_.insert(entity);
    }

    void ScriptSystem::on_script_destroy(entt::registry&, entt::entity entity)
    {
        // Remove any bookkeeping for this entity.
        live_scripts_.erase(entity);
    }
} // namespace eeng::ecs::systems
