// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef StickyNoteComponent_hpp
#define StickyNoteComponent_hpp

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

/*
Adding a note to a StickyNoteComponent:

if (auto* note = registry.try_get<eeng::ecs::StickyNoteComponent>(entity))
{
    eeng::ecs::StickyNoteComponent_Append(*note, "Collision enter");
    eeng::ecs::StickyNoteComponent_AppendStack(*note, "Collision stay");
}
*/

namespace eeng::ecs
{
    struct StickyNoteComponent
    {
        static constexpr std::size_t MaxLines = 7;
        static constexpr std::size_t MaxLineLength = 128;

        struct Line
        {
            std::array<char, MaxLineLength> text{};
            std::uint8_t length = 0;
            float age = 0.0f;

            std::string_view view() const
            {
                return std::string_view(text.data(), length);
            }

            bool ends_with(char c) const
            {
                return length > 0 && text[length - 1] == c;
            }

            void set(std::string_view value)
            {
                const std::size_t copy_len = std::min(value.size(), MaxLineLength - 1);
                if (copy_len > 0)
                    std::memcpy(text.data(), value.data(), copy_len);
                text[copy_len] = '\0';
                length = static_cast<std::uint8_t>(copy_len);
            }

            void append_char(char c)
            {
                if (length + 1 < MaxLineLength)
                {
                    text[length++] = c;
                    text[length] = '\0';
                }
                else if (length > 0)
                {
                    text[length - 1] = c;
                }
            }

            void clear()
            {
                if (!text.empty())
                    text[0] = '\0';
                length = 0;
                age = 0.0f;
            }
        };

        std::array<Line, MaxLines> lines{};
        std::uint8_t head = 0;
        std::uint8_t count = 0;

        float max_age = 2.0f;
        bool enabled = true;

        glm::vec3 world_offset{ 0.0f };
        std::uint32_t color_bg = 0x80000000u;
        std::uint32_t color_text = 0xffffffffu;
    };

    std::string to_string(const StickyNoteComponent& note);

    template<typename Visitor>
    void visit_asset_refs(StickyNoteComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(StickyNoteComponent&, Visitor&&) {}

    void StickyNoteComponent_Clear(StickyNoteComponent& note);
    void StickyNoteComponent_Append(StickyNoteComponent& note, std::string_view text);
    void StickyNoteComponent_AppendStack(StickyNoteComponent& note, std::string_view text);
    std::string StickyNoteComponent_Dump(const StickyNoteComponent& note);
    void StickyNoteComponent_Update(StickyNoteComponent& note, float dt);
}

#endif // StickyNoteComponent_hpp
