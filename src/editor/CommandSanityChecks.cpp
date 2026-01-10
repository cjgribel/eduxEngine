// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/CommandSanityChecks.hpp"
#include "EngineContextHelpers.hpp"
#include "LogMacros.h"
#include "ecs/HeaderComponent.hpp"
#include <string>

namespace
{
    // Helper to get entity name from HeaderComponent.
    std::string get_entity_debug_name(const eeng::EntityManager& em, const eeng::ecs::Entity& entity)
    {
        const auto& reg = em.registry();
        if (!reg.valid(entity))
            return "<invalid entity>";
        std::string name = "<unknown>";
        std::string guid_str = "<unknown>";
        if (reg.valid(entity) && reg.all_of<eeng::ecs::HeaderComponent>(entity))
        {
            const auto& header = reg.get<eeng::ecs::HeaderComponent>(entity);
            name = header.name;
            if (header.guid.valid())
                guid_str = header.guid.to_string();
        }
        if (!reg.all_of<eeng::ecs::HeaderComponent>(entity))
            return "<no header>";
        return name + " (" + guid_str + ")";
    }
}

namespace eeng::editor
{
    // Run invariant checks after command execution; logs warnings on anomalies.
    void run_command_sanity_checks(const EngineContext& ctx)
    {
        EENG_LOG(&ctx, "Running command sanity checks...");

        using namespace eeng::ecs;

        auto* entity_manager = eeng::try_get_entity_manager_ptr(ctx, "CommandSanityChecks");
        if (!entity_manager)
            return;

        const auto& registry = entity_manager->registry();
        entity_manager->for_each_registered_entity([&ctx, &registry](const Entity& entity, const Guid& guid) {
            const auto debug_name = get_entity_debug_name(*entity_manager, entity);
            if (!registry.valid(entity))
            {
                EENG_LOG_WARN(&ctx,
                    "CommandSanity: Registered entity %u (%s) is invalid in registry.",
                    static_cast<unsigned int>(entity.to_integral()),
                    debug_name.c_str());
                return;
            }
            if (!registry.all_of<HeaderComponent>(entity))
            {
                EENG_LOG_WARN(&ctx,
                    "CommandSanity: Registered entity %u (%s) missing HeaderComponent.",
                    static_cast<unsigned int>(entity.to_integral()),
                    debug_name.c_str());
            }
            });

        // auto rm = eeng::try_get_resource_manager_ptr(ctx, "CommandSanityChecks");
        // auto em = try_get_entity_manager_ptr(ctx, "CommandSanityChecks");
        // auto reg = try_get_batch_registry_ptr(ctx);

        // if (!entity_manager)
        //     return;
#if 0

        // --- Entity validity -------------------------------------------------

        entity_manager->for_each_entity([&ctx, &entity_manager](Entity entity) {
            if (!entity_manager->entity_valid(entity))
            {
                EENG_LOG_WARN(&ctx, "CommandSanity: Invalid entity detected: %s", to_string(entity).c_str());
            }
            });

        // Make sure all entt entities are in EntityManager maps
        entity_manager->for_each_entity([&ctx, &entity_manager](Entity entity) {
            auto guid_opt = entity_manager->get_entity_guid(entity);
            if (!guid_opt.valid())
            {
                EENG_LOG_WARN(&ctx, "CommandSanity: Entity %s missing GUID mapping.", to_string(entity).c_str());
            }
            });

        // --- Parent-child consistency ---------------------------------------

        entity_manager->for_each_entity([&ctx, &entity_manager](Entity entity) {
            auto parent_opt = entity_manager->get_parent(entity);
            if (parent_opt)
            {
                Entity parent = *parent_opt;
                auto children = entity_manager->get_children(parent);
                auto it = std::find(children.begin(), children.end(), entity);
                if (it == children.end())
                {
                    EENG_LOG_WARN(&ctx, "CommandSanity: Entity %s has parent %s, but is not listed among its children.",
                        to_string(entity).c_str(),
                        to_string(parent).c_str());
                }
            }
            });

        entity_manager->for_each_entity([&ctx, &entity_manager](Entity entity) {
            auto children = entity_manager->get_children(entity);
            for (const Entity& child : children)
            {
                auto parent_opt = entity_manager->get_parent(child);
                if (!parent_opt || *parent_opt != entity)
                {
                    EENG_LOG_WARN(&ctx, "CommandSanity: Entity %s lists child %s, but child does not reference it as parent.",
                        to_string(entity).c_str(),
                        to_string(child).c_str());
                }
            }
            });

        // Make sure all entities belong to a batch
        entity_manager->for_each_entity([&ctx, &entity_manager](Entity entity) {
            auto batch_opt = entity_manager->get_entity_batch(entity);
            if (!batch_opt)
            {
                EENG_LOG_WARN(&ctx, "CommandSanity: Entity %s does not belong to any batch.", to_string(entity).c_str());
            }
            });

        // Make sure all batches' live entities are valid
        if (ctx.batch_registry)
        {
            auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
            br.for_each_loaded_batch([&ctx, &entity_manager](const BatchId& batch_id, const Batch& batch) {
                for (const auto& entity : batch.live_entities)
                {
                    if (!entity_manager->entity_valid(entity))
                    {
                        EENG_LOG_WARN(&ctx, "CommandSanity: Batch %s contains invalid live entity %s.",
                            batch_id.to_string().c_str(),
                            to_string(entity).c_str());
                    }
                }
                });
        }

#endif
    }
}
