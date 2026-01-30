// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "StickyNoteComponent.hpp"
#include <format>

namespace eeng::ecs
{
    void StickyNoteComponent_Clear(StickyNoteComponent& note)
    {
        note = StickyNoteComponent{};
    }

    void StickyNoteComponent_Append(StickyNoteComponent& note, std::string_view text)
    {
        if (text.empty())
            return;

        const std::size_t write_index =
            (note.head + note.count) % StickyNoteComponent::MaxLines;

        if (note.count == StickyNoteComponent::MaxLines)
        {
            note.head = (note.head + 1) % StickyNoteComponent::MaxLines;
        }
        else
        {
            note.count++;
        }

        auto& line = note.lines[write_index];
        line.set(text);
        line.age = 0.0f;
        note.enabled = true;
    }

    void StickyNoteComponent_AppendStack(StickyNoteComponent& note, std::string_view text)
    {
        if (text.empty())
            return;

        if (note.count == 0)
        {
            StickyNoteComponent_Append(note, text);
            return;
        }

        const std::size_t last_index =
            (note.head + note.count - 1) % StickyNoteComponent::MaxLines;

        auto& last = note.lines[last_index];
        auto last_view = last.view();
        const bool last_stacked = last.ends_with('+');

        if (last_stacked && last_view.size() > 0)
            last_view.remove_suffix(1);

        if (last_view == text)
        {
            if (!last_stacked)
                last.append_char('+');
            last.age = 0.0f;
            note.enabled = true;
            return;
        }

        StickyNoteComponent_Append(note, text);
    }

    std::string StickyNoteComponent_Dump(const StickyNoteComponent& note)
    {
        std::string out;
        if (!note.enabled || note.count == 0)
            return out;

        out.reserve(note.count * (StickyNoteComponent::MaxLineLength + 1));

        for (std::size_t i = 0; i < note.count; ++i)
        {
            const std::size_t index = (note.head + i) % StickyNoteComponent::MaxLines;
            const auto& line = note.lines[index];

            if (line.length == 0)
                continue;

            if (note.max_age > 0.0f && line.age > note.max_age)
                continue;

            out.append(line.text.data(), line.length);
            out.push_back('\n');
        }

        return out;
    }

    void StickyNoteComponent_Update(StickyNoteComponent& note, float dt)
    {
        if (!note.enabled || note.count == 0)
            return;

        for (std::size_t i = 0; i < note.count; ++i)
        {
            const std::size_t index = (note.head + i) % StickyNoteComponent::MaxLines;
            note.lines[index].age += dt;
        }

        if (note.max_age <= 0.0f)
            return;

        while (note.count > 0)
        {
            auto& line = note.lines[note.head];
            if (line.age <= note.max_age)
                break;

            line.clear();
            note.head = (note.head + 1) % StickyNoteComponent::MaxLines;
            note.count--;
        }

        if (note.count == 0)
            note.enabled = false;
    }
}

namespace eeng::ecs
{
    std::string to_string(const StickyNoteComponent& note)
    {
        return std::format(
            "StickyNote(lines = {}, enabled = {})",
            note.count,
            note.enabled ? "true" : "false");
    }
}
