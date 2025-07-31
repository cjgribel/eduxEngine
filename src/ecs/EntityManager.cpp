// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EntityManager.hpp"
#include <entt/entt.hpp>

namespace eeng
{
    EntityManager::EntityManager()
        : registry_(std::make_shared<entt::registry>())
        , scene_graph_(std::make_unique<ecs::SceneGraph>())
    {
    }

    EntityManager::~EntityManager() = default;

} // namespace eeng
