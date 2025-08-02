// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EntityManager.hpp"
#include "HeaderComponent.hpp"
#include <entt/entt.hpp>
#include <cassert>

namespace eeng
{
    //using Header = ecs::HeaderComponent;

    EntityManager::EntityManager()
        : registry_(std::make_shared<entt::registry>())
        , scene_graph_(std::make_unique<ecs::SceneGraph>())
    {
    }

    EntityManager::~EntityManager() = default;

    bool EntityManager::entity_valid(const ecs::Entity& entity) const
    {
        return registry_->valid(entity);
    }

    void EntityManager::register_entity(const ecs::Entity& entity)
    {
        assert(registry_->all_of<ecs::HeaderComponent>(entity));
        auto& header_comp = registry_->get<ecs::HeaderComponent>(entity);

        // Add entity to scene graph
        auto parent_entity = ecs::Entity{ entt::entity{header_comp.parent_entity} };
        if (parent_entity.is_null())
        {
            scene_graph_->insert_node(entity);
        }
        else
        {
            assert(scene_graph_->contains(parent_entity));
            scene_graph_->insert_node(entity, parent_entity);
        }

        // Register GUID
        guid_to_entity_map_[header_comp.guid] = entity;

        // Add to chunk registry
        // auto& chunk_tag = header.chunk_tag;
        // chunk_registry.add_entity(header.chunk_tag, entity);
    }

} // namespace eeng
