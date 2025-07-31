// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IEntityManager.hpp"
#include "ecs/SceneGraph.hpp"
#include <entt/fwd.hpp>

namespace eeng
{
    class EntityManager : public IEntityManager
    {
    public:
        EntityManager();

        ~EntityManager();

        ecs::Entity create_entity() override { return ecs::Entity{}; }

        void destroy_entity(ecs::Entity entity) override {}

        // entt::registry& registry() noexcept override  { return *_registry; }
        // const entt::registry& registry() const noexcept override { return *_registry; }

    private:
        std::unique_ptr<entt::registry> registry_;
        std::unique_ptr<ecs::SceneGraph> scene_graph_;
        // + scene graph
    };


} // namespace eeng