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

        // virtual ecs::Entity create_entity() = 0;
        // virtual void destroy_entity(ecs::Entity entity) = 0;

        virtual bool entity_valid(const ecs::Entity& entity) const = 0;
        
        #if 0
        bool Scene::entity_parent_registered(const Entity& entity)
        {
            assert(registry->all_of<HeaderComponent>(entity));
            auto& header = registry->get<HeaderComponent>(entity);
            auto entity_parent = Entity{ entt::entity{header.entity_parent} };
            // auto entity_parent = get_entity_parent(entity);

            if (entity_parent.is_null()) return true;
            return scenegraph->tree.contains(entity_parent);
        }

        void Scene::reparent_entity(const Entity& entity, const Entity& parent_entity)
        {
            assert(registry->all_of<HeaderComponent>(entity));
            registry->get<HeaderComponent>(entity).entity_parent = parent_entity;

            scenegraph->reparent(entity, parent_entity);
        }

        void Scene::set_entity_header_parent(const Entity& entity, const Entity& entity_parent)
        {
            assert(registry->all_of<HeaderComponent>(entity));
            registry->get<HeaderComponent>(entity).entity_parent = entity_parent;

            // register_entity(entity);
        }

        void Scene::register_entity(const Entity& entity)
        {
            assert(registry->all_of<HeaderComponent>(entity));

            auto& header = registry->get<HeaderComponent>(entity);
            auto& chunk_tag = header.chunk_tag;
            auto entity_parent = Entity{ entt::entity{header.entity_parent} };

            chunk_registry.add_entity(header.chunk_tag, entity);

            if (entity_parent.is_null())
            {
                scenegraph->insert_node(entity);
            }
            else
            {
                assert(scenegraph->tree.contains(entity_parent));
                scenegraph->insert_node(entity, entity_parent);
            }
        }

        Entity Scene::create_empty_entity(const Entity& entity_hint)
        {
            if (entity_hint.is_null())
                return Entity{ registry->create() };

            Entity entity = Entity{ registry->create(entity_hint) };
            assert(entity == entity_hint);
            return entity;
        }

        Entity Scene::create_entity(
            const std::string& chunk_tag,
            const std::string& name,
            const Entity& entity_parent,
            const Entity& entity_hint)
        {
            Entity entity = create_empty_entity(entity_hint);

            std::string used_name = name.size() ? name : std::to_string(entity.to_integral());
            std::string used_chunk_tag = chunk_tag.size() ? chunk_tag : "default_chunk";
            uint32_t guid = 0;
            registry->emplace<HeaderComponent>(entity, used_name, used_chunk_tag, guid, entity_parent);

#if 1
            register_entity(entity);
#else
            chunk_registry.addEntity(used_chunk_tag, entity);

            // std::cout << "Scene::create_entity_and_attach_to_scenegraph " << entt::to_integral(entity) << std::endl; //
            if (entity_parent == entt::null)
            {
                scenegraph.create_node(entity);
            }
            else
            {
                assert(scenegraph.tree.contains(entity_parent));
                scenegraph.create_node(entity, entity_parent);
            }
#endif

            std::cout << "Scene::create_entity " << entity.to_integral() << std::endl;
            return entity;
        }

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

        void Scene::queue_entity_for_destruction(const Entity& entity)
        {
            // entities_pending_destruction.push_back(entity);
            entities_pending_destruction.push_back(entity);
        }

        void Scene::destroy_pending_entities()
        {
            int count = 0;
            while (entities_pending_destruction.size())
            {
                auto entity = entities_pending_destruction.front();
                entities_pending_destruction.pop_front();

                // Remove from chunk registry
                assert(registry->valid(entity));
                assert(registry->all_of<HeaderComponent>(entity));
                chunk_registry.remove_entity(registry->get<HeaderComponent>(entity).chunk_tag, entity);

                // Remove from scene graph
                if (scenegraph->tree.contains(entity))
                    scenegraph->erase_node(entity);

                // Destroy entity. May lead to additional entities being added to the queue.
                // registry->destroy(entity);
                registry->destroy(entity, 0); // TODO: This is a fix to keep generations of entities equal

                count++;
            }

            if (count)
                eeng::Log("%i entities destroyed", count);
        }
#endif

        virtual entt::registry& registry() noexcept = 0;
        virtual const entt::registry& registry() const noexcept = 0;
        virtual std::weak_ptr<entt::registry> registry_wptr() noexcept = 0;
        virtual std::weak_ptr<const entt::registry> registry_wptr() const noexcept = 0;

    };
} // namespace eeng