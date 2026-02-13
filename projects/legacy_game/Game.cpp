// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Game.hpp"
#include "LegacyGameMetaReg.hpp"
#include "editor/ecs/FirstPersonCameraComponent.hpp"
#include "editor/ecs/ThirdPersonCameraComponent.hpp"
#include "editor/ProjectConfig.hpp"
#include "glmcommon.hpp"
#include "ImGuiHelpers.hpp"
#include "imgui.h"
#include "LogMacros.h"
#include "BatchRegistry.hpp"
#include "ecs/TransformComponent.hpp"
#include "ecs/VehicleRig1Builder.hpp"
#include "ecs/VehicleRig1ControlComponent.hpp"
#include "ecs/VehicleRig1Component.hpp"
#include "ecs/PistonConstraintDriveComponent.hpp"
#include "ecs/PistonInputComponent.hpp"
#include "ecs/PistonAnimSyncComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/ModelComponent.hpp"
#include "ecs/EntityManager.hpp"
#include "editor/EditorActions.hpp"
#include "editor/EntityPickerPopup.hpp"
#include "engineapi/IInputManager.hpp"
#include "meta/MetaAux.h"
#include "meta/MetaSerialize.hpp"
#include <entt/entt.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>




bool Game::init()
{
    if (ctx)
        eeng::legacy_game::register_legacy_game_meta_types(*ctx);

    forwardRenderer = std::make_shared<eeng::ForwardRenderer>();
    forwardRenderer->init("shaders/phong_vert.glsl", "shaders/phong_frag.glsl");

    // Use the shared debug renderer owned by the engine services.
    shapeRenderer = ctx ? ctx->shape_renderer : nullptr;
    if (!shapeRenderer)
    {
        // Fallback for non-engine contexts or tests.
        shapeRenderer = std::make_shared<ShapeRendering::ShapeRenderer>();
        shapeRenderer->init();
    }

    runtime_pipeline_.init(*ctx);
    if (ctx && ctx->services)
        ctx->services->debug_render_settings = runtime_pipeline_.debug_render_settings();
    playerControllerSystem = std::make_unique<eeng::ecs::systems::MannequinPlayerControllerSystem>();
    if (playerControllerSystem)
        playerControllerSystem->set_physics_system(runtime_pipeline_.physics_system());
    vehicleRig1ControlSystem = std::make_unique<eeng::ecs::systems::VehicleRig1ControlSystem>();
    pistonInputSystem = std::make_unique<eeng::ecs::systems::PistonInputSystem>();

    // Prefer project config asset root when running inside the editor.
    std::filesystem::path assets_root = "assets";
    if (ctx && ctx->project_config)
        assets_root = ctx->project_config->assets_root;
    const auto asset_path = [&](const std::string& relative)
    {
        // Normalize asset lookups through the project root.
        return (assets_root / relative).string();
    };

    // Grass
    grassMesh = std::make_shared<eeng::RenderableMesh>();
    grassMesh->load(asset_path("grass/grass_trees_merged.fbx"), false);

    // Horse
    horseMesh = std::make_shared<eeng::RenderableMesh>();
    horseMesh->load(asset_path("Animals/Horse.fbx"), false);

    // Character
    characterMesh = std::make_shared<eeng::RenderableMesh>();
#if 0
    // Character
    characterMesh->load(asset_path("Ultimate Platformer Pack/Character/Character.fbx"), false);
#endif
#if 0
    // Enemy
    characterMesh->load(asset_path("Ultimate Platformer Pack/Enemies/Bee.fbx"), false);
#endif
#if 0
    // ExoRed 5.0.1 PACK FBX, 60fps, No keyframe reduction
    characterMesh->load(asset_path("ExoRed/exo_red.fbx"));
    characterMesh->load(asset_path("ExoRed/idle (2).fbx"), true);
    characterMesh->load(asset_path("ExoRed/walking.fbx"), true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#ifdef AMY_PATH
    // Amy 5.0.1 PACK FBX
    characterMesh->load(AMY_PATH);
    characterMesh->load(AMY_IDLE_PATH, true);
    characterMesh->load(AMY_WALK_PATH, true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#if 0
    // Eve 5.0.1 PACK FBX
    // Fix for assimp 5.0.1 (https://github.com/assimp/assimp/issues/4486)
    // FBXConverter.cpp, line 648: 
    //      const float zero_epsilon = 1e-6f; => const float zero_epsilon = Math::getEpsilon<float>();
    characterMesh->load(asset_path("Eve/Eve By J.Gonzales.fbx"));
    characterMesh->load(asset_path("Eve/idle.fbx"), true);
    characterMesh->load(asset_path("Eve/walking.fbx"), true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif

#ifdef SPONZA_PATH
    sponzaMesh = std::make_shared<eeng::RenderableMesh>();
    sponzaMesh->load(SPONZA_PATH, false);
#endif
#ifdef CHARACTER_PATH
    qcharacterMesh = std::make_shared<eeng::RenderableMesh>();
    qcharacterMesh->load(CHARACTER_PATH);
#endif
#ifdef ENEMY_PATH
    enemyMesh = std::make_shared<eeng::RenderableMesh>();
    enemyMesh->load(ENEMY_PATH);
#endif
#ifdef EXORED_PATH
    exoredMesh = std::make_shared<eeng::RenderableMesh>();
    exoredMesh->load(EXORED_PATH);
    exoredMesh->load(EXORED_ANIM0_PATH, true);
    exoredMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#ifdef EVE_PATH
    eveMesh = std::make_shared<eeng::RenderableMesh>();
    eveMesh->load(EVE_PATH);
    eveMesh->load(EVE_ANIM0_PATH, true);
    eveMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#ifdef MANNEQUIN_PATH
    mannequinMesh = std::make_shared<eeng::RenderableMesh>();
    mannequinMesh->load(MANNEQUIN_PATH);
    mannequinMesh->load(MANNEQUIN_ANIM0_PATH, true);
#endif
#ifdef UE5QUINN_PATH
    ue5quinnMesh = std::make_shared<eeng::RenderableMesh>();
    ue5quinnMesh->load(UE5QUINN_PATH);
    ue5quinnMesh->load(UE5QUINN_ANIM0_PATH, true);
#endif

    grassWorldMatrix = glm_aux::TRS(
        { 0.0f, 0.0f, 0.0f },
        0.0f, { 0, 1, 0 },
        { 100.0f, 100.0f, 100.0f });

    horseWorldMatrix = glm_aux::TRS(
        { 30.0f, 0.0f, -35.0f },
        35.0f, { 0, 1, 0 },
        { 0.01f, 0.01f, 0.01f });

    return true;
}

void Game::update_edit(
    float time,
    float deltaTime)
{
    play_mode = false;
    update(time, deltaTime);
}

void Game::update_play(
    float time,
    float deltaTime)
{
    play_mode = true;
    update(time, deltaTime);
}

void Game::update(
    float time,
    float deltaTime)
{
    auto& registry = ctx->entity_manager->registry();

    pointlight.pos = glm::vec3(
        glm_aux::R(time * 0.1f, { 0.0f, 1.0f, 0.0f }) *
        glm::vec4(100.0f, 100.0f, 100.0f, 1.0f));

#ifdef AMY_PATH
    characterWorldMatrix2 = glm_aux::TRS(
        { -3, 0, 0 },
        time * glm::radians(50.0f), { 0, 1, 0 },
        { 0.03f, 0.03f, 0.03f });

    characterWorldMatrix3 = glm_aux::TRS(
        { 3, 0, 0 },
        time * glm::radians(50.0f) * 0, { 0, 1, 0 },
        { 0.03f, 0.03f, 0.03f });
#endif

    if (play_mode && playerControllerSystem)
    {
        playerControllerSystem->update(registry, *ctx, deltaTime);
    }
    if (play_mode && vehicleRig1ControlSystem)
    {
        vehicleRig1ControlSystem->update(registry, *ctx, deltaTime);
    }
    if (play_mode && pistonInputSystem)
    {
        pistonInputSystem->update(registry, *ctx, deltaTime);
    }

    update_active_camera_state();

    // Keep camera matrices in sync for systems that run during update (e.g. gizmos).
    if (matrices.windowSize.x > 0 && matrices.windowSize.y > 0)
    {
        const float aspectRatio = float(matrices.windowSize.x) / matrices.windowSize.y;
        matrices.P = glm::perspective(
            glm::radians(60.0f),
            aspectRatio,
            active_camera.near_plane,
            active_camera.far_plane);
        matrices.V = active_camera.model_to_view;
        matrices.VP = glm_aux::create_viewport_matrix(0.0f, 0.0f,
            static_cast<float>(matrices.windowSize.x),
            static_cast<float>(matrices.windowSize.y),
            0.0f, 1.0f);
    }

    if (play_mode)
        runtime_pipeline_.update_play(*ctx, deltaTime);
    else
        runtime_pipeline_.update_edit(*ctx, deltaTime);

#ifdef AMY_PATH
    // Drive the main character instance from the player entity transform.
    glm::vec3 player_pos = glm::vec3(0.0f, 0.0f, 0.0f);
    if (player_entity.has_id() && registry.valid(player_entity))
    {
        if (const auto* tfm = registry.try_get<eeng::ecs::TransformComponent>(player_entity))
            player_pos = glm::vec3(tfm->world_matrix[3]);
    }

    characterWorldMatrix1 = glm_aux::TRS(
        player_pos,
        0.0f, { 0, 1, 0 },
        { 0.03f, 0.03f, 0.03f });
#endif

    // Build a view ray from the active camera (useful for debug/picking).
    view_ray = glm_aux::Ray(active_camera.position, active_camera.forward);

    // Intersect view ray with AABBs of other objects.
#ifdef AMY_PATH
    glm_aux::intersect_ray_AABB(view_ray, character_aabb2.min, character_aabb2.max);
    glm_aux::intersect_ray_AABB(view_ray, character_aabb3.min, character_aabb3.max);
#endif
    glm_aux::intersect_ray_AABB(view_ray, horse_aabb.min, horse_aabb.max);

    // We can also compute a ray from the current mouse position,
    // to use for object picking and such ...
    if (!play_mode && ctx->input_manager && ctx->input_manager->GetMouseState().rightButton)
    {
        const auto mouse = ctx->input_manager->GetMouseState();
        glm::ivec2 windowPos(mouse.x, matrices.windowSize.y - mouse.y);
        auto ray = glm_aux::world_ray_from_window_coords(windowPos, matrices.V, matrices.P, matrices.VP);
        // Intersect with e.g. AABBs ...

        EENG_LOG(ctx, "Picking ray origin = %s, dir = %s",
            glm_aux::to_string(ray.origin).c_str(),
            glm_aux::to_string(ray.dir).c_str());
    }
}

void Game::render_edit(
    float time,
    int windowWidth,
    int windowHeight)
{
    play_mode = false;
    render(time, windowWidth, windowHeight);
}

void Game::render_play(
    float time,
    int windowWidth,
    int windowHeight)
{
    play_mode = true;
    render(time, windowWidth, windowHeight);
}

void Game::render(
    float time,
    int windowWidth,
    int windowHeight)
{
    renderUI();

    update_active_camera_state();

    matrices.windowSize = glm::ivec2(windowWidth, windowHeight);

    // Projection matrix
    const float aspectRatio = float(windowWidth) / windowHeight;
    matrices.P = glm::perspective(
        glm::radians(60.0f),
        aspectRatio,
        active_camera.near_plane,
        active_camera.far_plane);

    // View matrix
    matrices.V = active_camera.model_to_view;

    matrices.VP = glm_aux::create_viewport_matrix(0.0f, 0.0f, windowWidth, windowHeight, 0.0f, 1.0f);

    // Debug gizmos (ImGui overlay)
    if (shapeRenderer && ctx->entity_manager)
    {
        const auto VP_P_V = matrices.VP * matrices.P * matrices.V;
        auto& registry = ctx->entity_manager->registry();
        runtime_pipeline_.render_debug(
            registry,
            *ctx,
            *shapeRenderer,
            VP_P_V,
            matrices.windowSize.y);
    }

    // Begin rendering pass
    forwardRenderer->beginPass(matrices.P, matrices.V, pointlight.pos, pointlight.color, active_camera.position);

    // Grass
    forwardRenderer->renderMesh(grassMesh, grassWorldMatrix);
    grass_aabb = grassMesh->m_model_aabb.post_transform(grassWorldMatrix);

    // Horse
    horseMesh->animate(3, time);
    forwardRenderer->renderMesh(horseMesh, horseWorldMatrix);
    horse_aabb = horseMesh->m_model_aabb.post_transform(horseWorldMatrix);

#ifdef AMY_PATH
    // Character, instance 1
    characterMesh->animate(characterAnimIndex, time * characterAnimSpeed);
    forwardRenderer->renderMesh(characterMesh, characterWorldMatrix1);
    character_aabb1 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix1);

    // Character, instance 2
    characterMesh->animate(1, time * characterAnimSpeed);
    forwardRenderer->renderMesh(characterMesh, characterWorldMatrix2);
    character_aabb2 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix2);

    // Character, instance 3
    // characterMesh->animate(2, time * characterAnimSpeed);
    characterMesh->animateBlend(1, 2, time, time, characterAnimBlend);
    forwardRenderer->renderMesh(characterMesh, characterWorldMatrix3);
    character_aabb3 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix3);
#endif

#ifdef SPONZA_PATH
    forwardRenderer->renderMesh(sponzaMesh, glm::mat4{ 1.0f });
#endif
#ifdef CHARACTER_PATH
    qcharacterMesh->animate(CHARACTER_ANIM, time);
    forwardRenderer->renderMesh(qcharacterMesh, glm_aux::TS({ 0.0f, 0.0f, -5.0f }, { 0.02f, 0.02f, 0.02f }));
#endif
#ifdef ENEMY_PATH
    enemyMesh->animate(ENEMY_ANIM, time);
    forwardRenderer->renderMesh(enemyMesh, glm_aux::TS({ 0.0f, 0.0f, -10.0f }, { 0.02f, 0.02f, 0.02f }));
#endif
#ifdef EXORED_PATH
    exoredMesh->animate(EXORED_ANIM, time);
    forwardRenderer->renderMesh(exoredMesh, glm_aux::TS({ 0.0f, 0.0f, -15.0f }, { 0.05f, 0.05f, 0.05f }));
#endif
#ifdef EVE_PATH
    eveMesh->animate(EVE_ANIM, time);
    forwardRenderer->renderMesh(eveMesh, glm_aux::TS({ 0.0f, 0.0f, -20.0f }, { 0.05f, 0.05f, 0.05f }));
#endif
#ifdef MANNEQUIN_PATH
    mannequinMesh->animate(MANNEQUIN_ANIM, time);
    forwardRenderer->renderMesh(mannequinMesh, glm_aux::TS({ 0.0f, 0.0f, -25.0f }, { 0.05f, 0.05f, 0.05f }));
#endif
#ifdef UE5QUINN_PATH
    ue5quinnMesh->animate(UE5QUINN_ANIM, time);
    forwardRenderer->renderMesh(ue5quinnMesh, glm_aux::TS({ -5.0f, 0.0f, -25.0f }, { 0.05f, 0.05f, 0.05f }));
#endif

    // End rendering pass
    drawcallCount = forwardRenderer->endPass();

    // "New" render system
    if (ctx->entity_manager)
    {
        auto& registry = ctx->entity_manager->registry();
        const auto proj_view = matrices.P * matrices.V;

        runtime_pipeline_.render_entities(
            registry,
            *ctx,
            proj_view,
            pointlight.pos,
            pointlight.color,
            active_camera.position);
    }

    // Draw player view ray
    if (view_ray)
    {
        shapeRenderer->push_states(ShapeRendering::Color4u{ 0xff00ff00 });
        shapeRenderer->push_line(view_ray.origin, view_ray.point_of_contact());
    }
    else
    {
        shapeRenderer->push_states(ShapeRendering::Color4u{ 0xffffffff });
        shapeRenderer->push_line(view_ray.origin, view_ray.origin + view_ray.dir * 100.0f);
    }
    shapeRenderer->pop_states<ShapeRendering::Color4u>();

    // Draw object bases
    {
#ifdef AMY_PATH
        shapeRenderer->push_basis_basic(characterWorldMatrix1, 1.0f);
        shapeRenderer->push_basis_basic(characterWorldMatrix2, 1.0f);
        shapeRenderer->push_basis_basic(characterWorldMatrix3, 1.0f);
#endif
        shapeRenderer->push_basis_basic(grassWorldMatrix, 1.0f);
        shapeRenderer->push_basis_basic(horseWorldMatrix, 1.0f);
    }

    // Draw AABBs
    {
        shapeRenderer->push_states(ShapeRendering::Color4u{ 0xFFE61A80 });
#ifdef AMY_PATH
        shapeRenderer->push_AABB(character_aabb1.min, character_aabb1.max);
        shapeRenderer->push_AABB(character_aabb2.min, character_aabb2.max);
        shapeRenderer->push_AABB(character_aabb3.min, character_aabb3.max);
#endif
        shapeRenderer->push_AABB(horse_aabb.min, horse_aabb.max);
        shapeRenderer->push_AABB(grass_aabb.min, grass_aabb.max);
        shapeRenderer->pop_states<ShapeRendering::Color4u>();
    }

    if (ctx && ctx->overlay_view_state)
    {
        // Publish the overlay view so the engine can flush shared debug rendering.
        auto& overlay = *ctx->overlay_view_state;
        overlay.view = matrices.V;
        overlay.proj = matrices.P;
        overlay.viewport = matrices.VP;
        overlay.window_size = matrices.windowSize;
        overlay.valid = true;
    }
    else if (shapeRenderer)
    {
        // Standalone fallback when no engine-owned overlay view exists.
        shapeRenderer->render(matrices.P * matrices.V);
        shapeRenderer->post_render();
    }
}

