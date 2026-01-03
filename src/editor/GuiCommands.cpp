//
//  EditComponentCommand.cpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include <iostream>
#include <cassert>
#include <unordered_map>
#include <utility>
#include "MetaSerialize.hpp"
#include "GuiCommands.hpp"
#include "BatchRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "meta/EntityMetaHelpers.hpp"
#include "meta/MetaAux.h"
#include "MetaLiterals.h"

// Used by Copy command
#include "ecs/SceneGraph.hpp"

namespace
{
    using eeng::Guid;
    using eeng::EngineContext;
    using eeng::EngineContextWeakPtr;
    using eeng::EntityManager;
    using eeng::ecs::Entity;

    struct HeaderJsonKeys
    {
        std::string type_id;
        std::string guid_key;
        std::string parent_key;
        std::string entityref_guid_key;
    };

    HeaderJsonKeys resolve_header_keys()
    {
        HeaderJsonKeys keys{};

        auto header_type = eeng::meta::resolve_by_type_id_string("eeng.ecs.HeaderComponent");
        if (!header_type)
            return keys;

        keys.type_id = eeng::meta::get_meta_type_id_string(header_type);

        for (auto [id, meta_data] : header_type.data())
        {
            if (eeng::DataMetaInfo* info = meta_data.custom(); info)
            {
                if (info->name == "guid")
                    keys.guid_key = get_meta_data_display_name(id, meta_data);
                else if (info->name == "parent_entity")
                    keys.parent_key = get_meta_data_display_name(id, meta_data);
            }
        }

        auto entity_ref_type = eeng::meta::resolve_by_type_id_string("eeng.ecs.EntityRef");
        if (entity_ref_type)
        {
            for (auto [id, meta_data] : entity_ref_type.data())
            {
                if (eeng::DataMetaInfo* info = meta_data.custom(); info && info->name == "guid")
                {
                    keys.entityref_guid_key = get_meta_data_display_name(id, meta_data);
                    break;
                }
            }
        }

        return keys;
    }

    const HeaderJsonKeys& header_keys()
    {
        static HeaderJsonKeys keys{};
        static bool initialized = false;

        if (!initialized || keys.type_id.empty())
        {
            keys = resolve_header_keys();
            initialized = true;
        }

        return keys;
    }

    void ensure_storage(entt::registry& registry, entt::id_type component_id)
    {
        if (!registry.storage(component_id))
        {
            auto meta_type = entt::resolve(component_id);
            assert(meta_type);

            auto assure_fn = meta_type.func(eeng::literals::assure_component_storage_hs);
            assert(assure_fn);

            auto result = assure_fn.invoke(
                {},
                entt::forward_as_meta(registry)
            );
            if (!result)
            {
                throw std::runtime_error(
                    "Failed to invoke assure_storage for " + eeng::meta::get_meta_type_display_name(meta_type));
            }
        }
    }

    bool try_capture_guid(EntityManager& em, const Entity& entity, Guid& guid)
    {
        if (!entity.has_id())
            return false;
        if (!em.entity_valid(entity))
            return false;
        if (!em.scene_graph().contains(entity))
            return false;
        guid = em.get_entity_guid(entity);
        return guid.valid();
    }

    Entity resolve_entity_from_guid(EntityManager& em, const Guid& guid)
    {
        if (!guid.valid())
            return Entity{};
        auto entity_opt = em.get_entity_from_guid(guid);
        if (!entity_opt || !entity_opt->has_id())
            return Entity{};
        if (!em.entity_valid(*entity_opt))
            return Entity{};
        return *entity_opt;
    }

