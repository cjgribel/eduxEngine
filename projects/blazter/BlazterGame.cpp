#include "BlazterGame.hpp"

#include "BatchRegistry.hpp"
#include "editor/OverlayRenderSettingsPersistence.hpp"
#include "imgui.h"
#include <glm/glm.hpp>
#include <algorithm>

namespace eeng::blazter
{
    BlazterGame::BlazterGame(std::shared_ptr<EngineContext> ctx)
        : ctx_(std::move(ctx))
    {
        // Keep construction lightweight. Later we can cache service pointers,
        // register game-singleton helpers, or initialize Blazter-specific data
        // containers that should exist before runtime init.
    }

    bool BlazterGame::init()
    {
        // Runtime-wide setup belongs here: initialize pipelines, register
        // render/debug settings, and hook shared engine services that both
        // edit and play modes should use.
        reset_session_flow();
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
        // Keep the old render hook minimal. If some systems still expect it,
        // use it for shared per-frame view publication rather than game logic.
        (void)time_s;
        publish_overlay_view(windowWidth, windowHeight);
    }

    void BlazterGame::render_scene(const RenderContext& ctx)
    {
        // Scene rendering belongs here once Blazter has a visual identity:
        // world draw calls, character and weapon effects, particle blasters,
        // decals, lighting tweaks, and any play/edit scene visualization.
        (void)ctx;
    }

    void BlazterGame::render_overlay(const RenderContext& ctx)
    {
        // Use overlay rendering for debug lines, targeting reticles, helper
        // markers, and other camera-space overlays that should sit above the
        // scene without being part of the main world render.
        publish_overlay_view(ctx.window_width, ctx.window_height);
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
        ImGui::Text("Selected players: %d", selected_player_count_);
        ImGui::Text("Active players: %d", active_player_count_);
        ImGui::Separator();

        switch (flow_state_)
        {
            case SessionFlowState::BootLoading:
                ImGui::TextWrapped(
                    "Booting Blazter. This is where we can show a loading screen while "
                    "menu essentials come online and heavy world content begins loading.");
                ImGui::ProgressBar(boot_load_progress_, ImVec2(-FLT_MIN, 0.0f), "Boot");
                ImGui::ProgressBar(background_world_load_progress_, ImVec2(-FLT_MIN, 0.0f), "Terrain");
                break;

            case SessionFlowState::MainMenu:
                ImGui::TextWrapped(
                    "Main menu placeholder. The terrain continues loading in the background "
                    "while the player chooses the local player count.");
                ImGui::SliderInt("Players", &selected_player_count_, 1, 4);
                ImGui::ProgressBar(background_world_load_progress_, ImVec2(-FLT_MIN, 0.0f), "Terrain");
                if (background_world_load_progress_ < 1.0f)
                    ImGui::TextDisabled("Terrain still streaming in the background...");
                else
                    ImGui::TextDisabled("Background world content is ready.");
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
                ImGui::TextWrapped(
                    "Gameplay placeholder. The menu flow has handed off to a live session.");
                ImGui::Text("Shared camera active for %d player(s).", active_player_count_);
                if (ImGui::Button("Return To Menu"))
                    transition_to(SessionFlowState::MainMenu);
                break;
        }

        ImGui::End();
    }

    void BlazterGame::publish_overlay_view(int windowWidth, int windowHeight)
    {
        // For now this publishes a safe identity view so editor overlays and
        // debug UI still have valid dimensions. Later this should publish the
        // active gameplay/editor camera matrices used by Blazter.
        if (!ctx_ || !ctx_->overlay_view_state)
            return;
        if (windowWidth <= 0 || windowHeight <= 0)
            return;

        auto& overlay = *ctx_->overlay_view_state;
        overlay.view = glm::mat4(1.0f);
        overlay.proj = glm::mat4(1.0f);
        overlay.viewport = glm::mat4(1.0f);
        overlay.window_size = glm::ivec2(windowWidth, windowHeight);
        overlay.valid = true;
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
        reset_session_flow();
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
        background_world_load_progress_ = 0.0f;
        session_load_progress_ = 0.0f;
        selected_player_count_ = 1;
        active_player_count_ = 0;
        start_requested_ = false;
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
        boot_load_progress_ = std::min(1.0f, boot_load_progress_ + std::max(0.0f, deltaTime_s) * 1.35f);
        background_world_load_progress_ = std::min(
            1.0f,
            background_world_load_progress_ + std::max(0.0f, deltaTime_s) * 0.35f);

        if (boot_load_progress_ >= 1.0f)
            transition_to(SessionFlowState::MainMenu);
    }

    void BlazterGame::update_main_menu(float deltaTime_s)
    {
        background_world_load_progress_ = std::min(
            1.0f,
            background_world_load_progress_ + std::max(0.0f, deltaTime_s) * 0.20f);

        if (start_requested_)
            begin_session_start();
    }

    void BlazterGame::update_session_loading(float deltaTime_s)
    {
        const float target_progress = std::max(background_world_load_progress_, session_load_progress_);
        background_world_load_progress_ = std::min(
            1.0f,
            background_world_load_progress_ + std::max(0.0f, deltaTime_s) * 0.65f);
        session_load_progress_ = std::min(
            1.0f,
            std::max(target_progress, session_load_progress_ + std::max(0.0f, deltaTime_s) * 0.90f));

        if (session_load_progress_ >= 1.0f && background_world_load_progress_ >= 1.0f)
            finish_session_start();
    }

    void BlazterGame::begin_session_start()
    {
        start_requested_ = false;
        session_load_progress_ = std::max(session_load_progress_, background_world_load_progress_ * 0.5f);
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
} // namespace eeng::blazter
