// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include <cstdint>

namespace eeng
{
    class EngineContext;

    enum class GuiFlags : uint8_t
    {
        ShowEngineInfo,
        ShowResourceViewer,
        ShowSceneGraph,
        ShowRenderStats,
        ShowInputDebugger,
        // ...
    };

    class IGuiManager
    {
    public:
        virtual void init() = 0;
        virtual void release() = 0;

        virtual void set_flag(GuiFlags flag, bool enabled) = 0;
        virtual bool is_flag_enabled(GuiFlags flag) const = 0;
        
        virtual void draw_engine_info(EngineContext& ctx) const = 0;
        
        virtual ~IGuiManager() = default;
    };
} // namespace eeng