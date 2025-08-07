// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "ecs/Entity.hpp"
#include <entt/fwd.hpp>
#include <cstddef>

namespace eeng
{
    class IEntityManager
    {
    public:
        virtual ~IEntityManager() = default;

        virtual bool entity_valid(
            const ecs::Entity& entity) const = 0;

        // When Entity was already created, e.g. during deserialization
        virtual void register_entity(
            const ecs::Entity& entity) = 0;

        virtual ecs::Entity create_empty_entity(
            const ecs::Entity& entity_hint) = 0;

        virtual ecs::Entity create_entity(
            const std::string& chunk_tag,
            const std::string& name,
            const ecs::Entity& entity_parent,
            const ecs::Entity& entity_hint) = 0;

        virtual bool entity_parent_registered(
            const ecs::Entity& entity) const = 0;

        virtual void reparent_entity(
            const ecs::Entity& entity, 
            const ecs::Entity& parent_entity) = 0;

        virtual void set_entity_header_parent(
            const ecs::Entity& entity, 
            const ecs::Entity& entity_parent) = 0;

        // entt::entity Scene::create_entity_hint(
        //     entt::entity hint_entity,
        //     entt::entity parent_entity)
        // {
        //     return create_entity("", "", parent_entity, hint_entity);
        // }

        // entt::entity Scene::create_entity(
        //     entt::entity parent_entity)
        // {
        //     return create_entity("", "", parent_entity, entt::null);
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