bool Game::get_editor_view(eeng::OverlayViewState& out) const
{
    if (matrices.windowSize.x <= 0 || matrices.windowSize.y <= 0)
        return false;

    out.view = matrices.V;
    out.proj = matrices.P;
    out.viewport = matrices.VP;
    out.window_size = matrices.windowSize;
    out.valid = true;
    return true;
}

void Game::ensure_vehicle_rig1_config()
{
    if (vehicle_rig1_spec_initialized_)
        return;

    reset_vehicle_rig1_config();
}

void Game::reset_vehicle_rig1_config()
{
    vehicle_rig1_spec_initialized_ = true;
    vehicle_rig1_spec_ = {};
    vehicle_rig1_spawn_pos_ = { 0.0f, 2.0f, 0.0f };
    vehicle_rig1_control_steer_speed_ = 6.0f;
    vehicle_rig1_control_steer_max_impulse_ = 2000.0f;
    vehicle_rig1_control_drive_velocity_ = 10.0f;
    vehicle_rig1_control_drive_max_impulse_ = 150.0f;
    vehicle_rig1_control_brake_max_impulse_ = 200.0f;

    vehicle_rig1_spec_.name_prefix = "VehicleRig1";
    vehicle_rig1_spec_.chunk_tag = "vehicle_rig1";
    vehicle_rig1_spec_.steer_axis = { 0.0f, 1.0f, 0.0f };
    vehicle_rig1_spec_.steer_limit = 0.8f;
    vehicle_rig1_spec_.disable_collisions = true;
    vehicle_rig1_spec_.chassis_model_name = "carbody";
    vehicle_rig1_spec_.wheel_model_name = "tyre";
    vehicle_rig1_spec_.chassis_half_extents = { 1.6f, 0.35f, 1.0f };

    auto make_wheel = [&](const glm::vec2& mount_sign, bool steerable, bool driven)
    {
        eeng::ecs::VehicleRig1WheelSpec wheel{};
        wheel.mount_override = false;
        wheel.mount_sign = mount_sign;

        // Hard-coded axes for the prototype.
        wheel.suspension_axis = { 0.0f, -1.0f, 0.0f };
        wheel.axle_axis = { 0.0f, 0.0f, 1.0f };

        // Suspension: rest/travel define the default linear limits in the 6DoF frame.
        wheel.suspension_rest_length = 0.9f;
        wheel.suspension_travel = 0.8f;
        wheel.use_linear_limits = true;
        wheel.linear_limit_min = { 1.0f, 0.0f, 0.0f };
        wheel.linear_limit_max = { 3.0f, 0.0f, 0.0f };
        wheel.linear_equilibrium_enabled = { 1.0f, 0.0f, 0.0f };
        wheel.linear_equilibrium_target = { 1.5f, 0.0f, 0.0f };

        // Spring tuning (works in the 6DoF constraint).
        wheel.spring_k = 500.0f;
        wheel.spring_d = 5.0f;

        // Collider sizes.
        wheel.wheel_collider_type = eeng::ecs::WheelColliderType::Sphere;
        wheel.wheel_radius = 0.35f;
        wheel.wheel_width = 0.25f;
        wheel.knuckle_radius = 0.15f;

        // Per-wheel capability flags.
        wheel.steerable = steerable;
        wheel.driven = driven;

        // Flip drive direction for steerable wheels if needed (front wheels were observed reversed).
        wheel.drive_direction = steerable ? -1.0f : 1.0f;
        // Keep steering direction consistent for both front wheels.
        wheel.steer_direction = 1.0f;
        return wheel;
    };

    vehicle_rig1_spec_.wheels.push_back(make_wheel({ 1.0f, 1.0f }, true, true));
    vehicle_rig1_spec_.wheels.push_back(make_wheel({ 1.0f, -1.0f }, true, true));
    vehicle_rig1_spec_.wheels.push_back(make_wheel({ -1.0f, 1.0f }, false, false));
    vehicle_rig1_spec_.wheels.push_back(make_wheel({ -1.0f, -1.0f }, false, false));
}

