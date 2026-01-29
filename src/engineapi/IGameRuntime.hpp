// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "engineapi/EditorViewState.hpp"
#include "engineapi/PlayModePolicy.hpp"
#include <string>
#include <vector>

namespace eeng
{
    struct EngineContext;

    /**
     * @brief Game runtime contract (engine plugin).
     *
     * IGameRuntime contains game logic and rendering only. It is *not* run directly
     * by the Engine; instead, an IApp (GameApp or EditorApp) hosts it and decides
     * the effective play policy and mode transitions.
     */
    class IGameRuntime
    {
    public:
        virtual ~IGameRuntime() noexcept = default;

        /// @brief Initialize game runtime state.
        virtual bool init() = 0;

        /// @brief Per-frame update (mode-agnostic default).
        virtual void update(float time_s, float deltaTime_s) = 0;

        /// @brief Per-frame render (mode-agnostic default).
        virtual void render(float time_s, int windowWidth, int windowHeight) = 0;

        /// @brief Release runtime resources.
        virtual void destroy() = 0;

        virtual void update_edit(float time_s, float deltaTime_s)
        {
            update(time_s, deltaTime_s);
        }

        virtual void update_play(float time_s, float deltaTime_s)
        {
            update(time_s, deltaTime_s);
        }

        virtual void render_edit(float time_s, int windowWidth, int windowHeight)
        {
            render(time_s, windowWidth, windowHeight);
        }

        virtual void render_play(float time_s, int windowWidth, int windowHeight)
        {
            render(time_s, windowWidth, windowHeight);
        }

        /// @brief Return the preferred play policy for this runtime (app may override).
        virtual PlayModePolicy preferred_play_policy() const
        {
            return PlayModePolicy::Preview;
        }

        /// @brief Return preferred startup batches for Strict play (app may override).
        virtual std::vector<std::string> preferred_startup_batches() const
        {
            return {};
        }

        virtual void on_play_world_created(EngineContext&)
        {
        }

        virtual void on_enter_play(EngineContext&)
        {
        }

        virtual void on_exit_play(EngineContext&)
        {
        }

        /// @brief Provide the current editor view state (for gizmos/picking).
        virtual bool get_editor_view(EditorViewState&) const
        {
            return false;
        }
    };
} // namespace eeng
