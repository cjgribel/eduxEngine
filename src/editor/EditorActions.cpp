// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/EditorActions.hpp"
#include "editor/CommandBatchHelpers.hpp"
#include "editor/CommandQueue.hpp"
#include "editor/GuiCommands.hpp"
#include "editor/BatchCommands.hpp"
#include "editor/content_generators/MannequinGraph.hpp"
#include "editor/content_generators/PistonGraph.hpp"
#include "ResourceManager.hpp"
#include "AssetMetaData.hpp"
#include "assets/importers/TerrainCooker.hpp"
#include "assets/types/AnimationGraphAsset.hpp"
#include "assets/types/ModelAssets.hpp"
#include "assets/types/TerrainAssets.hpp"
#include "ecs/EntityManager.hpp"
#include "ThreadPool.hpp"
#include "meta/MetaAux.h"
#include "LogMacros.h"
#include <array>
#include <cctype>
#include <fstream>
#include <memory>
#include <unordered_set>

namespace eeng::editor
{
    namespace
    {
        // Adds a component id only when the meta type resolves.
        void append_component_if_valid(
            std::vector<entt::id_type>& ids,
            entt::meta_type meta_type)
        {
            if (meta_type)
                ids.push_back(meta_type.id());
        }

        bool can_queue(EngineContext& ctx)
        {
        return static_cast<bool>(ctx.command_queue);
        }

        bool can_queue_action(EngineContext& ctx, const char* action_label)
        {
            if (!can_queue(ctx))
                return false;
            if (!ctx.command_queue->has_in_flight())
                return true;

            // Keep command order deterministic by ignoring new work while busy.
            EENG_LOG_WARN(&ctx, "%s ignored: command queue busy.", action_label);
            return false;
        }

        bool try_add_command(EngineContext& ctx, CommandPtr&& command, const char* action_label)
        {
            if (!ctx.command_queue->add(std::move(command)))
            {
                // Queue enforces the same policy as can_queue_action (defensive).
                EENG_LOG_WARN(&ctx, "%s ignored: command queue busy.", action_label);
                return false;
            }
            return true;
        }

        std::vector<ecs::Entity> filter_out_descendants(
            eeng::ecs::SceneGraph& scenegraph,
            const std::deque<ecs::Entity>& entities)
        {
            std::vector<ecs::Entity> filtered_entities;
            filtered_entities.reserve(entities.size());

            for (const auto& entity : entities)
            {
                bool is_child = false;
                for (const auto& entity_other : entities)
                {
                    if (entity == entity_other)
                        continue;
                    if (scenegraph.is_descendant_of(entity, entity_other))
                    {
                        is_child = true;
                        break;
                    }
                }
                if (!is_child)
                    filtered_entities.push_back(entity);
            }

            return filtered_entities;
        }

        std::string sanitize_asset_name(std::string name)
        {
            for (char& ch : name)
            {
                const unsigned char uch = static_cast<unsigned char>(ch);
                if (std::isalnum(uch) || ch == '_' || ch == '-')
                    continue;
                ch = '_';
            }
            while (!name.empty() && (name.back() == '_' || name.back() == ' '))
                name.pop_back();
            if (name.empty())
                name = "TerrainRecipe";
            return name;
        }

        std::string make_unique_asset_stem(
            const std::filesystem::path& directory,
            std::string stem)
        {
            stem = sanitize_asset_name(std::move(stem));
            std::string candidate = stem;
            int suffix = 1;
            while (std::filesystem::exists(directory / (candidate + ".json"))
                || std::filesystem::exists(directory / (candidate + ".meta.json")))
            {
                candidate = stem + "_" + std::to_string(suffix++);
            }
            return candidate;
        }

        std::deque<ecs::Entity> filter_out_read_only_entities(
            EngineContext& ctx,
            const std::deque<ecs::Entity>& selection,
            const char* context_label)
        {
            std::deque<ecs::Entity> filtered{};
            filtered.resize(0);

            for (const auto& entity : selection)
            {
                if (!entity.has_id())
                    continue;
                if (editor::is_entity_in_read_only_batch(entity, ctx, context_label))
                    continue;
                filtered.push_back(entity);
            }

            if (filtered.empty() && !selection.empty())
            {
                EENG_LOG_WARN(&ctx, "%s ignored: selection only contains generated/read-only terrain content.", context_label);
            }

            return filtered;
        }

