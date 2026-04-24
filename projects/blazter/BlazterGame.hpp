#pragma once

#include "EngineContext.hpp"
#include "ecs/RuntimePipeline.hpp"
#include "engineapi/IGameRuntime.hpp"
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <string_view>
#include <memory>
#include <unordered_set>
#include <vector>

namespace eeng::blazter
{
    class BlazterGame : public IGameRuntime
    {
    public:
        explicit BlazterGame(std::shared_ptr<EngineContext> ctx);

        bool init() override;
        void update(float time_s, float deltaTime_s) override;
        void update_edit(float time_s, float deltaTime_s) override;
        void update_play(float time_s, float deltaTime_s) override;
        void render(float time_s, int windowWidth, int windowHeight) override;
        void render_scene(const RenderContext& ctx) override;
        void render_overlay(const RenderContext& ctx) override;
        void render_gui(const RenderContext& ctx) override;
        void destroy() override;
        PlayModePolicy preferred_play_policy() const override;
        std::vector<std::string> preferred_startup_batches() const override;
        void on_play_world_created(EngineContext& ctx) override;
        void on_enter_play(EngineContext& ctx) override;
        void on_exit_play(EngineContext& ctx) override;

    private:
        enum class SessionFlowState
        {
            BootLoading,
            MainMenu,
            SessionLoading,
            Playing,
        };

        void reset_session_flow();
        void transition_to(SessionFlowState next_state);
        void update_flow(float deltaTime_s);
        void update_boot_loading(float deltaTime_s);
        void update_main_menu(float deltaTime_s);
        void update_session_loading(float deltaTime_s);
        PlayModePolicy active_play_policy() const;
        bool should_run_game_boot() const;
        void reset_game_camera();
        void queue_level_preload();
        float level_preload_progress() const;
        bool is_level_preload_ready() const;
        void begin_session_start();
        void finish_session_start();
        std::string_view flow_state_label() const;
        bool build_play_view(OverlayViewState& out, glm::ivec2 window_size) const;
        bool build_view_for_mode(OverlayViewState& out, RenderMode mode, glm::ivec2 window_size) const;
        void publish_play_overlay_view(int windowWidth, int windowHeight);

        struct GameCamera
        {
            glm::vec3 position{ 0.0f, 7.5f, 22.0f };
            float yaw = -1.5707963f;
            float pitch = -0.28f;
            float near_plane = 0.1f;
            float far_plane = 500.0f;
            float fov_y_degrees = 60.0f;
        };

        std::shared_ptr<EngineContext> ctx_;
        eeng::ecs::RuntimePipeline runtime_pipeline_;
        SessionFlowState flow_state_{ SessionFlowState::BootLoading };
        float flow_state_elapsed_s_{ 0.0f };
        float boot_load_progress_{ 0.0f };
        float session_load_progress_{ 0.0f };
        int selected_player_count_{ 1 };
        int active_player_count_{ 0 };
        bool start_requested_{ false };
        glm::ivec2 last_window_size_{ 0, 0 };
        std::unordered_set<std::string> queued_level_batches_;
        GameCamera game_camera_{};
    };
} // namespace eeng::blazter
