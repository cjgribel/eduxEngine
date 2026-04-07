// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "engineapi/PlayModePolicy.hpp"
#include <string>
#include <vector>

namespace eeng
{
    struct EngineContext;

    /**
     * @brief Engine-facing application contract.
     *
     * The Engine runs exactly one IApp at a time. An IApp is responsible for:
     * - Driving update/render for both Edit and Play modes.
     * - Deciding the effective Play/Edit policy (Preview vs Strict).
     * - Providing startup batches for Strict play and reacting to mode changes.
     *
     * EditorApp and GameApp are the two primary implementations:
     * - EditorApp hosts editor tooling + a game runtime.
     * - GameApp hosts only a game runtime.
     */
    class IApp
    {
    public:
        virtual ~IApp() noexcept = default;

        /// @brief Initialize the app and any owned runtimes/systems.
        virtual bool init() = 0;

        /// @brief Per-frame update while the engine is running.
        virtual void update(float time_s, float deltaTime_s) = 0;

        /// @brief Per-frame render while the engine is running.
        virtual void render(float time_s, int windowWidth, int windowHeight) = 0;

        /// @brief Shutdown and release app resources.
        virtual void destroy() = 0;

        /// @brief Update while in edit mode (defaults to update()).
        virtual void update_edit(float time_s, float deltaTime_s)
        {
            update(time_s, deltaTime_s);
        }

        /// @brief Update while in play mode (defaults to update()).
        virtual void update_play(float time_s, float deltaTime_s)
        {
            update(time_s, deltaTime_s);
        }

        /// @brief Render while in edit mode (defaults to render()).
        virtual void render_edit(float time_s, int windowWidth, int windowHeight)
        {
            render(time_s, windowWidth, windowHeight);
        }

        /// @brief Render while in play mode (defaults to render()).
        virtual void render_play(float time_s, int windowWidth, int windowHeight)
        {
            render(time_s, windowWidth, windowHeight);
        }

        /// @brief Return the effective play mode policy for this app.
        virtual PlayModePolicy play_policy() const
        {
            return PlayModePolicy::Preview;
        }

        /// @brief Return batch names to load when entering Strict play mode.
        virtual std::vector<std::string> play_startup_batches() const
        {
            return {};
        }

        /// @brief Configure the play world before loading batches (Strict mode hook).
        virtual void on_play_world_created(EngineContext&)
        {
        }

        /// @brief Called after entering play mode (after any Strict-mode loads).
        virtual void on_enter_play(EngineContext&)
        {
        }

        /// @brief Called before leaving play mode.
        virtual void on_exit_play(EngineContext&)
        {
        }
    };
} // namespace eeng