void Game::spawn_vehicle_rig1_from_prefab()
{
    if (!ctx)
        return;

    ensure_vehicle_rig1_config();

    eeng::ecs::VehicleRig1ChassisSpec chassis_spec{};
    chassis_spec.position = vehicle_rig1_spawn_pos_;
    chassis_spec.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    chassis_spec.half_extents = vehicle_rig1_spec_.chassis_half_extents;

    nlohmann::json prefab_json = eeng::ecs::build_vehicle_rig1_prefab_json(
        *ctx,
        vehicle_rig1_spec_,
        chassis_spec);
    if (!prefab_json.is_array() || prefab_json.empty())
    {
        EENG_LOG_WARN(ctx, "VehicleRig1 prefab build failed: empty JSON.");
        return;
    }

    // Inject control component on the rig root before spawning.
    eeng::ecs::VehicleRig1ControlComponent control{};
    control.steer_limit = vehicle_rig1_spec_.steer_limit;
    control.steer_speed = vehicle_rig1_control_steer_speed_;
    control.steer_max_impulse = vehicle_rig1_control_steer_max_impulse_;
    control.drive_velocity = vehicle_rig1_control_drive_velocity_;
    control.drive_max_impulse = vehicle_rig1_control_drive_max_impulse_;
    control.brake_max_impulse = vehicle_rig1_control_brake_max_impulse_;

    const auto control_type = eeng::meta::get_meta_type_id_string<eeng::ecs::VehicleRig1ControlComponent>();
    auto& root_json = prefab_json.front();
    if (!root_json.contains("components") || !root_json["components"].is_object())
        root_json["components"] = nlohmann::json::object();
    root_json["components"][control_type] = eeng::meta::serialize_any(
        entt::forward_as_meta(control),
        eeng::meta::SerializationPurpose::file);

    // Spawn via command so undo/redo works.
    eeng::editor::SceneActions::spawn_entity_branch_from_json(
        *ctx,
        std::move(prefab_json),
        eeng::ecs::Entity{},
        false);
}

