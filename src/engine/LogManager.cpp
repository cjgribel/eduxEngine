// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "LogManager.hpp"
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
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
        using LogColor = ILogManager::LogColor;

        struct LogEntry
        {
            std::string message;
            std::string display;
            int count = 1;
            LogColor color;
        };

        std::vector<LogEntry> entries;
        ImGuiTextFilter filter;
        bool auto_scroll = true;
        bool scroll_to_bottom = false;

        Widget()
        {
            clear();
        }

        void clear()
        {
            entries.clear();
        }

        static bool same_color(const LogColor& a, const LogColor& b)
        {
            if (a.has_color != b.has_color)
                return false;
            if (!a.has_color)
                return true;
            return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        }

        static std::string format_display_line(const std::string& time_prefix, const std::string& message, int count)
        {
            if (count > 1)
                return time_prefix + " " + message + " (x" + std::to_string(count) + ")";
            return time_prefix + " " + message;
        }

        void add_log(const std::string& message, const LogColor& color)
        {
            std::string time_prefix = relative_time_string();
            if (!entries.empty() && entries.back().message == message && same_color(entries.back().color, color))
            {
                LogEntry& entry = entries.back();
                ++entry.count;
                entry.display = format_display_line(time_prefix, message, entry.count);
            }
            else
            {
                LogEntry entry;
                entry.message = message;
                entry.display = format_display_line(time_prefix, message, 1);
                entry.color = color;
                entries.push_back(std::move(entry));
            }

            if (auto_scroll)
                scroll_to_bottom = true;
        }

        void draw_line(const LogEntry& entry) const
        {
            if (entry.color.has_color)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(entry.color.r, entry.color.g, entry.color.b, entry.color.a));
                ImGui::TextUnformatted(entry.display.c_str());
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::TextUnformatted(entry.display.c_str());
            }
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
            if (filter.IsActive())
            {
                for (const LogEntry& entry : entries)
                {
                    const char* line_start = entry.display.c_str();
                    const char* line_end = line_start + entry.display.size();
                    if (filter.PassFilter(line_start, line_end))
                        draw_line(entry);
                }
            }
            else
            {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(entries.size()));
                while (clipper.Step())
                {
                    for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                    {
                        draw_line(entries[static_cast<size_t>(line_no)]);
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

        {
            std::lock_guard<std::mutex> lk(mutex_);
            widget_ptr->add_log(formatted, ILogManager::LogColor{});
        }
    }

    void LogManager::log(const ILogManager::LogColor& color, const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        std::string formatted = format_string(fmt, args);
        va_end(args);

        {
            std::lock_guard<std::mutex> lk(mutex_);
            widget_ptr->add_log(formatted, color);
        }
    }

    void LogManager::clear()
    {
        std::lock_guard<std::mutex> lk(mutex_);
        widget_ptr->clear();
    }

    void LogManager::draw_gui_widget(const char* label, bool* p_open)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        widget_ptr->draw(label, p_open);
    }
}
