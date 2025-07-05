// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "LogManager.hpp"
#include <cstdarg>
#include <cstdio>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "imgui.h"

namespace
{
    std::string format_string(const char* fmt, va_list args)
    {
        va_list args_copy;
        va_copy(args_copy, args);
        int length = vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);

        std::string buffer(length, '\0');
        vsnprintf(buffer.data(), length + 1, fmt, args);
        return buffer;
    }

    std::string relative_time_string()
    {
        using namespace std::chrono;
        static auto start = steady_clock::now();
        auto now = steady_clock::now();
        auto elapsed = duration_cast<milliseconds>(now - start);

        int seconds = static_cast<int>(elapsed.count() / 1000);
        int millis = static_cast<int>(elapsed.count() % 1000);

        std::ostringstream oss;
        oss << "[+" << seconds << '.' << std::setw(3) << std::setfill('0') << millis << ']';
        return oss.str();
    }

    std::string current_time_string()
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto now_time_t = system_clock::to_time_t(now);
        auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm;
#if defined(_WIN32)
        localtime_s(&tm, &now_time_t);
#else
        localtime_r(&now_time_t, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "[%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << now_ms.count()
            << ']';

        return oss.str();
    }
}

namespace eeng
{
    struct LogManager::Widget
    {
        ImGuiTextBuffer buf;
        ImGuiTextFilter filter;
        ImVector<int> line_offsets;
        bool auto_scroll = true;
        bool scroll_to_bottom = false;

        Widget()
        {
            clear();
        }

        void clear()
        {
            buf.clear();
            line_offsets.clear();
            line_offsets.push_back(0);
        }

        void add_log(const std::string& text)
        {
            int old_size = buf.size();
            buf.appendf("%s", text.c_str());

            for (int i = old_size; i < buf.size(); ++i)
                if (buf[i] == '\n')
                    line_offsets.push_back(i + 1);

            if (auto_scroll)
                scroll_to_bottom = true;
        }

        void draw(const char* title, bool* p_open)
        {
            if (!ImGui::Begin(title, p_open))
            {
                ImGui::End();
                return;
            }

            if (ImGui::BeginPopup("Options"))
            {
                ImGui::Checkbox("Auto-scroll", &auto_scroll);
                ImGui::EndPopup();
            }

            if (ImGui::Button("Options"))
                ImGui::OpenPopup("Options");

            ImGui::SameLine();
            bool clear_clicked = ImGui::Button("Clear");

            ImGui::SameLine();
            bool copy_clicked = ImGui::Button("Copy");

            ImGui::SameLine();
            filter.Draw("Filter", -100.0f);

            ImGui::Separator();
            ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            if (clear_clicked)
                clear();
            if (copy_clicked)
                ImGui::LogToClipboard();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            const char* buf_begin = buf.begin();
            const char* buf_end = buf.end();
            if (filter.IsActive())
            {
                for (int line_no = 0; line_no < line_offsets.Size; line_no++)
                {
                    const char* line_start = buf_begin + line_offsets[line_no];
                    const char* line_end = (line_no + 1 < line_offsets.Size)
                        ? (buf_begin + line_offsets[line_no + 1] - 1)
                        : buf_end;
                    if (filter.PassFilter(line_start, line_end))
                        ImGui::TextUnformatted(line_start, line_end);
                }
            }
            else
            {
                ImGuiListClipper clipper;
                clipper.Begin(line_offsets.Size);
                while (clipper.Step())
                {
                    for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                    {
                        const char* line_start = buf_begin + line_offsets[line_no];
                        const char* line_end = (line_no + 1 < line_offsets.Size)
                            ? (buf_begin + line_offsets[line_no + 1] - 1)
                            : buf_end;
                        ImGui::TextUnformatted(line_start, line_end);
                    }
                }
                clipper.End();
            }
            ImGui::PopStyleVar();

            if (scroll_to_bottom)
                ImGui::SetScrollHereY(1.0f);
            scroll_to_bottom = false;

            ImGui::EndChild();
            ImGui::End();
        }
    };

    // ---- LogManager public interface ----

    LogManager::LogManager()
        : widget_ptr(std::make_unique<Widget>())
    {
    }

    LogManager::~LogManager() = default;

    void LogManager::log(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        std::string formatted = format_string(fmt, args);
        va_end(args);

        // Frame counter prefix
            //std::string with_prefix = "[frame#" + std::to_string(ImGui::GetFrameCount()) + "] " + formatted + "\n";
        // Wall clock time stamp
            //std::string with_prefix = current_time_string() + " " + formatted + "\n";
        // Relative time stamp
        std::string with_prefix = relative_time_string() + " " + formatted + "\n";

        widget_ptr->add_log(with_prefix);
    }

    void LogManager::clear()
    {
        widget_ptr->clear();
    }

    void LogManager::draw_gui_widget(const char* label, bool* p_open)
    {
        widget_ptr->draw(label, p_open);
    }
}