void Game::ensure_piston_rig_config()
{
    if (piston_rig_spec_initialized_)
        return;

    reset_piston_rig_config();
}

void Game::reset_piston_rig_config()
{
    piston_rig_spec_initialized_ = true;
    piston_rig_spec_ = {};
    piston_rig_spec_.name_prefix = "Piston";
    piston_rig_spec_.chunk_tag = "piston_rig";
    piston_rig_spec_.position = { 0.0f, 2.0f, 0.0f };
    piston_rig_spec_.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    piston_rig_spec_.anchor_local_a = { 0.0f, 0.0f, 0.0f };
    piston_rig_spec_.anchor_local_b = { 1.0f, 0.0f, 0.0f };
    piston_rig_spec_.use_sockets = true;
    piston_rig_spec_.axis_local = { 1.0f, 0.0f, 0.0f };
    piston_rig_spec_.disable_collisions = true;
    piston_rig_spec_.stroke_min = 0.0f;
    piston_rig_spec_.stroke_max = 1.0f;
    piston_rig_spec_.max_force = 2000.0f;
    piston_rig_spec_.max_velocity = 1.0f;
    piston_rig_spec_.mode = 0;
    piston_rig_spec_.target_extension = 0.0f;
    piston_rig_spec_.lock_when_idle = true;
}

