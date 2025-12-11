
#pragma once

#include "EngineContext.hpp"
#include "InspectorState.hpp"
#include "editor/AssetPickerPopup.hpp"
// #include "ecs/Entity.hpp"
#include "AssetRef.hpp"
#include "MetaAux.h"
#include "entt/entt.hpp"

namespace eeng::editor
{
#if 1
    template <class T>
    bool inspect_AssetRef(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        auto ptr = any.try_cast<eeng::AssetRef<T>>();
        assert(ptr && "inspect_AssetRef: could not cast meta_any to AssetRef<T>");

        bool modified = false;

        // --- State indicator (bound/unbound) ---------------------------------
        ImVec4 state_color{};
        state_color = ptr->handle ? ImVec4(0.2f, 0.9f, 0.2f, 1.0f)   // bound
            : ImVec4(1.0f, 0.2f, 0.2f, 1.0f);  // unbound;
        ImGui::PushStyleColor(ImGuiCol_Text, state_color);
        ImGui::TextUnformatted(ptr->handle ? "Bound" : "Unbound");
        ImGui::PopStyleColor();

        // --- GUID row --------------------------------------------------------
        inspector.begin_leaf("GUID");

        auto& guid = ptr->guid;
        ImGui::TextUnformatted(guid.valid() ? guid.to_string().c_str() : "n/a");

        inspector.end_leaf();

        // --- Handle row ------------------------------------------------------
        inspector.begin_leaf("Live handle");
        const auto type_name = get_meta_type_name<T>();
        ImGui::TextDisabled("%s", type_name.c_str());

        auto handle = ptr->handle;
        if (handle)
        {
            ImGui::Text("idx %zu", handle.idx);
            ImGui::Text("ver %u", handle.ver);
        }
        else
        {
            ImGui::TextUnformatted("n/a");
        }
        inspector.end_leaf();

        // --- Picker row ------------------------------------------------------
        inspector.begin_leaf("Asset");

        auto index = ctx.resource_manager->get_index_data();
        std::string current_label = detail::make_asset_ref_label(ptr->guid, index);

        // Stable Guid-based popup id
        std::string popup_id = "asset_picker##";
        popup_id += ptr->guid.to_string();   // still stable for invalid Guid

        if (ImGui::Button(current_label.c_str()))
        {
            ImGui::OpenPopup(popup_id.c_str());
        }
        ImGui::SameLine();
        // ImGui::TextDisabled("click to change");

        // Popup with list of assets of type T
        if (asset_picker_popup<T>(popup_id.c_str(), *ptr, index))
        {
            modified = true;
        }

        inspector.end_leaf();

        return modified;
    }

#else
    template <class T>
    bool inspect_AssetRef(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        auto index = ctx.resource_manager->get_index_data();

        // Check const?

        auto ptr = any.try_cast<eeng::AssetRef<T>>();
        assert(ptr && "inspect_EntityRef: could not cast meta_any to AssetRef");

        // COLORED TEXT TEST
        ImVec4 state_color{};
        state_color = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, state_color);
        ImGui::TextUnformatted("Bound");
        ImGui::PopStyleColor();
        //
        state_color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, state_color);
        ImGui::SameLine();
        ImGui::TextUnformatted("Unbound");
        ImGui::PopStyleColor();


        inspector.begin_leaf("GUID");

        auto& guid = ptr->guid;
        ImGui::TextUnformatted(guid.valid() ? guid.to_string().c_str() : "n/a");

        //bool guid_modified = editor::inspect_type(guid, inspector);
        inspector.end_leaf();

        inspector.begin_leaf("Live handle");
        const auto type_name = get_meta_type_name<T>();
        ImGui::TextDisabled("%s", type_name.c_str());

        auto handle = ptr->handle;
        if (handle)
        {
            ImGui::Text("idx %zu", handle.idx);
            ImGui::Text("ver %u", handle.ver);
        }
        else
        {
            ImGui::TextUnformatted("n/a");
        }
        inspector.end_leaf();

        // auto ptr = any.try_cast<Guid>();
        // assert(ptr && "serialize_Guid: could not cast meta_any to Guid");
        // j = ptr->raw();

        return false;
    }
#endif
} // namespace