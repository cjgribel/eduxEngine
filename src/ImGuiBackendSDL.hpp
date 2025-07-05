// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

struct SDL_Window;
union SDL_Event;
using SDL_GLContext = void*;

namespace eeng::imgui_backend
{
    bool init(SDL_Window* window, SDL_GLContext context);
    void shutdown();
    void process_event(const SDL_Event* event);
    void begin_frame();
    void end_frame();

    bool want_capture_keyboard();
    bool want_capture_mouse();
    void show_demo_window();
}
