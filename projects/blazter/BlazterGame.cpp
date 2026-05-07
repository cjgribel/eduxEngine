#include "BlazterGame.hpp"

#include "BatchRegistry.hpp"
#include "editor/OverlayRenderSettingsPersistence.hpp"
#include "glmcommon.hpp"
#include "imgui.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace eeng::blazter
{
    namespace
    {
        struct LevelDef
        {
            std::string_view id;
            std::string_view display_name;
            std::span<const std::string_view> preload_batches;
        };

        constexpr std::string_view kLevel01Batches[] = {
            "level_01",
            "terrain_TerrainRecipe_chunk_0_0",
            "terrain_TerrainRecipe_chunk_1_0",
            "terrain_TerrainRecipe_chunk_0_1",
            "terrain_TerrainRecipe_chunk_1_1",
        };

        constexpr LevelDef kLevel01{
            "level_01",
            "Level 01",
            kLevel01Batches
        };

        constexpr const LevelDef& current_level_def()
        {
            return kLevel01;
        }
    }

    BlazterGame::BlazterGame(std::shared_ptr<EngineContext> ctx)
        : ctx_(std::move(ctx))
    {
        // Keep construction lightweight. Later we can cache service pointers,
        // register game-singleton helpers, or initialize Blazter-specific data
        // containers that should exist before runtime init.
    }

    bool BlazterGame::init()
    {
        // Runtime-wide setup: initialize pipelines, register
        // render/debug settings, and hook shared engine services that both
        // edit and play modes should use.
        reset_session_flow();
        reset_game_camera();
        if (ctx_)
        {
            runtime_pipeline_.init(*ctx_);
            if (ctx_->services)
            {
                ctx_->services->debug_render_settings = runtime_pipeline_.debug_render_settings_edit();
                ctx_->services->debug_render_settings_edit = runtime_pipeline_.debug_render_settings_edit();
                ctx_->services->debug_render_settings_play = runtime_pipeline_.debug_render_settings_play();
                ctx_->services->overlay_render_settings = runtime_pipeline_.overlay_render_settings();

                eeng::editor::load_overlay_render_settings(
                    *ctx_,
                    *runtime_pipeline_.overlay_render_settings(),
                    *runtime_pipeline_.debug_render_settings_edit(),
                    *runtime_pipeline_.debug_render_settings_play());
            }
        }
        return true;
    }

    void BlazterGame::update(float time_s, float deltaTime_s)
    {
        // Legacy: mode-agnostic mode. Can be used for shared update logic.
        (void)time_s;
        update_edit(time_s, deltaTime_s);
    }

    void BlazterGame::update_edit(float time_s, float deltaTime_s)
    {
        // Editor-side simulation belongs here: free camera support, gizmo-safe
        // preview behavior, authoring helpers, and anything that should update
        // while the world is not in active gameplay.
        (void)time_s;
        if (ctx_)
            runtime_pipeline_.update_edit(*ctx_, deltaTime_s);
    }

    void BlazterGame::update_play(float time_s, float deltaTime_s)
    {
        // Real gameplay simulation belongs here: player controller, AI,
        // projectile updates, combat rules, pickups, and any runtime-only
        // systems that should advance during Warm/Cold Play.
        (void)time_s;
        update_flow(deltaTime_s);
        if (ctx_)
            runtime_pipeline_.update_play(*ctx_, deltaTime_s);
    }

    void BlazterGame::render(float time_s, int windowWidth, int windowHeight)
    {
        // Legacy render hook kept only for compatibility with older callers.
        // Camera selection is now explicit in RenderContext.camera_view.
        (void)time_s;
        (void)windowWidth;
        (void)windowHeight;
    }

    void BlazterGame::render_scene(const RenderContext& ctx)
    {
        // Scene rendering belongs here once Blazter has a visual identity:
        // world draw calls, character and weapon effects, particle blasters,
        // decals, lighting tweaks, and any play/edit scene visualization.
        if (!ctx_ || !ctx_->entity_manager)
            return;
        if (!ctx.camera_view.valid)
            return;

        auto& registry = ctx_->entity_manager->registry();
        const glm::mat4 proj_view = ctx.camera_view.view_proj;
        const glm::vec3 eye_pos = ctx.camera_view.position;
        const glm::vec3 light_pos = eye_pos + glm::vec3(12.0f, 18.0f, 8.0f);
        const glm::vec3 light_color(1.0f, 0.98f, 0.92f);

        runtime_pipeline_.render_entities(
            registry,
            *ctx_,
            proj_view,
            light_pos,
            light_color,
            eye_pos);

        runtime_pipeline_.render_particles(
            registry,
            *ctx_,
            proj_view,
            ctx.camera_view.right,
            ctx.camera_view.up);
    }

    void BlazterGame::render_overlay(const RenderContext& ctx)
    {
        // Use overlay rendering for debug lines, targeting reticles, helper
        // markers, and other camera-space overlays that should sit above the
        // scene without being part of the main world render.
        if (!ctx_ || !ctx_->entity_manager || !ctx_->shape_renderer || !ctx.camera_view.valid)
            return;

        auto& registry = ctx_->entity_manager->registry();
        const auto vp_p_v = ctx.camera_view.viewport * ctx.camera_view.view_proj;
        runtime_pipeline_.render_debug(
            registry,
            *ctx_,
            *ctx_->shape_renderer,
            vp_p_v,
            ctx.window_height);
        runtime_pipeline_.render_runtime_overlays(
            registry,
            *ctx_,
            *ctx_->shape_renderer);
    }

    void BlazterGame::render_gui(const RenderContext& ctx)
    {
        // HUD and in-game UI belong here: health/ammo, objective widgets,
        // pause screens, debug panels, and temporary tuning controls.
        (void)ctx;

        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Blazter Flow"))
        {
            ImGui::End();
            return;
        }

        ImGui::Text("State: %.*s",
            static_cast<int>(flow_state_label().size()),
            flow_state_label().data());
        ImGui::Text("Policy: %s",
            active_play_policy() == PlayModePolicy::Preview ? "Preview" : "Warm Play");
        ImGui::Text("Selected players: %d", selected_player_count_);
        ImGui::Text("Active players: %d", active_player_count_);
        ImGui::Separator();

        switch (flow_state_)
        {
            case SessionFlowState::BootLoading:
                ImGui::TextWrapped(
                    "Booting Blazter. This stage should stay brief and get us to a responsive "
                    "menu quickly, rather than pretending the whole level is already loaded.");
                ImGui::ProgressBar(boot_load_progress_, ImVec2(-FLT_MIN, 0.0f), "Boot");
                break;

            case SessionFlowState::MainMenu:
                ImGui::TextWrapped(
                    "Main menu placeholder. The predetermined start level is preloading in the "
                    "background while the player chooses the local player count.");
                ImGui::Text("Start level: %.*s",
                    static_cast<int>(current_level_def().display_name.size()),
                    current_level_def().display_name.data());
                ImGui::SliderInt("Players", &selected_player_count_, 1, 4);
                ImGui::ProgressBar(level_preload_progress(), ImVec2(-FLT_MIN, 0.0f), "Level preload");
                if (!is_level_preload_ready())
                    ImGui::TextDisabled("Start level still preloading in the background...");
                else
                    ImGui::TextDisabled("Start level content is ready.");
                if (ImGui::Button("Start"))
                    start_requested_ = true;
                break;

            case SessionFlowState::SessionLoading:
                ImGui::TextWrapped(
                    "Session-loading placeholder. This is where we would finish any required "
                    "loads, spawn player entities, and switch to the shared gameplay camera.");
                ImGui::ProgressBar(session_load_progress_, ImVec2(-FLT_MIN, 0.0f), "Session");
                ImGui::Text("Preparing %d player(s)...", selected_player_count_);
                break;

            case SessionFlowState::Playing:
                if (should_run_game_boot())
                {
                    ImGui::TextWrapped(
                        "Gameplay placeholder. The menu flow has handed off to a live session.");
                }
                else
                {
                    ImGui::TextWrapped(
                        "Preview policy: game boot is bypassed and the snapshot-authored world "
                        "is played directly.");
                }
                ImGui::Text("Shared camera active for %d player(s).", active_player_count_);
                if (should_run_game_boot() && ImGui::Button("Return To Menu"))
                    transition_to(SessionFlowState::MainMenu);
                break;
        }

        ImGui::End();
    }

    void BlazterGame::destroy()
    {
        // Tear down only what this runtime owns. As Blazter grows, release
        // transient gameplay state, detach service hooks, and persist any
        // editor-facing settings that should survive app restarts.
        if (ctx_ && ctx_->services
            && ctx_->services->debug_render_settings_edit == runtime_pipeline_.debug_render_settings_edit()
            && ctx_->services->debug_render_settings_play == runtime_pipeline_.debug_render_settings_play()
            && ctx_->services->overlay_render_settings == runtime_pipeline_.overlay_render_settings())
        {
            ctx_->services->debug_render_settings = nullptr;
            ctx_->services->debug_render_settings_edit = nullptr;
            ctx_->services->debug_render_settings_play = nullptr;
            ctx_->services->overlay_render_settings = nullptr;
        }
        runtime_pipeline_.shutdown();
    }

    PlayModePolicy BlazterGame::preferred_play_policy() const
    {
        // Default to Warm Play so the new project exercises runtime-owned boot
        // through startup batch config rather than Preview snapshots.
        return PlayModePolicy::Strict;
    }

    std::vector<std::string> BlazterGame::preferred_startup_batches() const
    {
        // Temporary fallback for non-editor hosts that do not supply project
        // config yet. Once standalone boot reads project config too, this can
        // likely disappear or become purely diagnostic.
        return { std::string("blazter_boot") };
    }

    void BlazterGame::on_play_world_created(EngineContext& ctx)
    {
        // Use this hook for play-world wiring that must happen before startup
        // batches load, such as registering world services, selecting runtime
        // systems, or preparing boot state consumed during content load.
        (void)ctx;
    }

    void BlazterGame::on_enter_play(EngineContext& ctx)
    {
        // Post-load boot belongs here: spawn or possess the player, initialize
        // ammo/health/session state, arm default weapons, bind cameras, and
        // kick off any scripted or data-driven mission flow.
        (void)ctx;
        reset_game_camera();
        if (should_run_game_boot())
        {
            reset_session_flow();
        }
        else
        {
            // Preview is intentionally direct: use the snapshot-authored world
            // immediately rather than re-running menu or level boot behavior.
            flow_state_ = SessionFlowState::Playing;
            flow_state_elapsed_s_ = 0.0f;
            boot_load_progress_ = 1.0f;
            session_load_progress_ = 1.0f;
            selected_player_count_ = 1;
            active_player_count_ = 1;
            start_requested_ = false;
            queued_level_batches_.clear();
        }
    }

    void BlazterGame::on_exit_play(EngineContext& ctx)
    {
        // Play-session cleanup belongs here: clear transient session state,
        // detach runtime-only systems, stop looping effects, and prepare for a
        // clean return to Edit or a future Cold Play restart.
        (void)ctx;
        reset_session_flow();
    }

    void BlazterGame::reset_session_flow()
    {
        flow_state_ = SessionFlowState::BootLoading;
        flow_state_elapsed_s_ = 0.0f;
        boot_load_progress_ = 0.0f;
        session_load_progress_ = 0.0f;
        selected_player_count_ = 1;
        active_player_count_ = 0;
        start_requested_ = false;
        queued_level_batches_.clear();
    }

    void BlazterGame::reset_game_camera()
    {
        // Temporary game-owned camera. Later this should become a proper
        // gameplay camera rig driven by player/session state rather than a
        // fixed vantage point.
        game_camera_ = GameCamera{};
    }

    PlayModePolicy BlazterGame::active_play_policy() const
    {
        if (ctx_ && ctx_->services)
            return ctx_->services->active_play_mode_policy.load(std::memory_order_relaxed);
        return PlayModePolicy::Preview;
    }

    bool BlazterGame::should_run_game_boot() const
    {
        return active_play_policy() != PlayModePolicy::Preview;
    }

    void BlazterGame::transition_to(SessionFlowState next_state)
    {
        flow_state_ = next_state;
        flow_state_elapsed_s_ = 0.0f;

        if (next_state == SessionFlowState::MainMenu)
        {
            active_player_count_ = 0;
            session_load_progress_ = 0.0f;
            start_requested_ = false;
        }
    }

    void BlazterGame::update_flow(float deltaTime_s)
    {
        flow_state_elapsed_s_ += std::max(0.0f, deltaTime_s);

        switch (flow_state_)
        {
            case SessionFlowState::BootLoading:
                update_boot_loading(deltaTime_s);
                break;
            case SessionFlowState::MainMenu:
                update_main_menu(deltaTime_s);
                break;
            case SessionFlowState::SessionLoading:
                update_session_loading(deltaTime_s);
                break;
            case SessionFlowState::Playing:
                break;
        }
    }

    void BlazterGame::update_boot_loading(float deltaTime_s)
    {
        boot_load_progress_ = std::min(1.0f, boot_load_progress_ + std::max(0.0f, deltaTime_s) * 4.0f);

        if (boot_load_progress_ >= 1.0f)
            transition_to(SessionFlowState::MainMenu);
    }

    void BlazterGame::update_main_menu(float deltaTime_s)
    {
        (void)deltaTime_s;
        if (!should_run_game_boot())
            return;
        queue_level_preload();

        if (start_requested_)
            begin_session_start();
    }

    void BlazterGame::update_session_loading(float deltaTime_s)
    {
        (void)deltaTime_s;
        if (!should_run_game_boot())
            return;
        queue_level_preload();
        session_load_progress_ = level_preload_progress();

        if (is_level_preload_ready())
            finish_session_start();
    }

    void BlazterGame::queue_level_preload()
    {
        if (!ctx_ || !ctx_->batch_registry)
            return;

        for (const auto batch_name : current_level_def().preload_batches)
        {
            BatchId id{};
            if (!ctx_->batch_registry->try_get_batch_id_by_name(std::string(batch_name), id))
                continue;
            if (ctx_->batch_registry->is_batch_loaded(id))
            {
                queued_level_batches_.erase(std::string(batch_name));
                continue;
            }
            // Queue each residency request once while the batch is pending. Re-submitting
            // every frame can flood the batch strand with duplicate loads and make both
            // preload and play-exit appear to hang.
            if (!queued_level_batches_.insert(std::string(batch_name)).second)
                continue;
            ctx_->batch_registry->queue_load(id, *ctx_);
        }
    }

    float BlazterGame::level_preload_progress() const
    {
        if (!ctx_ || !ctx_->batch_registry || current_level_def().preload_batches.empty())
            return 1.0f;

        std::size_t loaded_count = 0;
        std::size_t known_count = 0;
        for (const auto batch_name : current_level_def().preload_batches)
        {
            BatchId id{};
            if (!ctx_->batch_registry->try_get_batch_id_by_name(std::string(batch_name), id))
                continue;
            ++known_count;
            if (ctx_->batch_registry->is_batch_loaded(id))
                ++loaded_count;
        }

        if (known_count == 0)
            return 0.0f;
        return static_cast<float>(loaded_count) / static_cast<float>(known_count);
    }

    bool BlazterGame::is_level_preload_ready() const
    {
        return level_preload_progress() >= 1.0f;
    }

    void BlazterGame::begin_session_start()
    {
        start_requested_ = false;
        queue_level_preload();
        session_load_progress_ = level_preload_progress();
        transition_to(SessionFlowState::SessionLoading);
    }

    void BlazterGame::finish_session_start()
    {
        // This is the seam where real session boot plugs in:
        // 1. spawn or instantiate the selected number of player entities
        // 2. assign controller/input ownership
        // 3. activate/configure the shared gameplay camera
        // 4. initialize any runtime-only match state
        active_player_count_ = selected_player_count_;
        transition_to(SessionFlowState::Playing);
    }

    std::string_view BlazterGame::flow_state_label() const
    {
        switch (flow_state_)
        {
            case SessionFlowState::BootLoading:
                return "BootLoading";
            case SessionFlowState::MainMenu:
                return "MainMenu";
            case SessionFlowState::SessionLoading:
                return "SessionLoading";
            case SessionFlowState::Playing:
                return "Playing";
        }
        return "Unknown";
    }

    bool BlazterGame::build_play_camera_view(CameraView& out, glm::ivec2 window_size) const
    {
        if (window_size.x <= 0 || window_size.y <= 0)
            return false;

        const glm::vec3 forward{
            std::cos(game_camera_.pitch) * std::cos(game_camera_.yaw),
            std::sin(game_camera_.pitch),
            std::cos(game_camera_.pitch) * std::sin(game_camera_.yaw)
        };
        const glm::vec3 eye = game_camera_.position;
        const glm::vec3 target = eye + glm::normalize(forward);
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const float aspect_ratio = static_cast<float>(window_size.x) / static_cast<float>(window_size.y);

        out.view = glm::lookAt(eye, target, up);
        out.proj = glm::perspective(
            glm::radians(game_camera_.fov_y_degrees),
            aspect_ratio,
            game_camera_.near_plane,
            game_camera_.far_plane);
        out.viewport = glm_aux::create_viewport_matrix(
            0.0f,
            0.0f,
            static_cast<float>(window_size.x),
            static_cast<float>(window_size.y),
            0.0f,
            1.0f);
        out.near_plane = game_camera_.near_plane;
        out.far_plane = game_camera_.far_plane;
        out.window_size = window_size;
        out.valid = true;
        finalize_camera_view(out);
        return true;
    }
} // namespace eeng::blazter
