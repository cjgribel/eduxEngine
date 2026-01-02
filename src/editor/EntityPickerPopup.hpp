// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/EntityManager.hpp"
#include "Guid.h"

#include <algorithm>
#include <string>
#include <vector>

#include "imgui.h"

namespace eeng::editor
{
    namespace detail
    {
        inline std::string make_entity_label(EntityManager& em, const ecs::EntityRef& ref)
        {
            // Prefer: if GUID resolves, show the resolved entity's name.
            if (ref.guid.valid())
            {
                auto e_opt = em.get_entity_from_guid(ref.guid);
                if (e_opt && e_opt->has_id() && em.entity_valid(*e_opt))
                {
                    const auto& h = em.get_entity_header(*e_opt);
                    if (!h.name.empty())
                    {
                        return h.name;
                    }
                    return ref.guid.to_string();
                }

                return ref.guid.to_string();
            }

            // Otherwise: if entity handle is valid, show its name.
            if (ref.entity.has_id() && em.entity_valid(ref.entity))
            {
                const auto& h = em.get_entity_header(ref.entity);
                if (!h.name.empty())
                {
                    return h.name;
                }
                return std::to_string(ref.entity.to_integral());
            }

            return std::string{ "n/a" };
        }

        struct EntityPickerItem
        {
            ecs::Entity entity{};
            Guid guid{};
            std::string name;
        };

        inline std::string make_item_row_text(const EntityPickerItem& it)
        {
            std::string s;
            s.reserve(it.name.size() + 96);

            s += it.name.empty() ? std::string{ "<unnamed>" } : it.name;
            s += "  (id ";
            s += std::to_string(it.entity.to_integral());
            s += ")  [";
            s += it.guid.valid() ? it.guid.to_string() : std::string{ "n/a" };
            s += "]";

            return s;
        }
    }

    // Popup: returns true if ref was modified
    inline bool entity_picker_popup(
        const char* popup_id,
        ecs::EntityRef& ref,
        EntityManager& em)
    {
        bool modified = false;

        if (!ImGui::BeginPopup(popup_id))
        {
            return false;
        }

        // --- Actions ---------------------------------------------------------
        if (ImGui::Button("Clear"))
        {
            ref.guid = {};
            ref.entity = {};
            modified = true;
        }

        ImGui::Separator();

        // --- Filter ----------------------------------------------------------
        static ImGuiTextFilter filter;
        filter.Draw("Filter");

        ImGui::Separator();

        // --- Gather items (sorted by name) -----------------------------------
        std::vector<detail::EntityPickerItem> items;
        items.reserve(em.registered_entity_count());

        em.for_each_registered_entity(
            [&](const ecs::Entity& e, const Guid& g)
            {
                detail::EntityPickerItem it;
                it.entity = e;
                it.guid = g;

                // Name is optional; if HeaderComponent always exists, this is safe.
                // If header is not present, replace with try-get logic.
                const auto& h = em.get_entity_header(e);
                it.name = h.name;

                items.push_back(std::move(it));
            });

        std::sort(
            items.begin(),
            items.end(),
            [](const detail::EntityPickerItem& a, const detail::EntityPickerItem& b)
            {
                if (a.name != b.name) return a.name < b.name;
                return a.entity.to_integral() < b.entity.to_integral();
            });

        // --- List ------------------------------------------------------------
        ImGui::BeginChild("entity_picker_list", ImVec2(520.0f, 360.0f), true);

        for (const auto& it : items)
        {
            const std::string row = detail::make_item_row_text(it);

            if (!filter.PassFilter(row.c_str()))
            {
                continue;
            }

            const bool is_selected =
                (ref.entity.has_id() && em.entity_valid(ref.entity) && ref.entity == it.entity);

            if (ImGui::Selectable(row.c_str(), is_selected))
            {
                ref.guid = it.guid;
                ref.entity = it.entity;

                modified = true;
                ImGui::CloseCurrentPopup();
                break;
            }
        }

        ImGui::EndChild();
        ImGui::EndPopup();

        return modified;
    }
}
