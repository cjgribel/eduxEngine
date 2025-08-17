// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "config.h"
#include "GameBase.h"
#include "EngineContext.hpp"
#include <iostream>
#include <memory>

struct SDL_Window;              // Forward declaration
typedef void* SDL_GLContext;    // Forward declaration

namespace eeng
{
    /**
     * @brief Main engine class handling SDL, OpenGL, ImGui initialization and the main loop.
     */
    class Engine
    {
    public:
        /** Constructor */
        explicit Engine(std::shared_ptr<EngineContext> ctx);

        /** Destructor, calls shutdown() */
        ~Engine();

        /**
         * @brief Initialize the engine.
         * @param title Window title
         * @param width Window width
         * @param height Window height
         * @return True if successful, false otherwise
         */
        bool init(const char* title, int width, int height);

        /// @brief Create a game of a given type and run main loop
        /// @tparam TGame Game type
        /// @tparam ...Args 
        /// @param ...args 
        template<typename TGame, typename... Args>
            requires (std::is_base_of<GameBase, TGame>::value)
        void run(Args&&... args)
        {
            if constexpr (requires { TGame(ctx); })
                run(std::make_unique<TGame>(ctx, std::forward<Args>(args)...));
            else
                run(std::make_unique<TGame>(std::forward<Args>(args)...));
        }

        /** @brief Clean up and close the engine. */
        void shutdown();

        /**
         * @brief Get SDL window pointer.
         * @return SDL_Window pointer
         */
        SDL_Window* window() const { return window_; }

    private:
        SDL_Window* window_ = nullptr;        ///< SDL Window pointer
        SDL_GLContext gl_context_ = nullptr;  ///< OpenGL context

        int window_height;    ///< Window height in pixels
        int window_width;     ///< Window width in pixels
        bool vsync;   ///< V-sync enabled state
        bool wireframe_mode; ///< Wireframe rendering state
        float min_frametime_ms; ///< Minimum frame duration in milliseconds (default 60 FPS)

        // EngineContext ctx;
        std::shared_ptr<EngineContext> ctx;

        /**
         * @brief Start the main loop.
         * @param game Unique pointer to the initial game game
         */
        void run(std::unique_ptr<GameBase> game);

        /** Initialize SDL library and window. */
        bool init_sdl(const char* title, int width, int height);

        /** Initialize OpenGL context. */
        bool init_opengl();

        /** Initialize ImGui for GUI rendering. */
        bool init_imgui();

        /** Handle SDL events. */
        void process_events(bool& running);

        /** Prepare frame rendering. */
        void begin_frame();

        /** Finish frame rendering. */
        void end_frame();

        void on_set_vsync(const SetVsyncEvent& e);
        void on_set_wireframe(const SetWireFrameRenderingEvent& e);
        void on_set_min_frametime(const SetMinFrameTimeEvent& e);
        void on_resource_task_completed(const ResourceTaskCompletedEvent& e);

    };

    using EnginePtr = std::unique_ptr<Engine>;
#if 0
    struct EngineFactory
    {
        static EnginePtr CreateDefaultEngine()
        {
            return std::make_unique<Engine>(
                std::make_unique<EntityManager>(),
                std::make_unique<ResourceManager>()
                // std::make_unique<SceneManager>(),
                // std::make_unique<EventDispatcher>(),
                // std::make_unique<Logger>());
        }
    }
#endif

} // namespace eeng

#endif // ENGINE_HPP