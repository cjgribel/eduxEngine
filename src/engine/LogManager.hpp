// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "ILogManager.hpp"
#include <memory>
#include <mutex>

namespace eeng
{
    class LogManager : public ILogManager
    {
    public:
        LogManager();
        ~LogManager();

        void log(const char* fmt, ...) override;
        void log(const ILogManager::LogColor& color, const char* fmt, ...) override;
        void clear() override;
        void draw_gui_widget(const char* label, bool* p_open = nullptr);

    private:
        struct Widget;
        std::unique_ptr<Widget> widget_ptr;
        mutable std::mutex mutex_;
    };
}
