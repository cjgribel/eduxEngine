// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ImGuiBackendSDL.hpp"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

namespace eeng::imgui_backend
{
    bool init(SDL_Window* window, SDL_GLContext context)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        return ImGui_ImplSDL2_InitForOpenGL(window, context)
            && ImGui_ImplOpenGL3_Init("#version 410 core");
    }

    void shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void process_event(const SDL_Event* event)
    {
        ImGui_ImplSDL2_ProcessEvent(event);
    }

    void begin_frame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void end_frame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    bool want_capture_keyboard()
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    bool want_capture_mouse()
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    void show_demo_window()
    {
        ImGui::ShowDemoWindow();
    }
}
