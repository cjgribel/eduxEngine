// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <deque>
#include <string>
#include "EngineContext.hpp"

namespace eeng::editor
{
    struct SceneActions
    {
        static void create_entity(EngineContext& ctx, const ecs::Entity& parent_entity);
        static void delete_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection);
        static void copy_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection);
        static void parent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection);
        static void unparent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection);
    };

    struct BatchActions
    {
        static void load_batch(EngineContext& ctx, const BatchId& id);
        static void unload_batch(EngineContext& ctx, const BatchId& id);
        static void load_all(EngineContext& ctx);
        static void unload_all(EngineContext& ctx);
        static void create_batch(EngineContext& ctx, std::string name);
        static void delete_batch(EngineContext& ctx, const BatchId& id);
    };
}
