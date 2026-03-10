#pragma once

#include "ecs/systems/FluidFrameSystem.hpp"
#include "engineapi/IGameRuntime.hpp"
#include "ecs/RuntimePipeline.hpp"
#include "EngineContext.hpp"
#include <memory>

namespace eeng::fluid_sandbox
{
    // FluidSandboxGame is a project-local runtime for fluid experiments.
    class FluidSandboxGame : public IGameRuntime
    {
    public:
        explicit FluidSandboxGame(std::shared_ptr<EngineContext> ctx);

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
        eeng::fluid_sandbox::ecs::systems::FluidFrameSystem fluid_frame_system_;
    };
}
