#pragma once

#include "GameBase.h"
#include "EngineContext.hpp"
#include <memory>

namespace eeng::project2
{
    // ReferenceGame is a minimal stub kept in sync with the evolving engine API.
    class ReferenceGame : public GameBase
    {
    public:
        explicit ReferenceGame(std::shared_ptr<EngineContext> ctx);

        bool init() override;
        void update(float time_s, float deltaTime_s) override;
        void render(float time_s, int windowWidth, int windowHeight) override;
        void destroy() override;
        PlayModePolicy play_mode_policy() const override;
        std::vector<std::string> play_startup_batches() const override;
        void on_play_world_created(EngineContext& ctx) override;
        void on_enter_play(EngineContext& ctx) override;
        void on_exit_play(EngineContext& ctx) override;

    private:
        std::shared_ptr<EngineContext> ctx_;
    };
}
