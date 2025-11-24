// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IEntityManager.hpp"
#include "Guid.h"
#include "ecs/SceneGraph.hpp"

namespace eeng
{
    class EntityManager : public IEntityManager
    {
    public:
        EntityManager();

        ~EntityManager();

        bool entity_valid(
            const ecs::Entity& entity) const override;

        ecs::Entity create_empty_entity(
            const ecs::Entity& entity_hint) override;

        std::pair<Guid,ecs::Entity> create_entity(
            const std::string& chunk_tag,
            const std::string& name,
            const ecs::Entity& entity_parent,
            const ecs::Entity& entity_hint) override;

        bool entity_parent_registered(
            const ecs::Entity& entity) const override;

        void reparent_entity(
            const ecs::Entity& entity, const ecs::Entity& parent_entity) override;

        void set_entity_header_parent(
            const ecs::Entity& entity,
            const ecs::Entity& entity_parent) override;

        void queue_entity_for_destruction(
            const ecs::Entity& entity) override;

        int destroy_pending_entities() override;

        entt::registry& registry() noexcept override { return *registry_; }
        const entt::registry& registry() const noexcept override { return *registry_; }
        std::weak_ptr<entt::registry> registry_wptr() noexcept override { return registry_; }
        std::weak_ptr<const entt::registry> registry_wptr() const noexcept override { return registry_; }

        // Not part of public API

        // destroy_pending_entities

    private:

        void register_entity(const ecs::Entity& entity) override;

        std::shared_ptr<entt::registry> registry_;
        std::unordered_map<Guid, entt::entity> guid_to_entity_map_;
        // On create:
        // guid_to_entity_map[guid] = entity;
        // On destroy:
        // guid_to_entity_map.erase(guid);

        std::unique_ptr<ecs::SceneGraph> scene_graph_;
        std::deque<ecs::Entity> entities_pending_destruction_;
    };


} // namespace eeng