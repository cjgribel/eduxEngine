// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IGuiManager.hpp"
#include "EngineContext.hpp"
#include <unordered_map>

namespace eeng
{
    class GuiManager : public IGuiManager
    {
    public:

        void init() override;
        void release() override;

        void set_flag(GuiFlags flag, bool enabled) override;
        bool is_flag_enabled(GuiFlags flag) const override;

        void draw(EngineContext& ctx) const override;

    private:

        // GUI windows
        void draw_profiler(EngineContext& ctx) const;
        void draw_storage(EngineContext& ctx) const;

        void draw_resource_browser(EngineContext& ctx) const;

        void draw_batch_registry(EngineContext& ctx) const;
        void draw_task_monitor(EngineContext& ctx) const;
        void draw_command_queue(EngineContext& ctx) const;
        void draw_rig_spawner(EngineContext& ctx) const;
        void draw_dockspace(EngineContext& ctx) const;

        void draw_scene_graph(EngineContext& ctx) const;
        void draw_animation_graph_visualizer(EngineContext& ctx) const;
        void draw_editor_controls(EngineContext& ctx) const;
        void draw_main_menu(EngineContext& ctx) const;

        std::unordered_map<GuiFlags, bool> flags;
    };

} // namespace eeng
