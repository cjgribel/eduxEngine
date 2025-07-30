//
//  EditComponentCommand.cpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include <iostream>
#include <cassert>
#include "MetaSerialize.hpp"
#include "MetaClone.hpp"
#include "GuiCommands.hpp"
// #include "meta_aux.h"

// Used by Copy command
#include "SceneGraph.hpp"

namespace Editor {

    CreateEntityCommand::CreateEntityCommand(
        // const CreateEntityFunc&& create_func,
        // const DestroyEntityFunc&& destroy_func,
        const Entity& parent_entity,
        const Context& context) :
        // create_func(create_func),
        // destroy_func(destroy_func),
        parent_entity(parent_entity),
        context(context),
        display_name("Create Entity") {
    }

    void CreateEntityCommand::execute()
    {
        if (created_entity.is_null())
        {
            created_entity = context.create_entity(parent_entity, Entity{});
            // created_entity = create_func(parent_entity, entt::null);
        }
        else
        {
            auto entity = context.create_entity(parent_entity, created_entity);
            // auto entity = create_func(parent_entity, created_entity);
            assert(entity == created_entity);
            created_entity = entity;
        }

        // std::cout << "CreateEntityCommand::execute() " << entt::to_integral(created_entity) << std::endl;
    }

    void CreateEntityCommand::undo()
    {
        assert(!created_entity.is_null());
        context.destroy_entity(created_entity);
        // destroy_func(created_entity);

        // std::cout << "CreateEntityCommand::undo() " << entt::to_integral(created_entity) << std::endl;
    }

    std::string CreateEntityCommand::get_name() const
    {
        return display_name;
    }

    // ------------------------------------------------------------------------

    DestroyEntityCommand::DestroyEntityCommand(
        const Entity& entity,
        const Context& context
        // const DestroyEntityFunc&& destroy_func
    ) :
        entity(entity),
        context(context)
        // destroy_func(destroy_func)
    {
        display_name = std::string("Destroy Entity ") + std::to_string(entity.to_integral());
    }

    void DestroyEntityCommand::execute()
    {
        assert(!entity.is_null());
        entity_json = Meta::serialize_entities(&entity, 1, context.registry);
        context.destroy_entity(entity);
        // destroy_func(entity);
    }

    void DestroyEntityCommand::undo()
    {
        Meta::deserialize_entities(entity_json, context);

        entity_json = nlohmann::json{};
    }

    std::string DestroyEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- CopyEntityCommand (REMOVE) --------------------------------------------------

    CopyEntityCommand::CopyEntityCommand(
        const Entity& entity,
        const Context& context) :
        entity_source(entity),
        context(context)
    {
        display_name = std::string("Copy Entity ") + std::to_string(entity.to_integral());
    }

    void CopyEntityCommand::execute()
    {
        assert(entity_copy.is_null());
        assert(!entity_source.is_null());
        assert(context.registry->valid(entity_source)); // context.entity_valid

        entity_copy = context.create_empty_entity(Entity{}); // context.registry->create(); // context.create_empty_entity
        Editor::clone_entity(context.registry, entity_source, entity_copy);

        assert(context.can_register_entity(entity_copy));
        context.register_entity(entity_copy);

        // assert(entity != entt::null);
        // entity_json = Meta::serialize_entities(&entity, 1, context.registry);
        // destroy_func(entity);
    }

    void CopyEntityCommand::undo()
    {
        assert(!entity_copy.is_null());
        assert(context.registry->valid(entity_copy));

        context.destroy_entity(entity_copy);
        entity_copy = Entity{};

        // Meta::deserialize_entities(entity_json, context);

        // entity_json = nlohmann::json{};
    }

    std::string CopyEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- CopyEntityBranchCommand --------------------------------------------

    CopyEntityBranchCommand::CopyEntityBranchCommand(
        const Entity& entity,
        const Context& context) :
        root_entity(entity),
        context(context)
    {
        display_name = std::string("Copy Entity ") + std::to_string(entity.to_integral());
    }

    void CopyEntityBranchCommand::execute()
    {
        //assert(copied_entities.empty());
        assert(!root_entity.is_null());
        assert(context.entity_valid(root_entity));

        // Obtain entity branch
        assert(!context.scenegraph.expired());
        auto scenegraph = context.scenegraph.lock();
        // Top-down ensures that the parent is copied before the child
        source_entities = scenegraph->get_branch_topdown(root_entity);

        // Hints for copied entites:
        // either no hints (not a redo) or previously copied & destroyed entities (undo-redo)
        auto entity_hints = copied_entities;
        if (entity_hints.empty()) entity_hints.assign(source_entities.size(), Entity{});
        copied_entities.clear();

        // Create copies top-down and resolve new parents
        for (int i = 0; i < source_entities.size(); i++)
        {
            auto& source_entity = source_entities[i];

            // Copy entity
            Entity entity_copy = context.create_empty_entity(entity_hints[i]);
            Editor::clone_entity(context.registry, source_entity, entity_copy);

            // Update entity parent for all except the root entity
            if (source_entity != root_entity)
            {
                // Find index of source entity's parent within the branch
                auto source_entity_parent = scenegraph->get_parent(source_entity);
                int source_entity_parent_index = -1;
                // The parent is located in the [0, i[ range
                for (int j = 0; j < i; j++)
                {
                    if (source_entities[j] == source_entity_parent) source_entity_parent_index = j;
                }
                assert(source_entity_parent_index > -1);

                // Use index to obtain parent of the copied entity
                auto& new_parent = copied_entities[source_entity_parent_index];
                // Now set new parent in the entity header
                context.set_entity_header_parent(entity_copy, new_parent);
            }

            // Register copied entity
            assert(context.can_register_entity(entity_copy));
            context.register_entity(entity_copy);

            copied_entities.push_back(entity_copy);
        }
    }

