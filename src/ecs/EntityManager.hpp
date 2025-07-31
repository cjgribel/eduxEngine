// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IEntityManager.hpp"
#include "ecs/SceneGraph.hpp"

namespace eeng
{
    class EntityManager : public IEntityManager
    {
    public:
        EntityManager();

        ~EntityManager();

        ecs::Entity create_entity() override { return ecs::Entity{}; }

        void destroy_entity(ecs::Entity entity) override {}

        entt::registry& registry() noexcept override { return *registry_; }
        const entt::registry& registry() const noexcept override { return *registry_; }
        std::weak_ptr<entt::registry> registry_wptr() noexcept override { return registry_; }
        std::weak_ptr<const entt::registry> registry_wptr() const noexcept override { return registry_; }

    private:
        std::shared_ptr<entt::registry> registry_;
        std::unique_ptr<ecs::SceneGraph> scene_graph_;
        // + scene graph
    };


} // namespace eeng