void Game::spawn_piston_rig_from_prefab()
{
    if (!ctx)
        return;

    ensure_piston_rig_config();

    nlohmann::json prefab_json = eeng::ecs::build_piston_rig_prefab_json(
        *ctx,
        piston_rig_spec_);
    if (!prefab_json.is_array() || prefab_json.empty())
    {
        EENG_LOG_WARN(ctx, "Piston rig prefab build failed: empty JSON.");
        return;
    }

    // Inject piston input + animation sync on the rig root before spawning.
    eeng::ecs::PistonInputComponent input{};
    eeng::ecs::PistonAnimSyncComponent anim_sync{};

    auto& root_json = prefab_json.front();
    if (!root_json.contains("components") || !root_json["components"].is_object())
        root_json["components"] = nlohmann::json::object();

    const auto input_type = eeng::meta::get_meta_type_id_string<eeng::ecs::PistonInputComponent>();
    root_json["components"][input_type] = eeng::meta::serialize_any(
        entt::forward_as_meta(input),
        eeng::meta::SerializationPurpose::file);

    const auto sync_type = eeng::meta::get_meta_type_id_string<eeng::ecs::PistonAnimSyncComponent>();
    root_json["components"][sync_type] = eeng::meta::serialize_any(
        entt::forward_as_meta(anim_sync),
        eeng::meta::SerializationPurpose::file);

    eeng::editor::SceneActions::spawn_entity_branch_from_json(
        *ctx,
        std::move(prefab_json),
        eeng::ecs::Entity{},
        false);
}

void Game::renderUI()
{
    ImGui::Begin("Game Info");

    ImGui::Text("Drawcall count %i", drawcallCount);
    const auto add_tooltip = [](const char* text)
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", text);
    };

    if (ImGui::ColorEdit3("Light color",
        glm::value_ptr(pointlight.color),
        ImGuiColorEditFlags_NoInputs))
    {
    }

#ifdef AMY_PATH
    if (characterMesh)
    {
        // Combo (drop-down) for animation clip
        int curAnimIndex = characterAnimIndex;
        std::string label = (curAnimIndex == -1 ? "Bind pose" : characterMesh->getAnimationName(curAnimIndex));
        if (ImGui::BeginCombo("Character animation##animclip", label.c_str()))
        {
            // Bind pose item
            const bool isSelected = (curAnimIndex == -1);
            if (ImGui::Selectable("Bind pose", isSelected))
                curAnimIndex = -1;
            if (isSelected)
                ImGui::SetItemDefaultFocus();

            // Clip items
            for (int i = 0; i < characterMesh->getNbrAnimations(); i++)
            {
                const bool isSelected = (curAnimIndex == i);
                const auto label = characterMesh->getAnimationName(i) + "##" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), isSelected))
                    curAnimIndex = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
            characterAnimIndex = curAnimIndex;
        }

        // In-world position label
        const auto VP_P_V = matrices.VP * matrices.P * matrices.V;
        const auto world_pos = glm::vec3(horseWorldMatrix[3]);
        glm::ivec2 window_coords;
        if (glm_aux::window_coords_from_world_pos(world_pos, VP_P_V, window_coords))
        {
            char label[256];
            std::snprintf(
                label,
                sizeof(label),
                "In-world GUI element\nWindow pos (%i, %i)\nWorld pos (%1.1f, %1.1f, %1.1f)",
                window_coords.x,
                window_coords.y,
                world_pos.x,
                world_pos.y,
                world_pos.z);

            eeng::gui::ImGuiPrintTextAt(
                window_coords,
                matrices.windowSize.y,
                label,
                "window_name",
                0x80000000,
                0xffffffff);
        }
    }

    ImGui::SliderFloat("Animation speed", &characterAnimSpeed, 0.1f, 5.0f);

    ImGui::SliderFloat("Animation mix", &characterAnimBlend, 0.0f, 1.0f);
