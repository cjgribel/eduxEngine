// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/content_generators/MannequinGraph.hpp"

namespace eeng::editor::content_generators
{
    assets::AnimationGraphAsset build_mannequin_graph(
        const std::string& graph_name,
        const std::string& clip_name)
    {
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

        return graph;
    }
}
