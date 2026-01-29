// ReferenceGame is a minimal stub kept in sync with the evolving engine API.
// It demonstrates Strict play-mode hooks without depending on any gameplay code.

#include "Module1/project2/ReferenceGame.hpp"
#include "BatchRegistry.hpp"

namespace eeng::project2
{
    ReferenceGame::ReferenceGame(std::shared_ptr<EngineContext> ctx)
        : ctx_(std::move(ctx))
    {
    }

    bool ReferenceGame::init()
    {
        if (ctx_)
            runtime_pipeline_.init(*ctx_);
        return true;
    }

    void ReferenceGame::update(float time_s, float deltaTime_s)
    {
        (void)time_s;
        update_edit(time_s, deltaTime_s);
    }

    void ReferenceGame::update_edit(float time_s, float deltaTime_s)
    {
        (void)time_s;
        if (ctx_)
            runtime_pipeline_.update_edit(*ctx_, deltaTime_s);
    }

    void ReferenceGame::update_play(float time_s, float deltaTime_s)
    {
        (void)time_s;
        if (ctx_)
            runtime_pipeline_.update_play(*ctx_, deltaTime_s);
    }

    void ReferenceGame::render(float time_s, int windowWidth, int windowHeight)
    {
        (void)time_s;
        (void)windowWidth;
        (void)windowHeight;
    }

    void ReferenceGame::destroy()
    {
        runtime_pipeline_.shutdown();
    }

    PlayModePolicy ReferenceGame::preferred_play_policy() const
    {
        // Prefer Preview to reuse the current edit-world state, or Strict to request
        // explicit batch loading each time play starts (the app can override).
        return PlayModePolicy::Strict;
    }

    std::vector<std::string> ReferenceGame::preferred_startup_batches() const
    {
        // In Strict mode, list preferred batch names to load at play start.
        // These are resolved through the play world's BatchRegistry by the app.
        return { std::string(BatchRegistry::kDefaultBatchName) };
    }

    void ReferenceGame::on_play_world_created(EngineContext& ctx)
    {
        // Configure the fresh play world before loading batches (e.g., set the batch index path).
        if (!ctx.batch_registry)
            return;

        auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);
        br.load_or_create_index("Module1/project2/batches/index.json");
    }

    void ReferenceGame::on_enter_play(EngineContext& ctx)
    {
        // Runtime-only setup goes here (spawn player, reset timers, etc).
        (void)ctx;
    }

    void ReferenceGame::on_exit_play(EngineContext& ctx)
    {
        // Cleanup runtime-only state before returning to edit mode.
        (void)ctx;
    }
}
