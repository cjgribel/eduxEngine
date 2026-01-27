#ifndef Game_hpp
#define Game_hpp
#pragma once

#include "GameBase.h"
#include "RenderableMesh.hpp"
#include "ForwardRenderer.hpp"
#include "ecs/systems/AnimationSystem.hpp"
#include "ecs/systems/AnimationGraphSystem.hpp"
#include "ecs/systems/PlayerControllerSystem.hpp"
#include "ecs/systems/PhysicsSystem.hpp"
#include "ecs/systems/ScriptSystem.hpp"
#include "ecs/systems/DebugRenderSystem.hpp"
#include "ecs/systems/RenderSystem.hpp"
#include "ecs/systems/StickyNoteSystem.hpp"
#include "ecs/systems/TransformSystem.hpp"
#include "editor/EditorRuntime.hpp"
#include "FirstPersonCameraSystem.hpp"
#include "ThirdPersonCameraSystem.hpp"
#include "glmcommon.hpp"
#include <entt/fwd.hpp> // For entt::registry - remove from here

// --> ENGINE API
#include "ShapeRenderer.hpp"
#include "Storage.hpp"
#include "EngineContext.hpp"

#if 0
// #define SPONZA_PATH "assets/crytek-sponza_hansen/sponza.obj"
#define CHARACTER_PATH "assets/Ultimate Platformer Pack/Character/Character.fbx"
#define CHARACTER_ANIM 0
#define ENEMY_PATH "assets/Ultimate Platformer Pack/Enemies/Enemy.fbx"
#define ENEMY_ANIM 0
#define EXORED_PATH "assets/ExoRed/exo_red.fbx"
#define EXORED_ANIM0_PATH "assets/ExoRed/idle.fbx"
#define EXORED_ANIM 1
#define EVE_PATH "assets/Eve/Eve By J.Gonzales.fbx"
#define EVE_ANIM0_PATH "assets/Eve/walking.fbx"
#define EVE_ANIM 1
#define MANNEQUIN_PATH "assets/UEMannequin/SK_Mannequin_tex.FBX"
#define MANNEQUIN_ANIM0_PATH "assets/UEMannequin/Anim_Stand_Idle_PreviewMesh.fbx"
#define MANNEQUIN_ANIM 0
#define UE5QUINN_PATH "assets/UE5_Mannequin/SKM_Quinn_tex.fbx"
#define UE5QUINN_ANIM0_PATH "assets/UE5_Mannequin/MF_Idle.fbx"
#define UE5QUINN_ANIM 0
#endif

/// @brief A Game may hold, update and render 3D geometry and GUI elements
class Game : public eeng::GameBase
{
public:
    Game(std::shared_ptr<eeng::EngineContext> ctx)
        : ctx(ctx)
    {
    }

    /// @brief For game resource initialization
    /// @return 
    bool init() override;

    /// @brief General update method that is called each frame
    /// @param time Total time elapsed in seconds
    /// @param deltaTime Time elapsed since the last frame
    /// @param input Input from mouse, keyboard and controllers
    void update(
        float time,
        float deltaTime) override;
    void update_edit(
        float time,
        float deltaTime) override;
    void update_play(
        float time,
        float deltaTime) override;

    /// @brief For rendering of game contents
    /// @param time Total time elapsed in seconds
    /// @param screenWidth Current width of the window in pixels
    /// @param screenHeight Current height of the window in pixels
    void render(
        float time,
        int windowWidth,
        int windowHeight) override;
    void render_edit(
        float time,
        int windowWidth,
        int windowHeight) override;
    void render_play(
        float time,
        int windowWidth,
        int windowHeight) override;

    /// @brief For destruction of game resources
    void destroy() override;

private:
    /// @brief For rendering of GUI elements
    void renderUI();

    // ENGINE API
    std::shared_ptr<eeng::EngineContext> ctx;
    // <--

    // Renderer for rendering imported animated or non-animated models
    eeng::ForwardRendererPtr forwardRenderer;

    // Immediate-mode renderer for basic 2D or 3D primitives
    ShapeRendererPtr shapeRenderer;

