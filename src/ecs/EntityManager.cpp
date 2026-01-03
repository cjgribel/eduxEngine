// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EntityManager.hpp"
#include "HeaderComponent.hpp"
#include "TransformComponent.hpp"
#include <entt/entt.hpp>
#include <cassert>
#include <stdexcept>

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

    // Intended use: runtime creation (parent exists & is registered etc)
    // create_entity_live_parent (-> register_entity_live_parent)
    void EntityManager::register_entity_live_parent(const Entity& entity)
    {
        assert(registry_->all_of<HeaderComponent>(entity));
        auto& header_comp = registry_->get<HeaderComponent>(entity);
        auto& parent_entity = header_comp.parent_entity.entity;
        auto& guid = header_comp.guid;

        // Add entity to scene graph
        if (!parent_entity.has_id())
        {
            scene_graph_->insert_node(entity);
        }
        else
        {
            assert(scene_graph_->contains(parent_entity));
            scene_graph_->insert_node(entity, parent_entity);
        }

        // Register GUID and Entity to maps
        guid_to_entity_map_[guid] = entity;
        entity_to_guid_map_[entity] = guid;

        // Add to chunk registry
        // auto& chunk_tag = header.chunk_tag;
        // chunk_registry.add_entity(header.chunk_tag, entity);
    }

    // Intended use: during deserialization
    // desc -> create_entity_unregistered -> register_entities_from_deserialization
    void EntityManager::register_entities_from_deserialization(const std::vector<Entity>& entities)
    {
        // 1) Build GUID <-> Entity maps
        for (auto entity : entities)
        {
            // assert(registry_->all_of<HeaderComponent>(entity));
            // auto& header = registry_->get<HeaderComponent>(entity);
            const Guid& guid = get_entity_header(entity).guid;

            guid_to_entity_map_[guid] = entity;
            entity_to_guid_map_[entity] = guid;

            // Ensure node exists in scene graph as a root; parent resolution happens later.
            if (!scene_graph_->contains(entity))
            {
                scene_graph_->insert_node(entity);
            }
        }

        // 2) Resolve parent GUIDs to Entities and reparent.
        // This assumes parents are already registered in the GUID map (same batch/branch or live scene).
        for (auto entity : entities)
        {
            auto& header = get_entity_header(entity);
            //auto& header = registry_->get<HeaderComponent>(entity);
            auto& parent_ref = header.parent_entity;

            const Guid& parent_guid = parent_ref.guid;  // or parent_ref.get_guid()
            if (!parent_guid.valid())
            {
                parent_ref.unbind();
                continue; // stays root
            }

            auto parent_entity_opt = get_entity_from_guid(parent_guid);
            if (!parent_entity_opt || !parent_entity_opt->has_id())
                throw std::runtime_error("Parent GUID not found while registering entities");
            if (!scene_graph_->contains(*parent_entity_opt))
                throw std::runtime_error("Parent entity not registered in scene graph");

            // Bind runtime handle into EntityRef
            parent_ref.bind(*parent_entity_opt);

            // Reparent
            scene_graph_->reparent(entity, *parent_entity_opt);
        }
    }

    // Intended use: during deserialization
    // desc -> create_entity_unregistered -> register_entities_from_deserialization
    Entity EntityManager::create_entity_unregistered(const Entity& entity_hint)
    {
        if (!entity_hint.has_id())
            return Entity{ registry_->create() };

        Entity entity = Entity{ registry_->create(entity_hint) };
        assert(entity == entity_hint);
        return entity;
    }

    // Intended use: runtime creation (parent exists & is registered etc)
    // create_entity_live_parent (-> register_entity_live_parent)
    std::pair<Guid, ecs::Entity> EntityManager::create_entity_live_parent(
        const std::string& chunk_tag,
        const std::string& name,
        const Entity& entity_parent, // Entity ????
        const Entity& entity_hint)
    {
        // Invariants: parent is null or already known to EM
        if (entity_parent.has_id())
        {
            assert(registry_->valid(entity_parent));
            // optionally:
            assert(entity_to_guid_map_.contains(entity_parent));
            assert(scene_graph_->contains(entity_parent));
        }

        Entity entity = create_entity_unregistered(entity_hint);

        std::string used_name = name.size() ? name : std::to_string(entity.to_integral());
        std::string used_chunk_tag = chunk_tag.size() ? chunk_tag : "default_chunk";
        auto guid = Guid::generate();

        ecs::EntityRef parent_ref{};
        if (entity_parent.has_id())
        {
            parent_ref = get_entity_ref(entity_parent);   // uses EM's maps
        }

        registry_->emplace<HeaderComponent>(entity, used_name, used_chunk_tag, guid, parent_ref);

        register_entity_live_parent(entity);

        //std::cout << "Scene::create_entity_live_parent " << entity.to_integral() << std::endl;
        return { guid, entity };
    }

    bool EntityManager::entity_parent_registered(const Entity& entity) const
    {
        auto entity_parent = get_entity_parent(entity).entity;
        // assert(registry_->all_of<HeaderComponent>(entity));
        // auto& header = registry_->get<HeaderComponent>(entity);
        // auto entity_parent = header.parent_entity.get_entity();

        if (!entity_parent.has_id()) return true;
        return scene_graph_->contains(entity_parent);
    }

    void EntityManager::reparent_entity(const Entity& entity, const Entity& parent_entity)
    {
        // auto entity_parent_guid_it = entity_to_guid_map_.find(parent_entity);
        // assert(entity_parent_guid_it != entity_to_guid_map_.end());
        // auto& parent_guid = entity_parent_guid_it->second;

        get_entity_header(entity).parent_entity = get_entity_ref(parent_entity);
        // set_entity_parent(entity, parent_entity);

        // assert(registry_->all_of<HeaderComponent>(entity));
        // registry_->get<HeaderComponent>(entity).parent_entity = EntityRef { parent_guid, parent_entity };

        scene_graph_->reparent(entity, parent_entity);

        if (registry_->all_of<TransformComponent>(entity))
        {
            auto& tfm = registry_->get<TransformComponent>(entity);
            tfm.mark_local_dirty();
        }
    }

    // void EntityManager::set_entity_parent(const Entity& entity, const Entity& entity_parent)
    // {
    //     get_entity_header(entity).parent_entity = get_entity_ref(entity_parent);

    //     // assert(registry_->all_of<HeaderComponent>(entity));
    //     // registry_->get<HeaderComponent>(entity).parent_entity = entity_parent;

    //     // register_entity_live_parent(entity);
    // }

    void EntityManager::queue_entity_for_destruction(const ecs::Entity& entity)
    {
        entities_pending_destruction_.push_back(entity);
    }

    int EntityManager::destroy_pending_entities()
    {
        int count = 0;
        while (entities_pending_destruction_.size())
        {
            // Fetch entity
            auto entity = entities_pending_destruction_.front();
            entities_pending_destruction_.pop_front();

            // Remove entity mappings
            auto guid_it = entity_to_guid_map_.find(entity);
            assert(guid_it != entity_to_guid_map_.end());
            auto guid = guid_it->second;
            entity_to_guid_map_.erase(entity);

            auto entity_it = guid_to_entity_map_.find(guid);
            assert(entity_it != guid_to_entity_map_.end());
            guid_to_entity_map_.erase(guid);

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

    ecs::SceneGraph& EntityManager::scene_graph() { return *scene_graph_; }
    const ecs::SceneGraph& EntityManager::scene_graph() const { return *scene_graph_; }

    ecs::EntityRef EntityManager::get_entity_ref(const ecs::Entity& entity) const
    {
        if (!entity.has_id())
        {
            return {}; // default/null EntityRef (guid + entity both “empty”)
        }

        return ecs::EntityRef{ get_entity_guid(entity), entity };
    }

    HeaderComponent& EntityManager::get_entity_header(const ecs::Entity& entity)
    {
        assert(registry_->all_of<HeaderComponent>(entity));
        return registry_->get<HeaderComponent>(entity);
    }

    const HeaderComponent& EntityManager::get_entity_header(const ecs::Entity& entity) const
    {
        assert(registry_->all_of<HeaderComponent>(entity));
        return registry_->get<HeaderComponent>(entity);
    }

    ecs::EntityRef EntityManager::get_entity_parent(const ecs::Entity& entity) const
    {
        return get_entity_header(entity).parent_entity;
    }

    // Guid& EntityManager::get_entity_guid(const ecs::Entity& entity)
    // {
    //     assert(!entity.is_null());
    //     auto it = entity_to_guid_map_.find(entity);
    //     assert(it != entity_to_guid_map_.end());
    //     return it->second;
    // }

    const Guid EntityManager::get_entity_guid(const ecs::Entity& entity) const
    {
        assert(entity.has_id());
        auto it = entity_to_guid_map_.find(entity);
        assert(it != entity_to_guid_map_.end());
        return it->second;
    }

std::optional<ecs::Entity> EntityManager::get_entity_from_guid(const Guid& guid) const
{
    auto it = guid_to_entity_map_.find(guid);
    if (it == guid_to_entity_map_.end())
    {
        return std::nullopt;
    }

    return it->second;
}

} // namespace eeng