        bool reject_read_only_batch_target(
            EngineContext& ctx,
            const BatchId& batch_id,
            const char* context_label)
        {
            if (!batch_id.valid())
                return false;
            if (!editor::is_batch_read_only(batch_id, ctx, context_label))
                return false;

            EENG_LOG_WARN(&ctx, "%s blocked: generated/read-only terrain batches are not editable targets.", context_label);
            return true;
        }

        bool path_is_within(const std::filesystem::path& child, const std::filesystem::path& parent)
        {
            if (child.empty() || parent.empty())
                return false;

            const auto child_norm = child.lexically_normal();
            const auto parent_norm = parent.lexically_normal();

            auto parent_it = parent_norm.begin();
            auto child_it = child_norm.begin();
            for (; parent_it != parent_norm.end() && child_it != child_norm.end(); ++parent_it, ++child_it)
            {
                if (*parent_it != *child_it)
                    return false;
            }
            return parent_it == parent_norm.end();
        }

        std::vector<BatchId> read_cooked_terrain_batch_ids(const std::filesystem::path& terrain_asset_path)
        {
            std::vector<BatchId> batch_ids{};

            std::ifstream file(terrain_asset_path);
            if (!file.is_open())
                return batch_ids;

            nlohmann::json j;
            try
            {
                file >> j;
            }
            catch (...)
            {
                return batch_ids;
            }

            const auto chunks_it = j.find("chunks");
            if (chunks_it == j.end() || !chunks_it->is_array())
                return batch_ids;

            for (const auto& elem : *chunks_it)
            {
                if (!elem.is_object() || !elem.contains("batch_id"))
                    continue;

                const auto& batch_json = elem["batch_id"];
                if (!batch_json.is_object() || !batch_json.contains("guid"))
                    continue;

                const auto raw = batch_json["guid"].get<Guid::underlying_type>();
                if (raw != 0)
                    batch_ids.emplace_back(raw);
            }

            return batch_ids;
        }

        std::vector<BatchId> collect_generated_terrain_batch_ids_for_recipe(
            EngineContext& ctx,
            const Guid& recipe_guid)
        {
            std::unordered_set<BatchId> batch_ids{};

            auto* batch_registry = dynamic_cast<BatchRegistry*>(ctx.batch_registry.get());
            auto* resource_manager = dynamic_cast<ResourceManager*>(ctx.resource_manager.get());
            if (!batch_registry || !resource_manager)
                return {};

            for (const BatchInfo* batch : batch_registry->list())
            {
                if (!batch)
                    continue;
                if (!batch->generated)
                    continue;
                if (batch->owner_guid != recipe_guid)
                    continue;
                if (batch->generator_tag != "terrain")
                    continue;
                batch_ids.insert(batch->id);
            }

            const auto index_data = resource_manager->get_index_data();
            if (index_data)
            {
                const auto recipe_it = index_data->by_guid.find(recipe_guid);
                if (recipe_it != index_data->by_guid.end() && recipe_it->second)
                {
                    const std::string recipe_name = sanitize_asset_name(recipe_it->second->meta.name);
                    const std::filesystem::path terrain_asset_path =
                        resource_manager->assets_root() / "terrain" / recipe_name / "terrain.json";
                    for (const auto& batch_id : read_cooked_terrain_batch_ids(terrain_asset_path))
                        batch_ids.insert(batch_id);
                }
            }

            return std::vector<BatchId>(batch_ids.begin(), batch_ids.end());
        }

        bool unload_generated_terrain_batches_for_recipe(
            EngineContext& ctx,
            const Guid& recipe_guid,
            std::string& error_out)
        {
            // Important: this preflight must run outside the RM strand. Batch
            // unload reaches back into ResourceManager to unbind/unload assets,
            // so doing both from the same serialized RM job can deadlock.
            auto* batch_registry = dynamic_cast<BatchRegistry*>(ctx.batch_registry.get());
            if (!batch_registry)
            {
                error_out = "valid BatchRegistry required";
                return false;
            }

            for (const auto& batch_id : collect_generated_terrain_batch_ids_for_recipe(ctx, recipe_guid))
            {
                if (!batch_id.valid() || !batch_registry->is_batch_loaded(batch_id))
                    continue;

                const TaskResult unload_result = batch_registry->queue_unload(batch_id, ctx).get();
                if (!unload_result.success)
                {
                    error_out = std::format(
                        "could not unload generated terrain batch {}",
                        batch_id.to_string());
                    return false;
                }
            }

            return true;
        }
    }

