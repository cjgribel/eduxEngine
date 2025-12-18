// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "EngineContext.hpp"
#include "InspectorState.hpp"
#include "ecs/EntityManager.hpp"
#include "editor/EntityPickerPopup.hpp"

#include "entt/entt.hpp"

#if 1
namespace eeng::editor
{
    inline bool inspect_EntityRef(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        auto ptr = any.try_cast<ecs::EntityRef>();
        assert(ptr && "inspect_EntityRef: could not cast meta_any to EntityRef");

        auto* em = dynamic_cast<eeng::EntityManager*>(ctx.entity_manager.get());
        assert(em && "inspect_EntityRef: EngineContext.entity_manager is not an EntityManager");

        bool modified = false;

        // --- State indicator (bound/unbound/resolved) ------------------------
        {
            bool resolved = false;

            if (ptr->guid.valid())
            {
                auto e_opt = em->get_entity_from_guid(ptr->guid);
                if (e_opt && e_opt->valid() && em->entity_valid(*e_opt))
                {
                    resolved = true;
                }
            }
            else if (ptr->entity.valid() && em->entity_valid(ptr->entity))
            {
                resolved = true;
            }

            const ImVec4 state_color =
                resolved ? ImVec4(0.2f, 0.9f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, state_color);
            ImGui::TextUnformatted(resolved ? "Bound" : "Unbound");
            ImGui::PopStyleColor();
        }

        // --- GUID row --------------------------------------------------------
        inspector.begin_leaf("GUID");
        {
            auto& guid = ptr->guid;
            ImGui::TextUnformatted(guid.valid() ? guid.to_string().c_str() : "n/a");
        }
        inspector.end_leaf();

        // --- Live ID row -----------------------------------------------------
        inspector.begin_leaf("Live ID");
        {
            const auto e = ptr->entity;
            ImGui::TextUnformatted((e.valid() && em->entity_valid(e)) ? std::to_string(e.to_integral()).c_str() : "n/a");
        }
        inspector.end_leaf();

        // --- Picker row ------------------------------------------------------
        inspector.begin_leaf("Entity");
        {
            std::string current_label = detail::make_entity_label(*em, *ptr);

            std::string popup_id = "entity_picker##";
            popup_id += ptr->guid.to_string();

            if (ImGui::Button(current_label.c_str()))
            {
                ImGui::OpenPopup(popup_id.c_str());
            }

            if (entity_picker_popup(popup_id.c_str(), *ptr, *em))
            {
                modified = true;
            }
        }
        inspector.end_leaf();

        return modified;
    }
}
#else
namespace eeng::editor
{
    // + ctx
    bool inspect_EntityRef(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        // Check const?

        auto ptr = any.try_cast<ecs::EntityRef>();
        assert(ptr && "inspect_EntityRef: could not cast meta_any to EntityRef");

        inspector.begin_leaf("GUID");

        auto& guid = ptr->guid;
        ImGui::TextUnformatted(guid.valid() ? guid.to_string().c_str() : "n/a");

        //bool guid_modified = editor::inspect_type(guid, inspector);
        inspector.end_leaf();

        inspector.begin_leaf("Live ID");
        auto entity = ptr->entity;
        ImGui::TextUnformatted(entity.valid() ? std::to_string(entity.to_integral()).c_str() : "n/a");
        inspector.end_leaf();

        // auto ptr = any.try_cast<Guid>();
        // assert(ptr && "serialize_Guid: could not cast meta_any to Guid");
        // j = ptr->raw();

        return false;
    }
} // namespace
#endif