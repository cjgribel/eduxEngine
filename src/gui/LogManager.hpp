// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "ILogManager.hpp"
#include <memory>

namespace eeng
{
    class LogManager : public ILogManager
    {
    public:
        LogManager();
        ~LogManager();

        void log(const char* fmt, ...) override;
        void clear() override;

    private:
        void draw_gui_widget(const char* label, bool* p_open = nullptr);
        friend class GuiManager; // draw_gui_widget is called from here

        struct Widget;
        std::unique_ptr<Widget> widget_ptr;
    };
}
