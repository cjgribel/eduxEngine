#pragma once

#include "engineapi/IGameRuntime.hpp"
#include "EngineContext.hpp"
#include <memory>

namespace eeng::project2
{
    // ReferenceGame is a minimal stub kept in sync with the evolving engine API.
    class ReferenceGame : public IGameRuntime
    {
    public:
        explicit ReferenceGame(std::shared_ptr<EngineContext> ctx);

        bool init() override;
        void update(float time_s, float deltaTime_s) override;
        void render(float time_s, int windowWidth, int windowHeight) override;
        void destroy() override;
        PlayModePolicy preferred_play_policy() const override;
        std::vector<std::string> preferred_startup_batches() const override;
        void on_play_world_created(EngineContext& ctx) override;
        void on_enter_play(EngineContext& ctx) override;
        void on_exit_play(EngineContext& ctx) override;

    private:
        std::shared_ptr<EngineContext> ctx_;
    };
}
