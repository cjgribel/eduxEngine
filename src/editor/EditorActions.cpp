// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/EditorActions.hpp"
#include "editor/CommandQueue.hpp"
#include "editor/GuiCommands.hpp"
#include "editor/BatchCommands.hpp"
#include "ResourceManager.hpp"
#include "AssetMetaData.hpp"
#include "assets/types/AnimationGraphAsset.hpp"
#include "ecs/EntityManager.hpp"
#include "LogMacros.h"
#include <memory>
#include <unordered_set>

namespace eeng::editor
{
    namespace
    {
        bool can_queue(EngineContext& ctx)
        {
            return ctx.command_queue != nullptr;
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
    }

    void SceneActions::create_entity(EngineContext& ctx, const ecs::Entity& parent_entity)
    {
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
        if (selection.empty())
            return;
        if (!can_queue_action(ctx, "DeleteEntities"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto roots = filter_out_descendants(
            em.scene_graph(),
            selection);

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
        if (selection.empty())
            return;
        if (!can_queue_action(ctx, "CopyEntities"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto roots = filter_out_descendants(
            em.scene_graph(),
            selection);

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
        if (selection.size() < 2)
            return;
        if (!can_queue_action(ctx, "ParentEntities"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();
        const auto new_parent = selection.back();

        for (const auto& entity : selection)
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
        if (selection.empty())
            return;
        if (!can_queue_action(ctx, "UnparentEntities"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();

        for (const auto& entity : selection)
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
        if (selection.empty() || comp_id == entt::id_type{})
            return;
        if (!can_queue_action(ctx, "AddComponents"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();

        for (const auto& entity : selection)
        {
            if (!entity.has_id())
                continue;
            if (!em.entity_valid(entity))
                continue;
            if (!scenegraph.contains(entity))
                continue;

            try_add_command(
                ctx,
                CommandFactory::Create<AddComponentToEntityCommand>(
                    entity,
                    comp_id,
                    ctx_wptr),
                "AddComponents");
        }
    }

    void SceneActions::remove_components(EngineContext& ctx, const std::deque<ecs::Entity>& selection, entt::id_type comp_id)
    {
        if (selection.empty() || comp_id == entt::id_type{})
            return;
        if (!can_queue_action(ctx, "RemoveComponents"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();

        for (const auto& entity : selection)
        {
            if (!entity.has_id())
                continue;
            if (!em.entity_valid(entity))
                continue;
            if (!scenegraph.contains(entity))
                continue;

            try_add_command(
                ctx,
                CommandFactory::Create<RemoveComponentFromEntityCommand>(
                    entity,
                    comp_id,
                    ctx_wptr),
                "RemoveComponents");
        }
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

        assets::AnimationGraphAsset graph{};
        graph.version = 1;
        graph.name = graph_name;

#if 0
        // Minimal single-clip graph.
        assets::AnimGraphLayer layer{};
        layer.name = "Base";
        layer.weight = 1.0f;
        layer.blend_mode = assets::AnimGraphBlendMode::Override;
        layer.entry_state = "Idle";

        assets::AnimGraphState state{};
        state.id = "Idle";
        state.type = assets::AnimGraphStateType::Clip;
        state.clip = clip_name;
        state.playback = assets::AnimGraphPlaybackMode::Loop;
        state.speed = 1.0f;
        layer.states.push_back(std::move(state));

        graph.layers.push_back(std::move(layer));
#elif 0
        // Blend1D locomotion test graph (idle -> walk -> run).
        assets::AnimGraphParamDef speed{};
        speed.name = "speed";
        speed.type = assets::AnimGraphParamType::Float;
        speed.default_float = 0.0f;
        speed.has_min = true;
        speed.has_max = true;
        speed.min_value = 0.0f;
        speed.max_value = 1.0f;
        graph.params.push_back(std::move(speed));

        assets::AnimGraphLayer layer{};
        layer.name = "Base";
        layer.weight = 1.0f;
        layer.blend_mode = assets::AnimGraphBlendMode::Override;
        layer.entry_state = "Locomotion";

        assets::AnimGraphState state{};
        state.id = "Locomotion";
        state.type = assets::AnimGraphStateType::BlendSpace1D;
        state.playback = assets::AnimGraphPlaybackMode::Loop;
        state.speed = 1.0f;
        state.param_x = "speed";
        state.param_min_x = 0.0f;
        state.param_max_x = 1.0f;
        state.samples.push_back(assets::AnimGraphBlendSample{ clip_name, 0.0f, 0.0f });
        state.samples.push_back(assets::AnimGraphBlendSample{ "walking", 0.5f, 0.0f });
        state.samples.push_back(assets::AnimGraphBlendSample{ "running", 1.0f, 0.0f });
        state.indices = { 0, 1, 1, 2 };
        layer.states.push_back(std::move(state));

        graph.layers.push_back(std::move(layer));
#else
        // Full UE mannequin graph (locomotion, actions, aim, reactions).
        (void)clip_name;

        auto add_float_param = [&](const std::string& name, float min_value, float max_value, float default_value)
        {
            assets::AnimGraphParamDef param{};
            param.name = name;
            param.type = assets::AnimGraphParamType::Float;
            param.default_float = default_value;
            param.has_min = true;
            param.has_max = true;
            param.min_value = min_value;
            param.max_value = max_value;
            graph.params.push_back(std::move(param));
        };

        auto add_bool_param = [&](const std::string& name)
        {
            assets::AnimGraphParamDef param{};
            param.name = name;
            param.type = assets::AnimGraphParamType::Bool;
            param.default_bool = false;
            graph.params.push_back(std::move(param));
        };

        add_float_param("U_LT", -1.0f, 1.0f, 0.0f);
        add_float_param("V_LT", -1.0f, 1.0f, 0.0f);
        add_float_param("U_RT", -1.0f, 1.0f, 0.0f);
        add_float_param("V_RT", -1.0f, 1.0f, 0.0f);

        add_bool_param("DO_JUMP");
        add_bool_param("DO_FIRE_RIFLE");
        add_bool_param("DO_RELOAD_RIFLE");
        add_bool_param("DO_HITREACTION0");
        add_bool_param("DO_HITREACTION1");
        add_bool_param("DO_HITREACTION2");
        add_bool_param("DO_HITREACTION3");

        constexpr const char* kClipAim = "Aim_Space_Ironsights_PreviewMesh";
        constexpr const char* kClipIdle = "Idle_Rifle_Ironsights_PreviewMesh";
        constexpr const char* kClipWalkFwd = "Walk_Fwd_Rifle_Ironsights_PreviewMesh";
        constexpr const char* kClipWalkBwd = "Walk_Bwd_Rifle_Ironsights_PreviewMesh";
        constexpr const char* kClipWalkRt = "Walk_Rt_Rifle_Ironsights_PreviewMesh";
        constexpr const char* kClipWalkLt = "Walk_Lt_Rifle_Ironsights_PreviewMesh";
        constexpr const char* kClipWalkFwdRt = "WalkFwdRtIronsight";
        constexpr const char* kClipWalkFwdLt = "WalkFwdLtIronsight";
        constexpr const char* kClipWalkBwdRt = "WalkBwdRtIronsight";
        constexpr const char* kClipWalkBwdLt = "WalkBwdLtIronsight";
        constexpr const char* kClipJump = "Jump_From_Stand_Ironsights_PreviewMesh";
        constexpr const char* kClipFire = "Fire_Rifle_Ironsights_PreviewMesh";
        constexpr const char* kClipReload = "Reload_Rifle_Ironsights_PreviewMesh";
        constexpr const char* kClipHit0 = "Hit_React_1_PreviewMesh";
        constexpr const char* kClipHit1 = "Hit_React_2_PreviewMesh";
        constexpr const char* kClipHit2 = "Hit_React_3_PreviewMesh";
        constexpr const char* kClipHit3 = "Hit_React_4_PreviewMesh";
        constexpr const char* kClipBindPose = "__bindpose__"; // Policy: Unresolved clip names sample bind pose.

        assets::AnimGraphLayer base{};
        base.name = "Base";
        base.weight = 1.0f;
        base.blend_mode = assets::AnimGraphBlendMode::Override;
        base.entry_state = "Locomotion";

        assets::AnimGraphState locomotion{};
        locomotion.id = "Locomotion";
        locomotion.type = assets::AnimGraphStateType::BlendSpace2D;
        locomotion.playback = assets::AnimGraphPlaybackMode::Loop;
        locomotion.speed = 1.0f;
        locomotion.param_x = "U_LT";
        locomotion.param_y = "V_LT";
        locomotion.param_min_x = -1.0f;
        locomotion.param_max_x = 1.0f;
        locomotion.param_min_y = -1.0f;
        locomotion.param_max_y = 1.0f;
        locomotion.samples = {
            assets::AnimGraphBlendSample{ kClipWalkBwdLt, -1.0f, -1.0f },
            assets::AnimGraphBlendSample{ kClipWalkBwd, 0.0f, -1.0f },
            assets::AnimGraphBlendSample{ kClipWalkBwdRt, 1.0f, -1.0f },
            assets::AnimGraphBlendSample{ kClipWalkLt, -1.0f, 0.0f },
            assets::AnimGraphBlendSample{ kClipIdle, 0.0f, 0.0f },
            assets::AnimGraphBlendSample{ kClipWalkRt, 1.0f, 0.0f },
            assets::AnimGraphBlendSample{ kClipWalkFwdLt, -1.0f, 1.0f },
            assets::AnimGraphBlendSample{ kClipWalkFwd, 0.0f, 1.0f },
            assets::AnimGraphBlendSample{ kClipWalkFwdRt, 1.0f, 1.0f }
        };
        locomotion.indices = { 0, 1, 4, 3, 1, 2, 5, 4, 3, 4, 7, 6, 4, 5, 8, 7 };
        base.states.push_back(std::move(locomotion));

        assets::AnimGraphState jump{};
        jump.id = "Jump";
        jump.type = assets::AnimGraphStateType::Clip;
        jump.clip = kClipJump;
        jump.playback = assets::AnimGraphPlaybackMode::Clamp;
        jump.rewind_on_enter = true;
        jump.speed = 1.0f;
        base.states.push_back(std::move(jump));

        assets::AnimGraphCondition jump_cond{};
        jump_cond.param = "DO_JUMP";
        jump_cond.op = assets::AnimGraphConditionOp::IsTrue;

        assets::AnimGraphTransition to_jump{};
        to_jump.from = "Locomotion";
        to_jump.to = "Jump";
        to_jump.duration = 0.5f;
        to_jump.conditions.mode = assets::AnimGraphConditionMode::All;
        to_jump.conditions.items.push_back(jump_cond);

        assets::AnimGraphTransition from_jump{};
        from_jump.from = "Jump";
        from_jump.to = "Locomotion";
        from_jump.duration = 0.5f;
        from_jump.has_exit_time = true;
        from_jump.exit_time = 0.5f;
        from_jump.conditions.mode = assets::AnimGraphConditionMode::All;

        base.transitions.push_back(std::move(to_jump));
        base.transitions.push_back(std::move(from_jump));
        graph.layers.push_back(std::move(base));

        assets::AnimGraphLayer actions{};
        actions.name = "Actions";
        actions.weight = 1.0f;
        actions.blend_mode = assets::AnimGraphBlendMode::Additive;
        actions.entry_state = "ActionIdle";

        assets::AnimGraphState action_idle{};
        action_idle.id = "ActionIdle";
        action_idle.type = assets::AnimGraphStateType::Clip;
        action_idle.clip = kClipBindPose;
        action_idle.playback = assets::AnimGraphPlaybackMode::Pose;
        actions.states.push_back(std::move(action_idle));

        assets::AnimGraphState fire{};
        fire.id = "Fire";
        fire.type = assets::AnimGraphStateType::Clip;
        fire.clip = kClipFire;
        fire.playback = assets::AnimGraphPlaybackMode::Clamp;
        fire.rewind_on_enter = true;
        actions.states.push_back(std::move(fire));

        assets::AnimGraphState reload{};
        reload.id = "Reload";
        reload.type = assets::AnimGraphStateType::Clip;
        reload.clip = kClipReload;
        reload.playback = assets::AnimGraphPlaybackMode::Clamp;
        reload.rewind_on_enter = true;
        actions.states.push_back(std::move(reload));

        assets::AnimGraphTransition to_fire{};
        to_fire.from = "ActionIdle";
        to_fire.to = "Fire";
        to_fire.duration = 0.0f;
        to_fire.conditions.mode = assets::AnimGraphConditionMode::All;
        to_fire.conditions.items.push_back({ "DO_FIRE_RIFLE", assets::AnimGraphConditionOp::IsTrue });
        actions.transitions.push_back(std::move(to_fire));

        assets::AnimGraphTransition fire_done{};
        fire_done.from = "Fire";
        fire_done.to = "ActionIdle";
        fire_done.duration = 0.0f;
        fire_done.has_exit_time = true;
        fire_done.exit_time = 1.0f;
        fire_done.conditions.mode = assets::AnimGraphConditionMode::All;
        actions.transitions.push_back(std::move(fire_done));

        assets::AnimGraphTransition to_reload{};
        to_reload.from = "ActionIdle";
        to_reload.to = "Reload";
        to_reload.duration = 0.0f;
        to_reload.conditions.mode = assets::AnimGraphConditionMode::All;
        to_reload.conditions.items.push_back({ "DO_RELOAD_RIFLE", assets::AnimGraphConditionOp::IsTrue });
        actions.transitions.push_back(std::move(to_reload));

        assets::AnimGraphTransition reload_done{};
        reload_done.from = "Reload";
        reload_done.to = "ActionIdle";
        reload_done.duration = 0.0f;
        reload_done.has_exit_time = true;
        reload_done.exit_time = 1.0f;
        reload_done.conditions.mode = assets::AnimGraphConditionMode::All;
        actions.transitions.push_back(std::move(reload_done));

        graph.layers.push_back(std::move(actions));

        assets::AnimGraphLayer aim{};
        aim.name = "Aim";
        aim.weight = 1.0f;
        aim.blend_mode = assets::AnimGraphBlendMode::Additive;
        aim.entry_state = "Aim";

        assets::AnimGraphState aim_state{};
        aim_state.id = "Aim";
        aim_state.type = assets::AnimGraphStateType::BlendSpace2D;
        aim_state.playback = assets::AnimGraphPlaybackMode::Pose;
        aim_state.speed = 1.0f;
        aim_state.param_x = "U_RT";
        aim_state.param_y = "V_RT";
        aim_state.param_min_x = -1.0f;
        aim_state.param_max_x = 1.0f;
        aim_state.param_min_y = -1.0f;
        aim_state.param_max_y = 1.0f;

        const float aim_inv = 1.0f / 85.0f;
        auto make_aim_sample = [&](float x, float y, float frame)
        {
            assets::AnimGraphBlendSample sample{};
            sample.clip = kClipAim;
            sample.x = x;
            sample.y = y;
            sample.pose_time = frame * aim_inv;
            return sample;
        };

        aim_state.samples = {
            make_aim_sample(-1.0f, -1.0f, 50.0f),
            make_aim_sample(0.0f, -1.0f, 20.0f),
            make_aim_sample(1.0f, -1.0f, 80.0f),
            make_aim_sample(-1.0f, 0.0f, 30.0f),
            make_aim_sample(0.0f, 0.0f, 0.0f),
            make_aim_sample(1.0f, 0.0f, 60.0f),
            make_aim_sample(-1.0f, 1.0f, 40.0f),
            make_aim_sample(0.0f, 1.0f, 10.0f),
            make_aim_sample(1.0f, 1.0f, 70.0f)
        };
        aim_state.indices = { 0, 1, 4, 3, 1, 2, 5, 4, 3, 4, 7, 6, 4, 5, 8, 7 };
        aim.states.push_back(std::move(aim_state));
        graph.layers.push_back(std::move(aim));

        assets::AnimGraphLayer reactions{};
        reactions.name = "Reactions";
        reactions.weight = 1.0f;
        reactions.blend_mode = assets::AnimGraphBlendMode::Additive;
        reactions.entry_state = "ReactionIdle";

        assets::AnimGraphState reaction_idle{};
        reaction_idle.id = "ReactionIdle";
        reaction_idle.type = assets::AnimGraphStateType::Clip;
        reaction_idle.clip = kClipBindPose;
        reaction_idle.playback = assets::AnimGraphPlaybackMode::Pose;
        reactions.states.push_back(std::move(reaction_idle));

        const char* hit_params[] = {
            "DO_HITREACTION0",
            "DO_HITREACTION1",
            "DO_HITREACTION2",
            "DO_HITREACTION3"
        };
        const char* hit_clips[] = {
            kClipHit0,
            kClipHit1,
            kClipHit2,
            kClipHit3
        };
        for (size_t i = 0; i < 4; i++)
        {
            const std::string hit_id = "HitReaction" + std::to_string(i);

            assets::AnimGraphState hit{};
            hit.id = hit_id;
            hit.type = assets::AnimGraphStateType::Clip;
            hit.clip = hit_clips[i];
            hit.playback = assets::AnimGraphPlaybackMode::Clamp;
            hit.rewind_on_enter = true;
            reactions.states.push_back(std::move(hit));

            assets::AnimGraphTransition to_hit{};
            to_hit.from = "ReactionIdle";
            to_hit.to = hit_id;
            to_hit.duration = 0.0f;
            to_hit.conditions.mode = assets::AnimGraphConditionMode::All;
            to_hit.conditions.items.push_back({ hit_params[i], assets::AnimGraphConditionOp::IsTrue });
            reactions.transitions.push_back(std::move(to_hit));

            assets::AnimGraphTransition from_hit{};
            from_hit.from = hit_id;
            from_hit.to = "ReactionIdle";
            from_hit.duration = 0.0f;
            from_hit.has_exit_time = true;
            from_hit.exit_time = 1.0f;
            from_hit.conditions.mode = assets::AnimGraphConditionMode::All;
            reactions.transitions.push_back(std::move(from_hit));
        }

        graph.layers.push_back(std::move(reactions));
#endif

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
        if (!can_queue_action(ctx, "AssignEntitiesToBatch"))
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
        selection_snapshot.reserve(selection.size());

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();
        std::unordered_set<ecs::Entity> seen;

        for (const auto& entity : selection)
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