    // TODO: Core systems to Engine
    std::unique_ptr<eeng::ecs::systems::RenderSystem> renderSystem;
    std::unique_ptr<eeng::ecs::systems::PlayerControllerSystem> playerControllerSystem;
    std::unique_ptr<eeng::ecs::systems::AnimationSystem> animationSystem;
    std::unique_ptr<eeng::ecs::systems::AnimationGraphSystem> animationGraphSystem;
    std::unique_ptr<eeng::ecs::systems::TransformSystem> transformSystem;
    // Bullet integration + ECS sync.
    std::unique_ptr<eeng::ecs::systems::PhysicsSystem> physicsSystem;
    // Script system placeholder wired into component lifetime.
    std::unique_ptr<eeng::ecs::systems::ScriptSystem> scriptSystem;
    std::unique_ptr<eeng::ecs::systems::DebugRenderSystem> debugRenderSystem;
    std::unique_ptr<eeng::ecs::systems::StickyNoteSystem> stickyNoteSystem;
    std::unique_ptr<eeng::editor::EditorRuntime> editorRuntime;
    std::unique_ptr<eeng::module1::systems::ThirdPersonCameraSystem> thirdPersonCameraSystem;
    std::unique_ptr<eeng::module1::systems::FirstPersonCameraSystem> firstPersonCameraSystem;

    // Entity registry - to use in labs
    std::shared_ptr<entt::registry> entity_registry; // unique + and out weak ptrs?

    // std::unique_ptr<eeng::ResourceRegistry> resource_registry;

    // <-- ENGINE API

    // Matrices for view, projection and viewport
    struct Matrices
    {
        glm::mat4 V;
        glm::mat4 P;
        glm::mat4 VP;
        glm::ivec2 windowSize;
    } matrices;

    enum class CameraMode
    {
        ThirdPerson = 0,
        FirstPerson = 1
    };

    struct ActiveCameraState
    {
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 forward{ 0.0f, 0.0f, -1.0f };
        glm::vec3 up{ 0.0f, 1.0f, 0.0f };
        glm::mat4 model_to_view{ 1.0f };
        glm::mat4 view_to_world{ 1.0f };
        float near_plane = 1.0f;
        float far_plane = 500.0f;
    } active_camera;

    eeng::ecs::Entity active_entity;
    eeng::ecs::Entity player_entity;
    eeng::ecs::Entity third_person_camera_entity;
    eeng::ecs::Entity first_person_camera_entity;
    eeng::ecs::Entity active_camera_entity;
    CameraMode active_camera_mode = CameraMode::ThirdPerson;
    glm_aux::Ray view_ray;
    bool f_was_down = false;

    // Light properties
    struct PointLight
    {
        glm::vec3 pos;
        glm::vec3 color{ 1.0f, 1.0f, 0.8f };
    } pointlight;

    // Game meshes
    std::shared_ptr<eeng::RenderableMesh> grassMesh, horseMesh, characterMesh;
#ifdef SPONZA_PATH
    std::shared_ptr<eeng::RenderableMesh> sponzaMesh;
#endif
#ifdef CHARACTER_PATH
    std::shared_ptr<eeng::RenderableMesh> qcharacterMesh;
#endif
#ifdef ENEMY_PATH
    std::shared_ptr<eeng::RenderableMesh> enemyMesh;
#endif
#ifdef EXORED_PATH
    std::shared_ptr<eeng::RenderableMesh> exoredMesh;
#endif
#ifdef EVE_PATH
    std::shared_ptr<eeng::RenderableMesh> eveMesh;
#endif
#ifdef MANNEQUIN_PATH
    std::shared_ptr<eeng::RenderableMesh> mannequinMesh;
#endif
#ifdef UE5QUINN_PATH
    std::shared_ptr<eeng::RenderableMesh> ue5quinnMesh;
#endif

    // Game entity transformations
    glm::mat4 characterWorldMatrix1, characterWorldMatrix2, characterWorldMatrix3;
    glm::mat4 grassWorldMatrix, horseWorldMatrix;

    // Game entity AABBs (for collision detection or visualization)
    eeng::AABB character_aabb1, character_aabb2, character_aabb3, horse_aabb, grass_aabb;

    // Placeholder animation state
    int characterAnimIndex = -1;
    float characterAnimSpeed = 1.0f;
    float characterAnimBlend = 0.5f;

    // Stats
    int drawcallCount = 0;

    void set_active_camera_mode(CameraMode mode);
    void ensure_editor_camera_entities();
    void sync_active_entity();
    void refresh_active_camera_state();

    // Placeholder for a future play-mode toggle that will swap in runtime cameras.
    bool play_mode = false;
};

#endif