    void SceneActions::create_entity(EngineContext& ctx, const ecs::Entity& parent_entity)
    {
        if (parent_entity.has_id() && is_entity_in_read_only_batch(parent_entity, ctx, "CreateEntity"))
        {
            EENG_LOG_WARN(&ctx, "CreateEntity blocked: cannot create children under generated/read-only terrain content.");
            return;
        }
        if (!can_queue_action(ctx, "CreateEntity"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<CreateEntityCommand>(
                parent_entity,
                ctx_wptr),
            "CreateEntity");
    }

    void SceneActions::delete_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        const auto mutable_selection = filter_out_read_only_entities(ctx, selection, "DeleteEntities");
        if (mutable_selection.empty())
            return;
        if (!can_queue_action(ctx, "DeleteEntities"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto roots = filter_out_descendants(
            em.scene_graph(),
            mutable_selection);

        for (const auto& entity : roots)
        {
            try_add_command(
                ctx,
                CommandFactory::Create<DestroyEntityBranchCommand>(
                    entity,
                    ctx_wptr),
                "DeleteEntities");
        }
    }

    void SceneActions::copy_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        const auto mutable_selection = filter_out_read_only_entities(ctx, selection, "CopyEntities");
        if (mutable_selection.empty())
            return;
        if (!can_queue_action(ctx, "CopyEntities"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto roots = filter_out_descendants(
            em.scene_graph(),
            mutable_selection);

        for (const auto& entity : roots)
        {
            try_add_command(
                ctx,
                CommandFactory::Create<CopyEntityBranchCommand>(
                    entity,
                    ctx_wptr),
                "CopyEntities");
        }
    }

    void SceneActions::parent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        const auto mutable_selection = filter_out_read_only_entities(ctx, selection, "ParentEntities");
        if (mutable_selection.size() < 2)
            return;
        if (!can_queue_action(ctx, "ParentEntities"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();
        const auto new_parent = mutable_selection.back();
        if (is_entity_in_read_only_batch(new_parent, ctx, "ParentEntities"))
        {
            EENG_LOG_WARN(&ctx, "ParentEntities blocked: cannot parent under generated/read-only terrain content.");
            return;
        }

        for (const auto& entity : mutable_selection)
        {
            if (entity == new_parent)
                continue;
            if (scenegraph.is_descendant_of(new_parent, entity))
                continue;

            try_add_command(
                ctx,
                CommandFactory::Create<ReparentEntityBranchCommand>(
                    entity,
                    new_parent,
                    ctx_wptr),
                "ParentEntities");
        }
    }

    void SceneActions::unparent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        const auto mutable_selection = filter_out_read_only_entities(ctx, selection, "UnparentEntities");
        if (mutable_selection.empty())
            return;
        if (!can_queue_action(ctx, "UnparentEntities"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();

        for (const auto& entity : mutable_selection)
        {
            if (scenegraph.is_root(entity))
                continue;

            try_add_command(
                ctx,
                CommandFactory::Create<ReparentEntityBranchCommand>(
                    entity,
                    ecs::Entity{},
                    ctx_wptr),
                "UnparentEntities");
        }
    }

    void SceneActions::add_components(EngineContext& ctx, const std::deque<ecs::Entity>& selection, entt::id_type comp_id)
    {
        const auto mutable_selection = filter_out_read_only_entities(ctx, selection, "AddComponents");
        if (mutable_selection.empty() || comp_id == entt::id_type{})
            return;
        if (!can_queue_action(ctx, "AddComponents"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();

        // Treat RigidBodyComponent as a bundle (add companions automatically).
        const auto rb_meta = meta::resolve_by_type_id_string("eeng.ecs.RigidBodyComponent");
        const bool is_rigidbody = rb_meta && rb_meta.id() == comp_id;
        std::vector<entt::id_type> bundle_ids;
        if (is_rigidbody)
        {
            bundle_ids.reserve(5);
            bundle_ids.push_back(rb_meta.id());
            append_component_if_valid(bundle_ids, meta::resolve_by_type_id_string("eeng.ecs.ColliderComponent"));
            append_component_if_valid(bundle_ids, meta::resolve_by_type_id_string("eeng.ecs.PhysicsMaterialComponent"));
            append_component_if_valid(bundle_ids, meta::resolve_by_type_id_string("eeng.ecs.CollisionFilterComponent"));
            append_component_if_valid(bundle_ids, meta::resolve_by_type_id_string("eeng.ecs.PhysicsEventsComponent"));
        }

        for (const auto& entity : mutable_selection)
        {
            if (!entity.has_id())
                continue;
            if (!em.entity_valid(entity))
                continue;
            if (!scenegraph.contains(entity))
                continue;

            if (is_rigidbody)
            {
                for (const auto id : bundle_ids)
                {
                    try_add_command(
                        ctx,
                        CommandFactory::Create<AddComponentToEntityCommand>(
                            entity,
                            id,
                            ctx_wptr),
                        "AddComponents");
                }
            }
            else
            {
                try_add_command(
                    ctx,
                    CommandFactory::Create<AddComponentToEntityCommand>(
                        entity,
                        comp_id,
                        ctx_wptr),
                    "AddComponents");
            }
        }
    }

    void SceneActions::remove_components(EngineContext& ctx, const std::deque<ecs::Entity>& selection, entt::id_type comp_id)
    {
        const auto mutable_selection = filter_out_read_only_entities(ctx, selection, "RemoveComponents");
        if (mutable_selection.empty() || comp_id == entt::id_type{})
            return;
        if (!can_queue_action(ctx, "RemoveComponents"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();

        // Treat RigidBodyComponent as a bundle (remove companions automatically, keep colliders).
        const auto rb_meta = meta::resolve_by_type_id_string("eeng.ecs.RigidBodyComponent");
        const bool is_rigidbody = rb_meta && rb_meta.id() == comp_id;
        std::vector<entt::id_type> bundle_ids;
        if (is_rigidbody)
        {
            bundle_ids.reserve(4);
            bundle_ids.push_back(rb_meta.id());
            append_component_if_valid(bundle_ids, meta::resolve_by_type_id_string("eeng.ecs.PhysicsMaterialComponent"));
            append_component_if_valid(bundle_ids, meta::resolve_by_type_id_string("eeng.ecs.CollisionFilterComponent"));
            append_component_if_valid(bundle_ids, meta::resolve_by_type_id_string("eeng.ecs.PhysicsEventsComponent"));
        }

        for (const auto& entity : mutable_selection)
        {
            if (!entity.has_id())
                continue;
            if (!em.entity_valid(entity))
                continue;
            if (!scenegraph.contains(entity))
                continue;

            if (is_rigidbody)
            {
                for (const auto id : bundle_ids)
                {
                    try_add_command(
                        ctx,
                        CommandFactory::Create<RemoveComponentFromEntityCommand>(
                            entity,
                            id,
                            ctx_wptr),
                        "RemoveComponents");
                }
            }
            else
            {
                try_add_command(
                    ctx,
                    CommandFactory::Create<RemoveComponentFromEntityCommand>(
                        entity,
                        comp_id,
                        ctx_wptr),
                    "RemoveComponents");
            }
        }
    }

    void SceneActions::spawn_entity_branch_from_json(
        EngineContext& ctx,
        nlohmann::json branch_json,
        const ecs::Entity& parent_entity,
        bool remap_guids)
    {
        if (branch_json.is_null())
            return;
        if (parent_entity.has_id() && is_entity_in_read_only_batch(parent_entity, ctx, "SpawnEntityBranch"))
        {
            EENG_LOG_WARN(&ctx, "SpawnEntityBranch blocked: cannot spawn under generated/read-only terrain content.");
            return;
        }
        if (!can_queue_action(ctx, "SpawnEntityBranch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<SpawnEntityBranchCommand>(
                std::move(branch_json),
                parent_entity,
                ctx_wptr,
                remap_guids),
            "SpawnEntityBranch");
    }

    void SceneActions::bake_transform_branch(EngineContext& ctx, const ecs::Entity& root_entity)
    {
        if (!root_entity.has_id())
            return;
        if (is_entity_in_read_only_batch(root_entity, ctx, "BakeTransformBranch"))
        {
            EENG_LOG_WARN(&ctx, "BakeTransformBranch blocked: generated/read-only terrain content cannot be edited directly.");
            return;
        }
        if (!can_queue_action(ctx, "BakeTransformBranch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<BakeTransformBranchCommand>(
                root_entity,
                ctx_wptr),
            "BakeTransformBranch");
    }

    void AssetActions::import_model(
        EngineContext& ctx,
        const std::filesystem::path& source_file,
        assets::ImportFlags flags,
        std::string model_name,
        std::shared_ptr<std::atomic<bool>> in_flight)
    {
        if (!can_queue_action(ctx, "ImportModel"))
            return;

        if (in_flight)
            in_flight->store(true, std::memory_order_relaxed);

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
        {
            if (in_flight)
                in_flight->store(false, std::memory_order_relaxed);
            return;
        }

        if (!try_add_command(
                ctx,
                CommandFactory::Create<ImportModelCommand>(
                    source_file,
                    flags,
                    std::move(model_name),
                    ctx_wptr,
                    std::move(in_flight)),
                "ImportModel"))
        {
            if (in_flight)
                in_flight->store(false, std::memory_order_relaxed);
        }
    }

    void AssetActions::import_texture(
        EngineContext& ctx,
        const std::filesystem::path& source_file,
        std::string texture_name,
        std::shared_ptr<std::atomic<bool>> in_flight)
    {
        if (!can_queue_action(ctx, "ImportTexture"))
            return;

        if (in_flight)
            in_flight->store(true, std::memory_order_relaxed);

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
        {
            if (in_flight)
                in_flight->store(false, std::memory_order_relaxed);
            return;
        }

        if (!try_add_command(
                ctx,
                CommandFactory::Create<ImportTextureCommand>(
                    source_file,
                    std::move(texture_name),
                    ctx_wptr,
                    std::move(in_flight)),
                "ImportTexture"))
        {
            if (in_flight)
                in_flight->store(false, std::memory_order_relaxed);
        }
    }

    void AssetActions::import_animation_graph_mock(
        EngineContext& ctx,
        std::string graph_name,
        std::string clip_name)
    {
        if (!can_queue_action(ctx, "ImportAnimationGraphMock"))
            return;

        if (graph_name.empty() || clip_name.empty())
        {
            EENG_LOG_WARN(&ctx, "Animation graph import skipped: missing name or clip.");
            return;
        }

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        const auto& assets_root = rm.assets_root();
        if (assets_root.empty())
        {
            EENG_LOG_WARN(&ctx, "Animation graph import skipped: assets root not set.");
            return;
        }

        assets::AnimationGraphAsset graph =
            content_generators::build_mannequin_graph(graph_name, clip_name);

        AssetMetaData meta{};
        meta.guid = Guid::generate();
        meta.guid_parent = Guid::invalid();
        meta.name = graph_name;
        meta.type_id = "assets.AnimationGraphAsset";

        const auto graph_dir = assets_root / "graphs";
        const auto asset_path = graph_dir / (graph_name + ".json");
        const auto meta_path = graph_dir / (graph_name + ".meta.json");

        rm.queue_import_job(
            [graph = std::move(graph), meta = std::move(meta), asset_path, meta_path, assets_root]
            (ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Import;
                try
                {
                    std::filesystem::create_directories(asset_path.parent_path());
                    if (std::filesystem::exists(asset_path) || std::filesystem::exists(meta_path))
                        throw std::runtime_error("Animation graph already exists: " + asset_path.string());

                    rm.import(graph, asset_path.string(), meta, meta_path.string());
                    rm.scan_assets_async(assets_root, ctx);
                    res.add_result(meta.guid, true, "Import ok");
                }
                catch (const std::exception& ex)
                {
                    res.add_result(meta.guid, false, ex.what());
                }
                catch (...)
                {
                    res.add_result(meta.guid, false, "unknown exception in import job");
                }
                return res;
            },
            ctx);
    }

    void AssetActions::import_animation_graph_piston(
        EngineContext& ctx,
        std::string graph_name,
        std::string clip_name)
    {
        if (!can_queue_action(ctx, "ImportAnimationGraphPiston"))
            return;

        if (graph_name.empty() || clip_name.empty())
        {
            EENG_LOG_WARN(&ctx, "Piston animation graph import skipped: missing name or clip.");
            return;
        }

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        const auto& assets_root = rm.assets_root();
        if (assets_root.empty())
        {
            EENG_LOG_WARN(&ctx, "Piston animation graph import skipped: assets root not set.");
            return;
        }

        assets::AnimationGraphAsset graph =
            content_generators::build_piston_graph(graph_name, clip_name);

        AssetMetaData meta{};
        meta.guid = Guid::generate();
        meta.guid_parent = Guid::invalid();
        meta.name = graph_name;
        meta.type_id = "assets.AnimationGraphAsset";

        const auto graph_dir = assets_root / "graphs";
        const auto asset_path = graph_dir / (graph_name + ".json");
        const auto meta_path = graph_dir / (graph_name + ".meta.json");

        rm.queue_import_job(
            [graph = std::move(graph), meta = std::move(meta), asset_path, meta_path, assets_root]
            (ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Import;
                try
                {
                    std::filesystem::create_directories(asset_path.parent_path());
                    if (std::filesystem::exists(asset_path) || std::filesystem::exists(meta_path))
                        throw std::runtime_error("Animation graph already exists: " + asset_path.string());

                    rm.import(graph, asset_path.string(), meta, meta_path.string());
                    rm.scan_assets_async(assets_root, ctx);
                    res.add_result(meta.guid, true, "Import ok");
                }
                catch (const std::exception& ex)
                {
                    res.add_result(meta.guid, false, ex.what());
                }
                catch (...)
                {
                    res.add_result(meta.guid, false, "unknown exception in import job");
                }
                return res;
            },
            ctx);
    }

    void AssetActions::create_terrain_recipe(
        EngineContext& ctx,
        std::string recipe_name)
    {
        auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        const auto& assets_root = rm.assets_root();
        if (assets_root.empty())
        {
            EENG_LOG_WARN(&ctx, "Terrain recipe creation skipped: assets root not set.");
            return;
        }

        const auto recipe_dir = assets_root / "terrain_recipes";
        const std::string recipe_stem = make_unique_asset_stem(recipe_dir, std::move(recipe_name));
        const auto asset_path = recipe_dir / (recipe_stem + ".json");
        const auto meta_path = recipe_dir / (recipe_stem + ".meta.json");

        assets::TerrainRecipeAsset recipe{};
        AssetMetaData meta{};
        meta.guid = Guid::generate();
        meta.guid_parent = Guid::invalid();
        meta.name = recipe_stem;
        meta.type_id = meta::get_meta_type_id_string<assets::TerrainRecipeAsset>();

        rm.queue_import_job(
            [recipe = std::move(recipe), meta = std::move(meta), asset_path, meta_path, assets_root]
            (ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Import;
                try
                {
                    std::filesystem::create_directories(asset_path.parent_path());
                    rm.import(recipe, asset_path.string(), meta, meta_path.string());
                    rm.scan_assets_async(assets_root, ctx);
                    res.add_result(meta.guid, true, "Terrain recipe created");
                }
                catch (const std::exception& ex)
                {
                    res.add_result(meta.guid, false, ex.what());
                }
                return res;
            },
            ctx);
    }

    void AssetActions::cook_terrain_recipe(
        EngineContext& ctx,
        const Guid& recipe_guid)
    {
        if (!recipe_guid.valid())
        {
            EENG_LOG_WARN(&ctx, "Terrain cook skipped: invalid recipe guid.");
            return;
        }

        auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        const auto& assets_root = rm.assets_root();
        if (assets_root.empty())
        {
            EENG_LOG_WARN(&ctx, "Terrain cook skipped: assets root not set.");
            return;
        }
        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired() || !ctx.thread_pool)
            return;

        ctx.thread_pool->queue_task(
            [ctx_wptr, recipe_guid, assets_root]() mutable
            {
                auto ctx_sp = ctx_wptr.lock();
                if (!ctx_sp)
                    return;

                // Pre-unload generated terrain batches before entering the RM
                // import job. This keeps cook safe even when a designer has
                // the old generated terrain chunk batches loaded in the editor.
                std::string unload_error;
                if (!unload_generated_terrain_batches_for_recipe(*ctx_sp, recipe_guid, unload_error))
                {
                    EENG_LOG_WARN(ctx_sp.get(), "Terrain cook skipped: %s.", unload_error.c_str());
                    return;
                }

                auto& rm = static_cast<ResourceManager&>(*ctx_sp->resource_manager);
                rm.queue_import_job(
                    [recipe_guid, assets_root](ResourceManager& rm, EngineContext& ctx) -> TaskResult
                    {
                        TaskResult res;
                        res.type = TaskResult::TaskType::Import;
                        try
                        {
                            const auto cook_result = assets::TerrainCooker::cook_recipe(rm, recipe_guid, ctx);
                            if (!cook_result.success)
                            {
                                res.add_result(recipe_guid, false, cook_result.error_message);
                                return res;
                            }

                            rm.scan_assets_async(assets_root, ctx);
                            res.add_result(cook_result.terrain_guid, true, "Terrain cook ok");
                        }
                        catch (const std::exception& ex)
                        {
                            res.add_result(recipe_guid, false, ex.what());
                        }
                        return res;
                    },
                    *ctx_sp).get();
            });
    }

    /// @brief Clear cooked terrain batches and assets associated with a terrain recipe, and remove the cook output from disk, allowing for a fresh cook.
    /// @param ctx The engine context.
    /// @param recipe_guid The GUID of the terrain recipe.
    void AssetActions::clear_cooked_terrain(
        EngineContext& ctx,
        const Guid& recipe_guid)
    {
        if (!recipe_guid.valid())
        {
            EENG_LOG_WARN(&ctx, "Clear cooked terrain skipped: invalid recipe guid.");
            return;
        }

        auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        const auto& assets_root = rm.assets_root();
        if (assets_root.empty())
        {
            EENG_LOG_WARN(&ctx, "Clear cooked terrain skipped: assets root not set.");
            return;
        }

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired() || !ctx.thread_pool)
            return;

        ctx.thread_pool->queue_task(
            [ctx_wptr, recipe_guid, assets_root]() mutable
            {
                auto ctx_sp = ctx_wptr.lock();
                if (!ctx_sp)
                    return;

                // Clear follows the same preflight as cook: unload generated
                // terrain batches off the RM strand first, then remove the
                // deterministic cooked outputs on the RM strand.
                std::string unload_error;
                if (!unload_generated_terrain_batches_for_recipe(*ctx_sp, recipe_guid, unload_error))
                {
                    EENG_LOG_WARN(ctx_sp.get(), "Clear cooked terrain skipped: %s.", unload_error.c_str());
                    return;
                }

                auto& rm = static_cast<ResourceManager&>(*ctx_sp->resource_manager);
                rm.queue_import_job(
                    [recipe_guid, assets_root](ResourceManager& rm, EngineContext& ctx) -> TaskResult
                    {
                        TaskResult res;
                        res.type = TaskResult::TaskType::Unimport;

                        auto* batch_registry = dynamic_cast<BatchRegistry*>(ctx.batch_registry.get());
                        if (!batch_registry || batch_registry->index_path().empty())
                        {
                            res.add_result(recipe_guid, false, "Clear cooked terrain failed: valid BatchRegistry required.");
                            return res;
                        }

                        const auto index_data = rm.get_index_data();
                        if (!index_data)
                        {
                            res.add_result(recipe_guid, false, "Clear cooked terrain failed: asset index unavailable.");
                            return res;
                        }

                        const auto recipe_it = index_data->by_guid.find(recipe_guid);
                        if (recipe_it == index_data->by_guid.end() || !recipe_it->second)
                        {
                            res.add_result(recipe_guid, false, "Clear cooked terrain failed: recipe entry missing from asset index.");
                            return res;
                        }

                        const std::string recipe_name = sanitize_asset_name(recipe_it->second->meta.name);
                        const std::filesystem::path cook_root = rm.assets_root() / "terrain" / recipe_name;

                        const std::vector<BatchId> owned_batches =
                            collect_generated_terrain_batch_ids_for_recipe(ctx, recipe_guid);

                        // Unload any generated terrain assets still open in the editor
                        // before removing their cook folder from disk.
                        for (const auto& entry : index_data->entries)
                        {
                            if (!path_is_within(entry.absolute_path, cook_root))
                                continue;

                            try
                            {
                                if (entry.meta.type_id == meta::get_meta_type_id_string<assets::TerrainAsset>())
                                    rm.unload_asset<assets::TerrainAsset>(entry.meta.guid, ctx);
                                else if (entry.meta.type_id == meta::get_meta_type_id_string<assets::TerrainChunkAsset>())
                                    rm.unload_asset<assets::TerrainChunkAsset>(entry.meta.guid, ctx);
                                else if (entry.meta.type_id == meta::get_meta_type_id_string<assets::ModelDataAsset>())
                                    rm.unload_asset<assets::ModelDataAsset>(entry.meta.guid, ctx);
                                else if (entry.meta.type_id == meta::get_meta_type_id_string<assets::GpuModelAsset>())
                                    rm.unload_asset<assets::GpuModelAsset>(entry.meta.guid, ctx);
                            }
                            catch (...)
                            {
                                // Keep cleanup best-effort here; scan below rebuilds the index.
                            }
                        }

                        for (const auto& batch_id : owned_batches)
                        {
                            if (!batch_registry->delete_batch(batch_id))
                            {
                                res.add_result(batch_id, false, "Clear cooked terrain failed: could not delete generated terrain batch.");
                                return res;
                            }
                        }
                        batch_registry->save_index();

                        std::error_code ec{};
                        std::filesystem::remove_all(cook_root, ec);
                        if (ec)
                        {
                            res.add_result(recipe_guid, false, "Clear cooked terrain failed: could not remove cook folder.");
                            return res;
                        }

                        rm.scan_assets_async(assets_root, ctx);
                        res.add_result(recipe_guid, true, "Cleared cooked terrain outputs.");
                        return res;
                    },
                    *ctx_sp).get();
            });
    }

    void AssetActions::unimport_assets(EngineContext& ctx, std::vector<Guid> roots)
    {
        if (roots.empty())
        {
            EENG_LOG_WARN(&ctx, "Unimport skipped: no assets selected.");
            return;
        }
        if (!can_queue_action(ctx, "UnimportAssets"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<UnimportAssetsCommand>(
                std::move(roots),
                ctx_wptr),
            "UnimportAssets");
    }

    void AssetActions::restore_assets(EngineContext& ctx, std::vector<Guid> roots)
    {
        if (roots.empty())
        {
            EENG_LOG_WARN(&ctx, "Restore skipped: no assets selected.");
            return;
        }

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        rm.queue_import_job(
            [roots = std::move(roots)](ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Restore;

                bool restored_any = false;
                for (const Guid& root : roots)
                {
                    std::string error;
                    if (!rm.restore_from_trash(root, ctx, &error))
                    {
                        if (error.empty())
                            error = "Restore failed.";
                        res.add_result(root, false, error);
                        continue;
                    }

                    restored_any = true;
                    res.add_result(root, true, "Restore ok");
                }

                if (restored_any)
                {
                    const auto& assets_root = rm.assets_root();
                    if (!assets_root.empty())
                        rm.scan_assets_async(assets_root, ctx);
                }

                return res;
            },
            ctx);
    }

    void BatchActions::load_batch(EngineContext& ctx, const BatchId& id)
    {
        if (!id.valid())
            return;
        if (!can_queue_action(ctx, "LoadBatch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<BatchLoadCommand>(
                id,
                ctx_wptr),
            "LoadBatch");
    }

    void BatchActions::unload_batch(EngineContext& ctx, const BatchId& id)
    {
        if (!id.valid())
            return;
        if (!can_queue_action(ctx, "UnloadBatch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<BatchUnloadCommand>(
                id,
                ctx_wptr),
            "UnloadBatch");
    }

    void BatchActions::load_all(EngineContext& ctx)
    {
        if (!can_queue_action(ctx, "LoadAllBatches"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<BatchLoadAllCommand>(
                ctx_wptr),
            "LoadAllBatches");
    }

    void BatchActions::unload_all(EngineContext& ctx)
    {
        if (!can_queue_action(ctx, "UnloadAllBatches"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<BatchUnloadAllCommand>(
                ctx_wptr),
            "UnloadAllBatches");
    }

    void BatchActions::create_batch(EngineContext& ctx, std::string name)
    {
        if (!can_queue_action(ctx, "CreateBatch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<CreateBatchCommand>(
                std::move(name),
                ctx_wptr),
            "CreateBatch");
    }

    void BatchActions::delete_batch(EngineContext& ctx, const BatchId& id)
    {
        if (!id.valid())
            return;
        if (reject_read_only_batch_target(ctx, id, "DeleteBatch"))
            return;
        if (!can_queue_action(ctx, "DeleteBatch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<DeleteBatchCommand>(
                id,
                ctx_wptr),
            "DeleteBatch");
    }

    void BatchActions::assign_entities_to_batch(
        EngineContext& ctx,
        const BatchId& id,
        const std::deque<ecs::Entity>& selection)
    {
        if (!id.valid() || selection.empty())
            return;
        if (reject_read_only_batch_target(ctx, id, "AssignEntitiesToBatch"))
            return;
        if (!can_queue_action(ctx, "AssignEntitiesToBatch"))
            return;

        const auto mutable_selection = filter_out_read_only_entities(ctx, selection, "AssignEntitiesToBatch");
        if (mutable_selection.empty())
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        // Policy: keep batch membership consistent across an entity branch.
        // If a parent moves, all descendants should follow.
        if (!ctx.entity_manager)
        {
            EENG_LOG(&ctx, "AssignEntitiesToBatch aborted: missing entity manager.");
            return;
        }

        std::vector<ecs::Entity> selection_snapshot;
        selection_snapshot.reserve(mutable_selection.size());

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();
        std::unordered_set<ecs::Entity> seen;

        for (const auto& entity : mutable_selection)
        {
            if (!entity.has_id() || !em.entity_valid(entity)) continue;
            if (!scenegraph.contains(entity)) continue;

            const auto branch = scenegraph.get_branch_topdown(entity);
            for (const auto& branch_entity : branch)
            {
                if (!branch_entity.has_id() || !em.entity_valid(branch_entity)) continue;
                
                if (seen.insert(branch_entity).second)
                    selection_snapshot.push_back(branch_entity);
            }
        }

        try_add_command(
            ctx,
            CommandFactory::Create<AssignEntitiesToBatchCommand>(
                id,
                std::move(selection_snapshot),
                ctx_wptr),
            "AssignEntitiesToBatch");
    }
}
