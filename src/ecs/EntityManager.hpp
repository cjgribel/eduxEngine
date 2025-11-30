// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IEntityManager.hpp"
#include "Guid.h"
#include "ecs/SceneGraph.hpp"

namespace eeng::ecs
{
    struct HeaderComponent;
}

namespace eeng
{
    class EntityManager : public IEntityManager
    {
    public:
        EntityManager();

        ~EntityManager();

        bool entity_valid(
            const ecs::Entity& entity) const override;

        // Intended use: during deserialization
        // desc -> create_empty_entity -> register_entities
        ecs::Entity create_empty_entity(
            const ecs::Entity& entity_hint) override;

        // Intended use: runtime creation (parent exists & is registered etc)
        // create_entity (-> register_entitity)
        std::pair<Guid, ecs::Entity> create_entity(
            const std::string& chunk_tag,
            const std::string& name,
            const ecs::Entity& entity_parent,
            const ecs::Entity& entity_hint) override;

        bool entity_parent_registered(
            const ecs::Entity& entity) const override;

        void reparent_entity(
            const ecs::Entity& entity, const ecs::Entity& parent_entity) override;

        // ONLY SETS HEADER - KEEP PRIVATE OR SKIP
        // void set_entity_parent(
        //     const ecs::Entity& entity,
        //     const ecs::Entity& entity_parent) override;

        void queue_entity_for_destruction(
            const ecs::Entity& entity) override;

        int destroy_pending_entities() override;

        entt::registry& registry() noexcept override { return *registry_; }
        const entt::registry& registry() const noexcept override { return *registry_; }
        std::weak_ptr<entt::registry>       registry_wptr() noexcept override { return registry_; }
        std::weak_ptr<const entt::registry> registry_wptr() const noexcept override { return registry_; }

        // Not part of interface

        ecs::SceneGraph& scene_graph();
        const ecs::SceneGraph& scene_graph() const;

        ecs::EntityRef get_entity_ref(const ecs::Entity& entity) const;

        ecs::HeaderComponent& get_entity_header(const ecs::Entity& entity);
        const ecs::HeaderComponent& get_entity_header(const ecs::Entity& entity) const;

        ecs::EntityRef get_entity_parent(const ecs::Entity& entity) const;

        Guid& get_entity_guid(const ecs::Entity& entity);
        const Guid& get_entity_guid(const ecs::Entity& entity) const;

        ecs::Entity get_entity_from_guid(const Guid& guid) const;

        // destroy_pending_entities

    private:

        // Why private?
        // Intended use: runtime creation (parent exists & is registered etc)
        // create_entity (-> register_entitity)
        void register_entity(const ecs::Entity& entity) override;

        // Why private?
        // Intended use: during deserialization
        // desc -> create_empty_entity -> register_entities
        void register_entities(const std::vector<ecs::Entity>& entities) override;


        std::shared_ptr<entt::registry>     registry_;
        std::unordered_map<Guid, ecs::Entity>    guid_to_entity_map_;
        std::unordered_map<ecs::Entity, Guid>    entity_to_guid_map_;
        // On create:
        // guid_to_entity_map[guid] = entity;
        // On destroy:
        // guid_to_entity_map.erase(guid);

        std::unique_ptr<ecs::SceneGraph>    scene_graph_;
        std::deque<ecs::Entity>             entities_pending_destruction_;
    };


} // namespace eeng