    bool update_entity_guid_in_json(nlohmann::json& entity_json, const Guid& guid)
    {
        const auto& keys = header_keys();
        if (!entity_json.contains("components"))
            return false;

        const auto old_guid = entity_json.value("entity_guid", Guid::invalid().raw());
        entity_json["entity_guid"] = guid.raw();

        auto& components = entity_json["components"];
        if (!components.is_object())
            return false;

        auto update_guid_field = [&](nlohmann::json& comp_json) -> bool
            {
                if (!comp_json.is_object())
                    return false;

                if (!keys.guid_key.empty() && comp_json.contains(keys.guid_key))
                {
                    comp_json[keys.guid_key] = guid.raw();
                    return true;
                }

                if (comp_json.contains("Guid"))
                {
                    comp_json["Guid"] = guid.raw();
                    return true;
                }

                if (comp_json.contains("guid"))
                {
                    comp_json["guid"] = guid.raw();
                    return true;
                }

                return false;
            };

        if (!keys.type_id.empty())
        {
            auto it = components.find(keys.type_id);
            if (it != components.end() && update_guid_field(it.value()))
                return true;
        }

        for (auto& [comp_name, comp_json] : components.items())
        {
            if (!comp_json.is_object())
                continue;

            if (comp_json.contains("Parent Entity") || comp_json.contains("parent_entity"))
            {
                if (update_guid_field(comp_json))
                    return true;
            }
        }

        for (auto& [comp_name, comp_json] : components.items())
        {
            if (!comp_json.is_object())
                continue;

            if (comp_json.contains("Guid") && comp_json["Guid"].is_number_unsigned()
                && comp_json["Guid"].get<Guid::underlying_type>() == old_guid)
            {
                comp_json["Guid"] = guid.raw();
                return true;
            }

            if (comp_json.contains("guid") && comp_json["guid"].is_number_unsigned()
                && comp_json["guid"].get<Guid::underlying_type>() == old_guid)
            {
                comp_json["guid"] = guid.raw();
                return true;
            }
        }

        return false;
    }

    bool update_parent_guid_in_json(nlohmann::json& entity_json, const Guid& parent_guid)
    {
        const auto& keys = header_keys();
        if (!entity_json.contains("components"))
            return false;

        auto& components = entity_json["components"];
        if (!components.is_object())
            return false;

        if (!keys.type_id.empty() && !keys.parent_key.empty())
        {
            auto it = components.find(keys.type_id);
            if (it != components.end() && it.value().is_object())
            {
                auto& header_json = it.value();
                if (header_json.contains(keys.parent_key))
                {
                    auto& parent_json = header_json[keys.parent_key];
                    if (!keys.entityref_guid_key.empty())
                    {
                        parent_json[keys.entityref_guid_key] = parent_guid.raw();
                        return true;
                    }
                    if (parent_json.contains("Guid"))
                    {
                        parent_json["Guid"] = parent_guid.raw();
                        return true;
                    }
                    if (parent_json.contains("guid"))
                    {
                        parent_json["guid"] = parent_guid.raw();
                        return true;
                    }
                }
            }
        }

        for (auto& [comp_name, comp_json] : components.items())
        {
            if (!comp_json.is_object())
                continue;

            if (comp_json.contains("Parent Entity"))
            {
                auto& parent_json = comp_json["Parent Entity"];
                if (parent_json.contains("Guid"))
                {
                    parent_json["Guid"] = parent_guid.raw();
                    return true;
                }
                if (parent_json.contains("guid"))
                {
                    parent_json["guid"] = parent_guid.raw();
                    return true;
                }
            }

            if (comp_json.contains("parent_entity"))
            {
                auto& parent_json = comp_json["parent_entity"];
                if (parent_json.contains("Guid"))
                {
                    parent_json["Guid"] = parent_guid.raw();
                    return true;
                }
                if (parent_json.contains("guid"))
                {
                    parent_json["guid"] = parent_guid.raw();
                    return true;
                }
            }
        }

        return false;
    }

    Guid guid_from_json(const nlohmann::json& entity_json)
    {
        if (!entity_json.contains("entity_guid"))
            return Guid::invalid();
        return Guid{ entity_json["entity_guid"].get<Guid::underlying_type>() };
    }

    void bind_refs_for_entity(Entity entity, EngineContext& ctx)
    {
        if (ctx.resource_manager)
            eeng::meta::bind_asset_refs_for_entity(entity, ctx);
        eeng::meta::bind_entity_refs_for_entity(entity, ctx);
    }

    void mark_batch_dirty_for_entity(Entity entity, EngineContext& ctx)
    {
        if (!ctx.batch_registry)
            return;
        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        br.mark_closure_dirty_for_entity(entity, ctx);
    }
}

namespace eeng::editor {
    using ecs::Entity;

    CreateEntityCommand::CreateEntityCommand(
        const Entity& parent_entity,
        EngineContextWeakPtr ctx) :
        parent_entity(parent_entity),
        ctx(std::move(ctx)),
        display_name("Create Entity") {
    }

