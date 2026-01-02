// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "ecs/Entity.hpp"
#include <entt/fwd.hpp>
#include <string>
#include <optional>
#include <cstddef>

namespace eeng
{
    class IEntityManager
    {
    public:
        virtual ~IEntityManager() = default;

        virtual bool entity_valid(
            const ecs::Entity& entity) const = 0;

        /// Register a single entity whose header contains a live parent handle (if any).
        /// Intended use: runtime creation, not deserialization (parent must already be live).
        virtual void register_entity_live_parent(
            const ecs::Entity& entity) = 0;

        /// Register a batch of deserialized entities using parent GUIDs in headers.
        /// Intended use: after deserialization when GUID maps are complete; throws if a parent is missing.
        virtual void register_entities_from_deserialization(const std::vector<ecs::Entity>& entities) = 0;

        /// Create an unregistered entity handle for deserialization paths.
        /// Intended use: create_entity_unregistered -> add components -> register_entities_from_deserialization.
        virtual ecs::Entity create_entity_unregistered(
            const ecs::Entity& entity_hint) = 0;

        /// Create and register an entity with a live parent handle.
        /// Intended use: runtime creation when parent is already registered in the scene graph.
        virtual std::pair<Guid, ecs::Entity> create_entity_live_parent(
            const std::string& chunk_tag,
            const std::string& name,
            const ecs::Entity& entity_parent    = ecs::Entity::EntityNull,
            const ecs::Entity& entity_hint      = ecs::Entity::EntityNull) = 0;

        virtual bool entity_parent_registered(
            const ecs::Entity& entity) const = 0;

        virtual void reparent_entity(
            const ecs::Entity& entity,
            const ecs::Entity& parent_entity) = 0;

        /// Resolve a GUID to a live entity handle if available.
        virtual std::optional<ecs::Entity> get_entity_from_guid(const Guid& guid) const = 0;

        // virtual void set_entity_parent(
        //     const ecs::Entity& entity,
        //     const ecs::Entity& entity_parent) = 0;

        // entt::entity Scene::create_entity_hint(
        //     entt::entity hint_entity,
        //     entt::entity parent_entity)
        // {
        //     return create_entity_live_parent("", "", parent_entity, hint_entity);
        // }

        // entt::entity Scene::create_entity_live_parent(
        //     entt::entity parent_entity)
        // {
        //     return create_entity_live_parent("", "", parent_entity, entt::null);
        // }

        virtual void queue_entity_for_destruction(
            const ecs::Entity& entity) = 0;

        virtual int destroy_pending_entities() = 0;

        virtual entt::registry& registry() noexcept = 0;
        virtual const entt::registry& registry() const noexcept = 0;
        virtual std::weak_ptr<entt::registry> registry_wptr() noexcept = 0;
        virtual std::weak_ptr<const entt::registry> registry_wptr() const noexcept = 0;
    };
} // namespace eeng
