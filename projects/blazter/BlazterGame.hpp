#pragma once

#include "EngineContext.hpp"
#include "ecs/RuntimePipeline.hpp"
#include "engineapi/IGameRuntime.hpp"
#include <memory>

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
        void publish_overlay_view(int windowWidth, int windowHeight);

        std::shared_ptr<EngineContext> ctx_;
        eeng::ecs::RuntimePipeline runtime_pipeline_;
    };
} // namespace eeng::blazter