    CommandStatus CreateEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);

        // First execution: create a fresh entity and capture a redo snapshot.
        if (entity_json.is_null())
        {
            Entity parent_current{};
            if (parent_entity.has_id()
                && em.entity_valid(parent_entity)
                && em.scene_graph().contains(parent_entity))
            {
                parent_current = parent_entity;
            }

            auto [guid, entity] = ctx_sp->entity_manager->create_entity_live_parent(
                "",
                "",
                parent_current,
                Entity{});
            created_guid = guid;
            created_entity = entity;

            auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
            if (!registry_sp)
                return CommandStatus::Done;

            entity_json = meta::serialize_entity_for_undo(
                static_cast<EntityManager&>(*ctx_sp->entity_manager).get_entity_ref(created_entity),
                registry_sp);
            if (!created_guid.valid())
                created_guid = guid_from_json(entity_json);
        }
        // Redo path: recreate from the serialized snapshot.
        else
        {
            auto er = meta::deserialize_entity_and_register_for_undo(
                entity_json,
                *ctx_sp);
            created_entity = er.entity;
            created_guid = er.guid;
            bind_refs_for_entity(created_entity, *ctx_sp);
        }

        if (created_entity.has_id())
            mark_batch_dirty_for_entity(created_entity, *ctx_sp);

        // std::cout << "CreateEntityCommand::execute() " << entt::to_integral(created_entity) << std::endl;
        return CommandStatus::Done;
    }

    CommandStatus CreateEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        // If we never created a live entity, only proceed if a GUID was recorded.
        if (!created_entity.has_id())
        {
            if (!created_guid.valid())
                return CommandStatus::Done;
        }

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (created_guid.valid())
        {
            auto entity_opt = em.get_entity_from_guid(created_guid);
            if (entity_opt && entity_opt->has_id())
            {
                mark_batch_dirty_for_entity(*entity_opt, *ctx_sp);
                ctx_sp->entity_manager->queue_entity_for_destruction(*entity_opt);
            }
            return CommandStatus::Done;
        }

        if (created_entity.has_id())
        {
            mark_batch_dirty_for_entity(created_entity, *ctx_sp);
            ctx_sp->entity_manager->queue_entity_for_destruction(created_entity);
        }
        // destroy_func(created_entity);

        // std::cout << "CreateEntityCommand::undo() " << entt::to_integral(created_entity) << std::endl;
        return CommandStatus::Done;
    }

    std::string CreateEntityCommand::get_name() const
    {
        return display_name;
    }

    // ------------------------------------------------------------------------

    DestroyEntityCommand::DestroyEntityCommand(
        const Entity& entity,
        EngineContextWeakPtr ctx
        // const DestroyEntityFunc&& destroy_func
    ) :
        entity(entity),
        ctx(std::move(ctx))
        // destroy_func(destroy_func)
    {
        display_name = std::string("Destroy Entity ") + std::to_string(entity.to_integral());
    }

    CommandStatus DestroyEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (!entity_guid.valid())
        {
            if (!try_capture_guid(em, entity, entity_guid))
                return CommandStatus::Done;
        }

        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        if (entity_json.is_null())
        {
            entity_json = meta::serialize_entity_for_undo(
                em.get_entity_ref(entity_current),
                registry_sp);
        }

        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        ctx_sp->entity_manager->queue_entity_for_destruction(entity_current);
        // destroy_func(entity);
        return CommandStatus::Done;
    }

    CommandStatus DestroyEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (entity_json.is_null())
            return CommandStatus::Done;

        auto er = meta::deserialize_entity_and_register_for_undo(
            entity_json,
            *ctx_sp);
        entity = er.entity;
        entity_guid = er.guid;
        bind_refs_for_entity(er.entity, *ctx_sp);
        if (er.entity.has_id())
            mark_batch_dirty_for_entity(er.entity, *ctx_sp);
        return CommandStatus::Done;
    }

    std::string DestroyEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- DestroyEntityBranchCommand ----------------------------------------

    DestroyEntityBranchCommand::DestroyEntityBranchCommand(
        const Entity& entity,
        EngineContextWeakPtr ctx
    ) :
        root_entity(entity),
        ctx(std::move(ctx))
    {
        display_name = std::string("Destroy Entity Branch ") + std::to_string(entity.to_integral());
    }

    CommandStatus DestroyEntityBranchCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);

        if (branch_json.is_null())
        {
            if (!root_guid.valid())
            {
                if (!try_capture_guid(em, root_entity, root_guid))
                    return CommandStatus::Done;
            }

            const auto root_current = resolve_entity_from_guid(em, root_guid);
            if (!root_current.has_id())
                return CommandStatus::Done;

            auto& scenegraph = em.scene_graph();
            if (!scenegraph.contains(root_current))
                return CommandStatus::Done;
            auto branch = scenegraph.get_branch_topdown(root_current);

            branch_json = nlohmann::json::array();

            for (const auto& entity : branch)
            {
                branch_json.push_back(meta::serialize_entity_for_undo(
                    em.get_entity_ref(entity),
                    registry_sp));
            }
        }

        if (!branch_json.is_array())
            return CommandStatus::Done;

        for (auto it = branch_json.rbegin(); it != branch_json.rend(); ++it)
        {
            auto guid = guid_from_json(*it);
            if (!guid.valid())
                continue;

            auto entity_opt = em.get_entity_from_guid(guid);
            if (!entity_opt || !entity_opt->has_id())
                continue;
            mark_batch_dirty_for_entity(*entity_opt, *ctx_sp);
            ctx_sp->entity_manager->queue_entity_for_destruction(*entity_opt);
        }
        return CommandStatus::Done;
    }

    CommandStatus DestroyEntityBranchCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (branch_json.is_null() || !branch_json.is_array())
            return CommandStatus::Done;

        std::vector<ecs::Entity> created_entities;
        created_entities.reserve(branch_json.size());

        for (const auto& entity_json : branch_json)
        {
            auto er = meta::deserialize_entity_for_undo(
                entity_json,
                *ctx_sp);
            created_entities.push_back(er.entity);
        }

        ctx_sp->entity_manager->register_entities_from_deserialization(created_entities);

        for (auto entity : created_entities)
        {
            bind_refs_for_entity(entity, *ctx_sp);
            mark_batch_dirty_for_entity(entity, *ctx_sp);
        }
        return CommandStatus::Done;
    }

    std::string DestroyEntityBranchCommand::get_name() const
    {
        return display_name;
    }

    // --- CopyEntityCommand (REMOVE) --------------------------------------------------

    CopyEntityCommand::CopyEntityCommand(
        const Entity& entity,
        EngineContextWeakPtr ctx) :
        entity_source(entity),
        ctx(std::move(ctx))
    {
        display_name = std::string("Copy Entity ") + std::to_string(entity.to_integral());
    }

    CommandStatus CopyEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);

        if (copy_json.is_null())
        {
            if (!source_guid.valid())
            {
                if (!try_capture_guid(em, entity_source, source_guid))
                    return CommandStatus::Done;
            }

            const auto source_current = resolve_entity_from_guid(em, source_guid);
            if (!source_current.has_id())
                return CommandStatus::Done;

            copy_json = meta::serialize_entity_for_undo(em.get_entity_ref(source_current), registry_sp);
            const Guid new_guid = Guid::generate();
            if (!update_entity_guid_in_json(copy_json, new_guid))
            {
                copy_json = nlohmann::json{};
                return CommandStatus::Done;
            }

            const auto parent_ref = em.get_entity_parent(source_current);
            if (!update_parent_guid_in_json(copy_json, parent_ref.guid))
            {
                copy_json = nlohmann::json{};
                return CommandStatus::Done;
            }
        }

        auto er = meta::deserialize_entity_and_register_for_undo(
            copy_json,
            *ctx_sp);
        const auto entity_copy = er.entity;
        bind_refs_for_entity(entity_copy, *ctx_sp);
        if (entity_copy.has_id())
            mark_batch_dirty_for_entity(entity_copy, *ctx_sp);
        return CommandStatus::Done;

        // assert(entity != entt::null);
        // entity_json = Meta::serialize_entities(&entity, 1, context.registry);
        // destroy_func(entity);
    }

    CommandStatus CopyEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (copy_json.is_null())
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        auto guid = guid_from_json(copy_json);
        if (!guid.valid())
            return CommandStatus::Done;

        auto entity_opt = em.get_entity_from_guid(guid);
        if (!entity_opt || !entity_opt->has_id())
            return CommandStatus::Done;

        mark_batch_dirty_for_entity(*entity_opt, *ctx_sp);
        ctx_sp->entity_manager->queue_entity_for_destruction(*entity_opt);

        // Meta::deserialize_entities(entity_json, context);

        // entity_json = nlohmann::json{};
        return CommandStatus::Done;
    }

    std::string CopyEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- CopyEntityBranchCommand --------------------------------------------

    CopyEntityBranchCommand::CopyEntityBranchCommand(
        const Entity& entity,
        EngineContextWeakPtr ctx) :
        root_entity(entity),
        ctx(std::move(ctx))
    {
        display_name = std::string("Copy Entity ") + std::to_string(entity.to_integral());
    }

    CommandStatus CopyEntityBranchCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        auto& scenegraph = em.scene_graph();

        if (branch_json.is_null())
        {
            if (!root_guid.valid())
            {
                if (!try_capture_guid(em, root_entity, root_guid))
                    return CommandStatus::Done;
            }

            const auto root_current = resolve_entity_from_guid(em, root_guid);
            if (!root_current.has_id())
                return CommandStatus::Done;
            if (!scenegraph.contains(root_current))
                return CommandStatus::Done;

            auto source_entities = scenegraph.get_branch_topdown(root_current);
            std::unordered_map<Entity, Guid> guid_map;
            guid_map.reserve(source_entities.size());

            branch_json = nlohmann::json::array();

            for (size_t i = 0; i < source_entities.size(); ++i)
            {
                const auto source_entity = source_entities[i];
                auto entity_json = meta::serialize_entity_for_undo(
                    em.get_entity_ref(source_entity),
                    registry_sp);

                const Guid new_guid = Guid::generate();
                guid_map[source_entity] = new_guid;

                if (!update_entity_guid_in_json(entity_json, new_guid))
                {
                    branch_json = nlohmann::json{};
                    return CommandStatus::Done;
                }

                Guid parent_guid = Guid::invalid();
                if (source_entity != root_current)
                {
                    const auto source_parent = scenegraph.get_parent(source_entity);
                    auto it = guid_map.find(source_parent);
                    if (it != guid_map.end())
                        parent_guid = it->second;
                }
                else
                {
                    parent_guid = em.get_entity_parent(source_entity).guid;
                }
                if (!update_parent_guid_in_json(entity_json, parent_guid))
                {
                    branch_json = nlohmann::json{};
                    return CommandStatus::Done;
                }

                branch_json.push_back(std::move(entity_json));
            }
        }

        if (!branch_json.is_array())
            return CommandStatus::Done;

        std::vector<Entity> created_entities;
        created_entities.reserve(branch_json.size());

        for (const auto& entity_json : branch_json)
        {
            auto er = meta::deserialize_entity_for_undo(
                entity_json,
                *ctx_sp);
            created_entities.push_back(er.entity);
        }

        ctx_sp->entity_manager->register_entities_from_deserialization(created_entities);

        for (auto entity : created_entities)
        {
            bind_refs_for_entity(entity, *ctx_sp);
            mark_batch_dirty_for_entity(entity, *ctx_sp);
        }
        return CommandStatus::Done;
    }

    CommandStatus CopyEntityBranchCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (branch_json.is_null() || !branch_json.is_array())
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);

        for (auto it = branch_json.rbegin(); it != branch_json.rend(); ++it)
        {
            const auto guid = guid_from_json(*it);
            if (!guid.valid())
                continue;

            auto entity_opt = em.get_entity_from_guid(guid);
            if (!entity_opt || !entity_opt->has_id())
                continue;
            mark_batch_dirty_for_entity(*entity_opt, *ctx_sp);
            ctx_sp->entity_manager->queue_entity_for_destruction(*entity_opt);
        }
        return CommandStatus::Done;
    }

    std::string CopyEntityBranchCommand::get_name() const
    {
        return display_name;
    }

    // --- ReparentEntityBranchCommand --------------------------------------------

    ReparentEntityBranchCommand::ReparentEntityBranchCommand(
        const Entity& entity,
        const Entity& parent_entity,
        EngineContextWeakPtr ctx) :
        entity(entity),
        new_parent_entity(parent_entity),
        ctx(std::move(ctx))
    {
        display_name = std::string("Reparent Entity ")
            + std::to_string(entity.to_integral())
            + " to "
            + std::to_string(parent_entity.to_integral());
    }

    CommandStatus ReparentEntityBranchCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        auto& scenegraph = em.scene_graph();

        if (!entity_guid.valid())
        {
            if (!entity.has_id())
                return CommandStatus::Done;
            if (!em.entity_valid(entity))
                return CommandStatus::Done;
            if (!scenegraph.contains(entity))
                return CommandStatus::Done;
            entity_guid = em.get_entity_guid(entity);
        }

        auto entity_opt = em.get_entity_from_guid(entity_guid);
        if (!entity_opt || !entity_opt->has_id())
            return CommandStatus::Done;
        if (!scenegraph.contains(*entity_opt))
            return CommandStatus::Done;

        auto entity_current = *entity_opt;

        if (!new_parent_guid.valid() && new_parent_entity.has_id())
        {
            if (em.entity_valid(new_parent_entity) && scenegraph.contains(new_parent_entity))
                new_parent_guid = em.get_entity_guid(new_parent_entity);
        }

        if (scenegraph.is_root(entity_current))
        {
            prev_parent_guid = Guid::invalid();
        }
        else
        {
            prev_parent_guid = em.get_entity_parent(entity_current).guid;
        }

        Entity parent_current{};
        if (new_parent_guid.valid())
        {
            auto parent_opt = em.get_entity_from_guid(new_parent_guid);
            if (!parent_opt || !parent_opt->has_id())
                return CommandStatus::Done;
            if (!scenegraph.contains(*parent_opt))
                return CommandStatus::Done;
            parent_current = *parent_opt;
        }

        // Transform dirtying on reparent is handled by EntityManager.
        ctx_sp->entity_manager->reparent_entity(entity_current, parent_current);
        return CommandStatus::Done;
    }

    CommandStatus ReparentEntityBranchCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        auto& scenegraph = em.scene_graph();

        if (!entity_guid.valid())
            return CommandStatus::Done;

        auto entity_opt = em.get_entity_from_guid(entity_guid);
        if (!entity_opt || !entity_opt->has_id())
            return CommandStatus::Done;
        if (!scenegraph.contains(*entity_opt))
            return CommandStatus::Done;

        Entity parent_current{};
        if (prev_parent_guid.valid())
        {
            auto parent_opt = em.get_entity_from_guid(prev_parent_guid);
            if (!parent_opt || !parent_opt->has_id())
                return CommandStatus::Done;
            if (!scenegraph.contains(*parent_opt))
                return CommandStatus::Done;
            parent_current = *parent_opt;
        }

        // Transform dirtying on reparent is handled by EntityManager.
        ctx_sp->entity_manager->reparent_entity(*entity_opt, parent_current);
        return CommandStatus::Done;
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
        EngineContextWeakPtr ctx) :
        entity(entity),
        comp_id(comp_id),
        ctx(std::move(ctx))
    {
        display_name = std::string("Add Component ")
            + std::to_string(comp_id)
            + " to Entity "
            + std::to_string(entity.to_integral());
    }

    CommandStatus AddComponentToEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (!entity_guid.valid())
        {
            if (!try_capture_guid(em, entity, entity_guid))
                return CommandStatus::Done;
        }
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        assert(!storage->contains(entity_current));

        storage->push(entity_current);
        return CommandStatus::Done;
    }

    CommandStatus AddComponentToEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        assert(storage->contains(entity_current));

        storage->remove(entity_current);
        return CommandStatus::Done;
    }

    std::string AddComponentToEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- RemoveComponentFromEntityCommand ----------------------------------------

    RemoveComponentFromEntityCommand::RemoveComponentFromEntityCommand(
        const Entity& entity,
        entt::id_type comp_id,
        EngineContextWeakPtr ctx) :
        entity(entity),
        comp_id(comp_id),
        ctx(std::move(ctx))
    {
        display_name = std::string("Remove Component ")
            + std::to_string(comp_id)
            + " from Entity "
            + std::to_string(entity.to_integral());
    }

    CommandStatus RemoveComponentFromEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (!entity_guid.valid())
        {
            if (!try_capture_guid(em, entity, entity_guid))
                return CommandStatus::Done;
        }
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        assert(storage->contains(entity_current));

        // Fetch component type
        auto comp_type = entt::resolve(comp_id);
        // Fetch component
        auto comp_any = comp_type.from_void(storage->value(entity_current));
        // Serialize component
        comp_json = meta::serialize_any(
            comp_type.from_void(storage->value(entity_current)),
            eeng::meta::SerializationPurpose::undo);
        // Remove component from entity
        storage->remove(entity_current);
        return CommandStatus::Done;
    }

    CommandStatus RemoveComponentFromEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        assert(!storage->contains(entity_current));

        // Fetch component type
        auto comp_type = entt::resolve(comp_id);
        // Default-construct component
        auto comp_any = comp_type.construct();
        // Deserialize component
        meta::deserialize_any(
            comp_json,
            comp_any,
            entity_current,
            *ctx_sp,
            eeng::meta::SerializationPurpose::undo);
        // Add component to entity
        storage->push(entity_current, comp_any.base().data());
        return CommandStatus::Done;
    }

    std::string RemoveComponentFromEntityCommand::get_name() const
    {
        return display_name;
    }
} // namespace Editor
