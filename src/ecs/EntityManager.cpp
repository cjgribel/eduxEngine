// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EntityManager.hpp"
#include "HeaderComponent.hpp"
#include <entt/entt.hpp>
#include <cassert>

namespace eeng
{
    //using Header = ecs::HeaderComponent;
    using namespace ecs;

    EntityManager::EntityManager()
        : registry_(std::make_shared<entt::registry>())
        , scene_graph_(std::make_unique<ecs::SceneGraph>())
    {
    }

    EntityManager::~EntityManager() = default;

    bool EntityManager::entity_valid(const Entity& entity) const
    {
        return registry_->valid(entity);
    }

    void EntityManager::register_entity(const Entity& entity)
    {
        assert(registry_->all_of<HeaderComponent>(entity));
        auto& header_comp = registry_->get<HeaderComponent>(entity);

        // Add entity to scene graph
        auto parent_entity = Entity{ entt::entity{header_comp.parent_entity} };
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

    Entity EntityManager::create_empty_entity(const Entity& entity_hint)
    {
        if (entity_hint.is_null())
            return Entity{ registry_->create() };

        Entity entity = Entity{ registry_->create(entity_hint) };
        assert(entity == entity_hint);
        return entity;
    }

    // -> create_and_register_entity
    Entity EntityManager::create_entity(
        const std::string& chunk_tag,
        const std::string& name,
        const Entity& entity_parent,
        const Entity& entity_hint)
    {
        Entity entity = create_empty_entity(entity_hint);

        std::string used_name = name.size() ? name : std::to_string(entity.to_integral());
        std::string used_chunk_tag = chunk_tag.size() ? chunk_tag : "default_chunk";
        auto guid = Guid::generate();
        registry_->emplace<HeaderComponent>(entity, used_name, used_chunk_tag, guid, entity_parent);

        register_entity(entity);

        //std::cout << "Scene::create_entity " << entity.to_integral() << std::endl;
        return entity;
    }

    bool EntityManager::entity_parent_registered(const Entity& entity) const
    {
        assert(registry_->all_of<HeaderComponent>(entity));
        auto& header = registry_->get<HeaderComponent>(entity);
        auto entity_parent = Entity{ entt::entity{header.parent_entity} };

        if (entity_parent.is_null()) return true;
        return scene_graph_->contains(entity_parent);
    }

    void EntityManager::reparent_entity(const Entity& entity, const Entity& parent_entity)
    {
        assert(registry_->all_of<HeaderComponent>(entity));
        registry_->get<HeaderComponent>(entity).parent_entity = parent_entity;

        scene_graph_->reparent(entity, parent_entity);
    }

    void EntityManager::set_entity_header_parent(const Entity& entity, const Entity& entity_parent)
    {
        assert(registry_->all_of<HeaderComponent>(entity));
        registry_->get<HeaderComponent>(entity).parent_entity = entity_parent;

        // register_entity(entity);
    }

    void EntityManager::queue_entity_for_destruction(const ecs::Entity& entity)
    {
        entities_pending_destruction_.push_back(entity);
    }

    int EntityManager::destroy_pending_entities()
    {
        int count = 0;
        while (entities_pending_destruction_.size())
        {
            auto entity = entities_pending_destruction_.front();
            entities_pending_destruction_.pop_front();

            // Remove from chunk registry
            assert(registry_->valid(entity));
            assert(registry_->all_of<HeaderComponent>(entity));

            // Remove from chunk registry
            //chunk_registry.remove_entity(registry_->get<HeaderComponent>(entity).chunk_tag, entity);

            // Remove from scene graph
            if (scene_graph_->contains(entity))
                scene_graph_->erase_node(entity);

            // Destroy entity. May lead to additional entities being added to the queue.
            registry_->destroy(entity);
            //registry_->destroy(entity, 0); // TODO: This is a fix to keep generations of entities equal

            count++;
        }

        return count;
    }

} // namespace eeng