    void CopyEntityBranchCommand::undo()
    {
        // Destroy bottom-up
        for (auto entity_it = copied_entities.rbegin();
            entity_it != copied_entities.rend();
            entity_it++)
        {
            context.destroy_entity(*entity_it);
        }
    }

    std::string CopyEntityBranchCommand::get_name() const
    {
        return display_name;
    }

    // --- ReparentEntityBranchCommand --------------------------------------------

    ReparentEntityBranchCommand::ReparentEntityBranchCommand(
        const Entity& entity,
        const Entity& parent_entity,
        const Context& context) :
        entity(entity),
        new_parent_entity(parent_entity),
        context(context)
    {
        display_name = std::string("Reparent Entity ")
            + std::to_string(entity.to_integral())
            + " to "
            + std::to_string(parent_entity.to_integral());
    }

    void ReparentEntityBranchCommand::execute()
    {
        assert(!context.scenegraph.expired());
        auto scenegraph = context.scenegraph.lock();

        if (scenegraph->is_root(entity))
            prev_parent_entity = Entity{};
        else
            prev_parent_entity = scenegraph->get_parent(entity);

        context.reparent_entity(entity, new_parent_entity);
    }

    void ReparentEntityBranchCommand::undo()
    {
        // assert(!context.scenegraph.expired());
        // auto scenegraph = context.scenegraph.lock();

        context.reparent_entity(entity, prev_parent_entity);
    }

    std::string ReparentEntityBranchCommand::get_name() const
    {
        return display_name;
    }

    // --- UnparentEntityBranchCommand --------------------------------------------

    // UnparentEntityBranchCommand::UnparentEntityBranchCommand(
    //     entt::entity entity,
    //     entt::entity parent_entity,
    //     const Context& context) :
    //     entity(entity),
    //     // new_parent_entity(parent_entity),
    //     context(context)
    // {
    //     display_name = std::string("Unparent Entity ") + std::to_string(entt::to_integral(entity));
    // }

    // void UnparentEntityBranchCommand::execute()
    // {
    //     assert(!context.scenegraph.expired());
    //     auto scenegraph = context.scenegraph.lock();

    //     if (scenegraph->is_root(entity))
    //     {
    //         return;
    //         // prev_parent_entity = entt::null;
    //     }
    //     // else
    //         prev_parent_entity = scenegraph->get_parent(entity);

    //     // scenegraph->reparent(entity, new_parent_entity);
    //     scenegraph->unparent(entity);
    // }

    // void UnparentEntityBranchCommand::undo()
    // {
    //     assert(!context.scenegraph.expired());
    //     auto scenegraph = context.scenegraph.lock();

    //     if (prev_parent_entity == entt::null)
    //     {
    //         assert(scenegraph->is_root(entity));
    //         // scenegraph->unparent(entity);
    //         return;
    //     }
    //     // else
    //         scenegraph->reparent(entity, prev_parent_entity);
    // }

    // std::string UnparentEntityBranchCommand::get_name() const
    // {
    //     return display_name;
    // }

    // --- AddComponentToEntityCommand ----------------------------------------

    AddComponentToEntityCommand::AddComponentToEntityCommand(
        const Entity& entity,
        entt::id_type comp_id,
        const Context& context) :
        entity(entity),
        comp_id(comp_id),
        context(context)
    {
        display_name = std::string("Add Component ")
            + std::to_string(comp_id)
            + " to Entity "
            + std::to_string(entity.to_integral());
    }

    void AddComponentToEntityCommand::execute()
    {
        // Fetch component storage
        auto storage = context.registry->storage(comp_id);
        assert(!storage->contains(entity));

        storage->push(entity);
    }

    void AddComponentToEntityCommand::undo()
    {
        // Fetch component storage
        auto storage = context.registry->storage(comp_id);
        assert(storage->contains(entity));

        storage->remove(entity);
    }

    std::string AddComponentToEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- RemoveComponentFromEntityCommand ----------------------------------------

    RemoveComponentFromEntityCommand::RemoveComponentFromEntityCommand(
        const Entity& entity,
        entt::id_type comp_id,
        const Context& context) :
        entity(entity),
        comp_id(comp_id),
        context(context)
    {
        display_name = std::string("Remove Component ")
            + std::to_string(comp_id)
            + " from Entity "
            + std::to_string(entity.to_integral());
    }

    void RemoveComponentFromEntityCommand::execute()
    {
        // Fetch component storage
        auto storage = context.registry->storage(comp_id);
        assert(storage->contains(entity));

        // Fetch component type
        auto comp_type = entt::resolve(comp_id);
        // Fetch component
        auto comp_any = comp_type.from_void(storage->value(entity));
        // Serialize component
        comp_json = Meta::serialize_any(comp_type.from_void(storage->value(entity)));
        // Remove component from entity
        storage->remove(entity);
    }

    void RemoveComponentFromEntityCommand::undo()
    {
        // Fetch component storage
        auto storage = context.registry->storage(comp_id);
        assert(!storage->contains(entity));

        // Fetch component type
        auto comp_type = entt::resolve(comp_id);
        // Default-construct component
        auto comp_any = comp_type.construct();
        // Deserialize component
        Meta::deserialize_any(comp_json, comp_any, entity, context);
        // Add component to entity
        storage->push(entity, comp_any.data());
    }

    std::string RemoveComponentFromEntityCommand::get_name() const
    {
        return display_name;
    }
} // namespace Editor