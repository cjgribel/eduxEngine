// ReferenceGame is a minimal stub kept in sync with the evolving engine API.

#include "Module1/project2/ReferenceGame.hpp"

namespace eeng::project2
{
    ReferenceGame::ReferenceGame(std::shared_ptr<EngineContext> ctx)
        : ctx_(std::move(ctx))
    {
    }

    bool ReferenceGame::init()
    {
        return true;
    }

    void ReferenceGame::update(float time_s, float deltaTime_s)
    {
        (void)time_s;
        (void)deltaTime_s;
    }

    void ReferenceGame::render(float time_s, int windowWidth, int windowHeight)
    {
        (void)time_s;
        (void)windowWidth;
        (void)windowHeight;
    }

    void ReferenceGame::destroy()
    {
    }
}
