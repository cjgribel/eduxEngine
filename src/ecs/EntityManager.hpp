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

        /// Create an unregistered entity handle for deserialization paths.
        /// Intended use: create_entity_unregistered -> add components -> register_entities_from_deserialization.
        ecs::Entity create_entity_unregistered(
            const ecs::Entity& entity_hint) override;

        /// Create and register an entity with a live parent handle.
        /// Intended use: runtime creation when parent is already registered in the scene graph.
        std::pair<Guid, ecs::Entity> create_entity_live_parent(
            const std::string& chunk_tag,
            const std::string& name,
            const ecs::Entity& entity_parent,
            const ecs::Entity& entity_hint) override;

        /// Register a single entity whose header contains a live parent handle (if any).
        /// Intended use: runtime creation, not deserialization (parent must already be live).
        void register_entity_live_parent(const ecs::Entity& entity) override;

        /// Register a batch of deserialized entities using parent GUIDs in headers.
        /// Intended use: after deserialization when GUID maps are complete; throws if a parent is missing.
        void register_entities_from_deserialization(const std::vector<ecs::Entity>& entities) override;

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

        // (public, not part of interface)

        ecs::SceneGraph& scene_graph();
        const ecs::SceneGraph& scene_graph() const;

        ecs::EntityRef get_entity_ref(const ecs::Entity& entity) const;

        ecs::HeaderComponent& get_entity_header(const ecs::Entity& entity);
        const ecs::HeaderComponent& get_entity_header(const ecs::Entity& entity) const;

        ecs::EntityRef get_entity_parent(const ecs::Entity& entity) const;

        // Guid& get_entity_guid(const ecs::Entity& entity);
        const Guid get_entity_guid(const ecs::Entity& entity) const;

        std::optional<ecs::Entity> get_entity_from_guid(const Guid& guid) const override;

        // destroy_pending_entities

        template<class Fn>
        void for_each_registered_entity(Fn&& fn) const
        {
            for (const auto& [entity, guid] : entity_to_guid_map_)
            {
                fn(entity, guid);
            }
        }

        std::size_t registered_entity_count() const
        {
            return entity_to_guid_map_.size();
        }

    private:
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
