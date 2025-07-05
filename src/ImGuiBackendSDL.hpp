// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

struct SDL_Window;
union SDL_Event;
using SDL_GLContext = void*;

namespace eeng
{
    bool imgui_backend_init(SDL_Window* window, SDL_GLContext context);
    void imgui_backend_shutdown();
    void imgui_backend_process_event(const SDL_Event* event);
    void imgui_backend_begin_frame();
    void imgui_backend_end_frame();
}