#endif

    ImGui::Separator();
    if (ImGui::CollapsingHeader("VehicleRig1 Rig", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ensure_vehicle_rig1_config();

        bool default_batch_loaded = false;
        if (ctx && ctx->batch_registry)
        {
            auto& br = static_cast<eeng::BatchRegistry&>(*ctx->batch_registry);
            eeng::BatchId default_id{};
            if (br.try_get_batch_id_by_name(eeng::BatchRegistry::kDefaultBatchName, default_id))
                default_batch_loaded = br.is_batch_loaded(default_id);
        }

        if (ImGui::TreeNodeEx("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat3("Spawn position", &vehicle_rig1_spawn_pos_.x, 0.05f);
            add_tooltip("World-space position for the chassis when spawning the rig.");

            if (ImGui::Button("Spawn VehicleRig1"))
                spawn_vehicle_rig1_from_prefab();

            ImGui::SameLine();
            if (ImGui::Button("Reset Config"))
                reset_vehicle_rig1_config();

            ImGui::SameLine();
            ImGui::TextDisabled(default_batch_loaded ? "default batch loaded" : "default batch not loaded");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Rig Config"))
        {
            ImGui::TextDisabled("Name prefix: %s", vehicle_rig1_spec_.name_prefix.c_str());
            ImGui::TextDisabled("Chunk tag: %s", vehicle_rig1_spec_.chunk_tag.c_str());

            glm::vec3 chassis_size = vehicle_rig1_spec_.chassis_half_extents * 2.0f;
            if (ImGui::DragFloat3("Chassis size", &chassis_size.x, 0.05f, 0.05f, 20.0f))
            {
                chassis_size = glm::max(chassis_size, glm::vec3(0.05f));
                vehicle_rig1_spec_.chassis_half_extents = chassis_size * 0.5f;
            }
            add_tooltip("Chassis collider size in world units (width, height, length).");

            ImGui::DragFloat3("Steer axis", &vehicle_rig1_spec_.steer_axis.x, 0.01f);
            add_tooltip("Chassis-local steering axis for the 6DoF constraint frame.");
            ImGui::DragFloat("Steer limit", &vehicle_rig1_spec_.steer_limit, 0.01f, 0.0f, 3.14f);
            add_tooltip("Max steering angle in radians (+/-) for steerable wheels.");
            ImGui::Checkbox("Disable collisions", &vehicle_rig1_spec_.disable_collisions);
            add_tooltip("Disable collision response between constrained body pairs.");

            for (std::size_t i = 0; i < vehicle_rig1_spec_.wheels.size(); ++i)
            {
                auto& wheel = vehicle_rig1_spec_.wheels[i];
                ImGui::PushID(static_cast<int>(i));
                const std::string label = "Wheel " + std::to_string(i);
                if (ImGui::TreeNode(label.c_str()))
                {
                    ImGui::Checkbox("Override mount", &wheel.mount_override);
                    add_tooltip("Use a custom mount position instead of deriving from chassis size.");
                    if (wheel.mount_override)
                    {
                        ImGui::DragFloat3("Mount local", &wheel.mount_local.x, 0.02f);
                        add_tooltip("Chassis-local mount position for this wheel/suspension.");
                    }
                    else
                    {
                        ImGui::DragFloat2("Mount sign", &wheel.mount_sign.x, 0.1f, -1.0f, 1.0f);
                        add_tooltip("Sign used to place the wheel from chassis size (X/Z).");
                        const glm::vec3 derived_mount{
                            vehicle_rig1_spec_.chassis_half_extents.x * wheel.mount_sign.x,
                            0.0f,
                            vehicle_rig1_spec_.chassis_half_extents.z * wheel.mount_sign.y
                        };
                        ImGui::TextDisabled("Derived mount: (%.2f, %.2f, %.2f)",
                            derived_mount.x,
                            derived_mount.y,
                            derived_mount.z);
                    }
                    ImGui::DragFloat3("Suspension axis", &wheel.suspension_axis.x, 0.02f);
                    add_tooltip("Chassis-local axis for suspension travel (6DoF frame X).");
                    ImGui::DragFloat3("Axle axis", &wheel.axle_axis.x, 0.02f);
                    add_tooltip("Knuckle/wheel local axle axis for the hinge.");
                    ImGui::DragFloat("Rest length", &wheel.suspension_rest_length, 0.01f, 0.0f, 10.0f);
                    add_tooltip("Rest length along suspension axis (positive direction).");
                    ImGui::DragFloat("Travel", &wheel.suspension_travel, 0.01f, 0.0f, 10.0f);
                    add_tooltip("Total suspension travel (symmetric around rest length).");

                    ImGui::Checkbox("Use linear limits", &wheel.use_linear_limits);
                    add_tooltip("Override default linear limits for the 6DoF suspension axis.");
                    if (wheel.use_linear_limits)
                    {
                        ImGui::DragFloat("Limit min X", &wheel.linear_limit_min.x, 0.01f, -10.0f, 10.0f);
                        add_tooltip("Minimum limit along suspension axis (constraint frame X).");
                        ImGui::DragFloat("Limit max X", &wheel.linear_limit_max.x, 0.01f, -10.0f, 10.0f);
                        add_tooltip("Maximum limit along suspension axis (constraint frame X).");

                        bool equilibrium_enabled = wheel.linear_equilibrium_enabled.x > 0.5f;
                        if (ImGui::Checkbox("Equilibrium X", &equilibrium_enabled))
                            wheel.linear_equilibrium_enabled = equilibrium_enabled ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f);
                        add_tooltip("Enable spring equilibrium on the suspension axis.");
                        ImGui::DragFloat("Equilibrium target X", &wheel.linear_equilibrium_target.x, 0.01f, -10.0f, 10.0f);
                        add_tooltip("Target equilibrium position along the suspension axis.");
                    }

                    ImGui::DragFloat("Spring K", &wheel.spring_k, 1.0f, 0.0f, 50000.0f);
                    add_tooltip("Suspension spring stiffness (6DoF spring on X).");
                    ImGui::DragFloat("Spring D", &wheel.spring_d, 1.0f, 0.0f, 50000.0f);
                    add_tooltip("Suspension damping (6DoF spring on X).");

                    ImGui::DragFloat("Wheel radius", &wheel.wheel_radius, 0.01f, 0.05f, 5.0f);
                    add_tooltip("Radius for autogenerated wheel collider.");
                    int collider_choice = static_cast<int>(wheel.wheel_collider_type);
                    if (ImGui::Combo("Wheel collider", &collider_choice, "Sphere\0Capsule\0"))
                        wheel.wheel_collider_type = static_cast<eeng::ecs::WheelColliderType>(collider_choice);
                    add_tooltip("Collider type used for autogenerated wheels.");
                    if (wheel.wheel_collider_type == eeng::ecs::WheelColliderType::Capsule)
                    {
                        ImGui::DragFloat("Wheel width", &wheel.wheel_width, 0.01f, 0.0f, 5.0f);
                        add_tooltip("Capsule height (wheel width) along the axle axis.");
                    }
                    ImGui::DragFloat("Knuckle radius", &wheel.knuckle_radius, 0.01f, 0.05f, 5.0f);
                    add_tooltip("Radius for autogenerated knuckle trigger collider.");
                    ImGui::DragFloat("Wheel mass", &wheel.wheel_mass, 0.1f, 0.0f, 100.0f);
                    add_tooltip("Override wheel mass in kg (<= 0 keeps auto-mass).");
                    ImGui::DragFloat("Knuckle mass", &wheel.knuckle_mass, 0.1f, 0.0f, 100.0f);
                    add_tooltip("Override knuckle mass in kg (<= 0 keeps auto-mass).");

                    ImGui::Checkbox("Steerable", &wheel.steerable);
                    add_tooltip("Allow steering on this wheel (6DoF angular X).");
                    ImGui::Checkbox("Driven", &wheel.driven);
                    add_tooltip("Enable drive motor on this wheel hinge.");
                    ImGui::DragFloat("Drive direction", &wheel.drive_direction, 0.1f, -5.0f, 5.0f);
                    add_tooltip("Sign flip for drive direction (+/-).");
                    ImGui::DragFloat("Steer direction", &wheel.steer_direction, 0.1f, -5.0f, 5.0f);
                    add_tooltip("Sign flip for steering direction (+/-).");

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Control Config"))
        {
            ImGui::DragFloat("Steer speed", &vehicle_rig1_control_steer_speed_, 0.1f, 0.0f, 50.0f);
            add_tooltip("Steering rate limit (rad/s).");
            ImGui::DragFloat("Steer max impulse", &vehicle_rig1_control_steer_max_impulse_, 10.0f, 0.0f, 100000.0f);
            add_tooltip("Max steering motor impulse (force).");
            ImGui::DragFloat("Drive velocity", &vehicle_rig1_control_drive_velocity_, 0.1f, 0.0f, 200.0f);
            add_tooltip("Target wheel angular velocity for drive.");
            ImGui::DragFloat("Drive max impulse", &vehicle_rig1_control_drive_max_impulse_, 10.0f, 0.0f, 100000.0f);
            add_tooltip("Max drive motor impulse.");
            ImGui::DragFloat("Brake max impulse", &vehicle_rig1_control_brake_max_impulse_, 10.0f, 0.0f, 100000.0f);
            add_tooltip("Max impulse when braking/coasting.");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Rig Monitor"))
        {
            const eeng::ecs::VehicleRig1ControlComponent* control = nullptr;
            const eeng::ecs::VehicleRig1RigComponent* rig = nullptr;
            int rig_count = 0;
            if (ctx && ctx->entity_manager)
            {
                auto& registry = ctx->entity_manager->registry();
                auto view = registry.view<eeng::ecs::VehicleRig1ControlComponent, eeng::ecs::VehicleRig1RigComponent>();
                rig_count = 0;
                for (auto it = view.begin(); it != view.end(); ++it)
                    ++rig_count;
                if (auto it = view.begin(); it != view.end())
                {
                    const entt::entity control_entity = *it;
                    control = registry.try_get<eeng::ecs::VehicleRig1ControlComponent>(control_entity);
                    rig = registry.try_get<eeng::ecs::VehicleRig1RigComponent>(control_entity);
                }
            }

            if (control)
            {
                ImGui::Text("Active rigs: %d", rig_count);
                ImGui::Text("Input steer/drive: %.2f / %.2f", control->steer_input, control->drive_input);
                ImGui::Text("Steer target/angle: %.2f / %.2f", control->steer_target, control->steer_angle);
                ImGui::Text("Steer speed/impulse: %.1f / %.1f", control->steer_speed, control->steer_max_impulse);
                ImGui::Text("Drive vel/impulse: %.1f / %.1f", control->drive_velocity, control->drive_max_impulse);
                ImGui::Text("Brake impulse: %.1f", control->brake_max_impulse);
                ImGui::Text("Controller id: %d", control->controller_id);
                if (rig)
                    ImGui::Text("Wheel count: %d", static_cast<int>(rig->wheels.size()));
            }
            else
            {
                ImGui::TextDisabled("VehicleRig1 not spawned (or control missing)");
            }

            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Piston Rig", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ensure_piston_rig_config();

        if (ImGui::TreeNodeEx("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat3("Position", &piston_rig_spec_.position.x, 0.05f);
            add_tooltip("World-space position for the piston rig root.");

            if (ImGui::Button("Spawn Piston Rig"))
                spawn_piston_rig_from_prefab();

            ImGui::SameLine();
            if (ImGui::Button("Reset Config"))
                reset_piston_rig_config();

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Rig Config"))
        {
            ImGui::TextDisabled("Name prefix: %s", piston_rig_spec_.name_prefix.c_str());
            ImGui::TextDisabled("Chunk tag: %s", piston_rig_spec_.chunk_tag.c_str());
            ImGui::TextDisabled("Constraint: 6DoF distance (axis from anchors)");

            if (ctx && ctx->entity_manager)
            {
                if (auto* em = dynamic_cast<eeng::EntityManager*>(ctx->entity_manager.get()))
                {
                    auto draw_entity_picker = [&](const char* label, eeng::ecs::EntityRef& ref, const char* popup_id)
                    {
                        ImGui::TextUnformatted(label);
                        ImGui::SameLine();
                        const std::string current_label = eeng::editor::detail::make_entity_label(*em, ref);
                        if (ImGui::Button(current_label.c_str()))
                            ImGui::OpenPopup(popup_id);
                        if (eeng::editor::entity_picker_popup(popup_id, ref, *em))
                        {
                        }
                    };

                    draw_entity_picker("Body A", piston_rig_spec_.body_a, "piston_body_a_picker");
                    draw_entity_picker("Body B", piston_rig_spec_.body_b, "piston_body_b_picker");
                }
            }

            ImGui::DragFloat3("Anchor A local", &piston_rig_spec_.anchor_local_a.x, 0.02f);
            add_tooltip("Anchor offset: root-local when sockets are off, target-local when sockets are on.");
            ImGui::DragFloat3("Anchor B local", &piston_rig_spec_.anchor_local_b.x, 0.02f);
            add_tooltip("Anchor offset: root-local when sockets are off, target-local when sockets are on.");
            ImGui::DragFloat3("Axis local", &piston_rig_spec_.axis_local.x, 0.02f);
            add_tooltip("Root-local axis used by the drive and alignment.");

            ImGui::Checkbox("Use sockets", &piston_rig_spec_.use_sockets);
            add_tooltip("Attach anchor entities via sockets instead of reparenting.");

            ImGui::Checkbox("Disable collisions", &piston_rig_spec_.disable_collisions);
            add_tooltip("Disable collision response between constrained bodies.");

            ImGui::DragFloat("Stroke min", &piston_rig_spec_.stroke_min, 0.01f, -10.0f, 10.0f);
            add_tooltip("Minimum extension along the axis.");
            ImGui::DragFloat("Stroke max", &piston_rig_spec_.stroke_max, 0.01f, -10.0f, 10.0f);
            add_tooltip("Maximum extension along the axis.");

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Drive Config"))
        {
            ImGui::DragFloat("Max force", &piston_rig_spec_.max_force, 10.0f, 0.0f, 100000.0f);
            add_tooltip("Max motor impulse/force.");
            ImGui::DragFloat("Max velocity", &piston_rig_spec_.max_velocity, 0.1f, 0.0f, 1000.0f);
            add_tooltip("Max linear velocity (units/sec).");

            static const char* kModes[] = { "Hold", "Extend", "Contract", "Position" };
            ImGui::Combo("Mode", &piston_rig_spec_.mode, kModes, IM_ARRAYSIZE(kModes));
            add_tooltip("Hold locks at current extension, Extend/Contract drives to limits.");

            if (piston_rig_spec_.mode == 3)
            {
                ImGui::DragFloat("Target extension", &piston_rig_spec_.target_extension, 0.01f, 0.0f, 1.0f);
                add_tooltip("Normalized [0,1] position within the stroke.");
            }

            ImGui::Checkbox("Lock when idle", &piston_rig_spec_.lock_when_idle);
            add_tooltip("Keep constraint locked when not driving.");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Live Control (Play)"))
        {
            eeng::ecs::PistonConstraintDriveComponent* live = nullptr;
            int rig_count = 0;
            if (ctx && ctx->entity_manager)
            {
                auto& registry = ctx->entity_manager->registry();
                auto view = registry.view<eeng::ecs::PistonConstraintDriveComponent>();
                rig_count = 0;
                for (auto it = view.begin(); it != view.end(); ++it)
                    ++rig_count;
                if (auto it = view.begin(); it != view.end())
                {
                    const entt::entity drive_entity = *it;
                    live = registry.try_get<eeng::ecs::PistonConstraintDriveComponent>(drive_entity);
                }
            }

            if (!live)
            {
                ImGui::TextDisabled("No piston rigs found.");
            }
            else
            {
                ImGui::Text("Active rigs: %d (editing first)", rig_count);
                ImGui::Text("Current pos/ext: %.3f / %.3f", live->current_position, live->current_extension);

                ImGui::DragFloat("Stroke min##piston_live", &live->stroke_min, 0.01f, -10.0f, 10.0f);
                add_tooltip("Minimum extension along the axis.");
                ImGui::DragFloat("Stroke max##piston_live", &live->stroke_max, 0.01f, -10.0f, 10.0f);
                add_tooltip("Maximum extension along the axis.");

                ImGui::DragFloat("Max force##piston_live", &live->max_force, 10.0f, 0.0f, 100000.0f);
                add_tooltip("Max motor impulse/force.");
                ImGui::DragFloat("Max velocity##piston_live", &live->max_velocity, 0.1f, 0.0f, 1000.0f);
                add_tooltip("Max linear velocity (units/sec).");

                static const char* kLiveModes[] = { "Hold", "Extend", "Contract", "Position" };
                ImGui::Combo("Mode##piston_live", &live->mode, kLiveModes, IM_ARRAYSIZE(kLiveModes));
                add_tooltip("Hold locks at current extension, Extend/Contract drives to limits.");

                if (live->mode == 3)
                {
                    ImGui::DragFloat("Target extension##piston_live", &live->target_extension, 0.01f, 0.0f, 1.0f);
                    add_tooltip("Normalized [0,1] position within the stroke.");
                }

                ImGui::Checkbox("Lock when idle##piston_live", &live->lock_when_idle);
                add_tooltip("Keep constraint locked when not driving.");
            }

            ImGui::TreePop();
        }
    }

    ImGui::End(); // end info window
}

void Game::destroy()
{
    if (ctx && ctx->services
        && ctx->services->debug_render_settings == runtime_pipeline_.debug_render_settings())
    {
        ctx->services->debug_render_settings = nullptr;
    }
    runtime_pipeline_.shutdown();
}

void Game::update_active_camera_state()
{
    if (!ctx || !ctx->entity_manager)
        return;

    auto& registry = ctx->entity_manager->registry();

    const auto copy_third_person = [&](const eeng::editor::ThirdPersonCameraComponent& camera)
    {
        active_camera.position = camera.position;
        active_camera.forward = camera.forward;
        active_camera.up = camera.up;
        active_camera.model_to_view = camera.model_to_view;
        active_camera.view_to_world = camera.view_to_world;
        active_camera.near_plane = camera.near_plane;
        active_camera.far_plane = camera.far_plane;
    };

    const auto copy_first_person = [&](const eeng::editor::FirstPersonCameraComponent& camera)
    {
        active_camera.position = camera.position;
        active_camera.forward = camera.forward;
        active_camera.up = camera.up;
        active_camera.model_to_view = camera.model_to_view;
        active_camera.view_to_world = camera.view_to_world;
        active_camera.near_plane = camera.near_plane;
        active_camera.far_plane = camera.far_plane;
    };

    // Prefer an active third-person camera if present.
    auto third_view = registry.view<eeng::editor::ThirdPersonCameraComponent>();
    for (auto entity : third_view)
    {
        const auto& camera = third_view.get<eeng::editor::ThirdPersonCameraComponent>(entity);
        if (!camera.active)
            continue;
        copy_third_person(camera);
        return;
    }

    // Fall back to an active first-person camera.
    auto first_view = registry.view<eeng::editor::FirstPersonCameraComponent>();
    for (auto entity : first_view)
    {
        const auto& camera = first_view.get<eeng::editor::FirstPersonCameraComponent>(entity);
        if (!camera.active)
            continue;
        copy_first_person(camera);
        return;
    }

    // If nothing is active, pick the first available camera in a stable order.
    if (third_view.begin() != third_view.end())
    {
        const auto& camera = third_view.get<eeng::editor::ThirdPersonCameraComponent>(*third_view.begin());
        copy_third_person(camera);
        return;
    }

    if (first_view.begin() != first_view.end())
    {
        const auto& camera = first_view.get<eeng::editor::FirstPersonCameraComponent>(*first_view.begin());
        copy_first_person(camera);
        return;
    }
}
