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
#include "ecs/TransformComponent.hpp"
#include "engineapi/IInputManager.hpp"
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
