#pragma once

#include "EngineContext.hpp"
#include "MetaAux.h"
#include "AssetIndexData.hpp"
#include "AssetRef.hpp"

#include "imgui.h"

namespace eeng::editor::detail
{
    template<class T>
    std::string get_meta_type_name()
    {
        auto meta_type = entt::resolve<T>();
        assert(meta_type && "get_meta_type_name: type not registered in entt meta system");
        return std::string(meta_type.info().name());
    }

    template<class T>
    std::span<const AssetEntry* const>
        get_assets_of_type(AssetIndexDataPtr index)
    {
        assert(index);

        const std::string type_name = get_meta_type_name<T>(); // meta::get_meta_type_id_string<T>();
        auto it = index->by_type.find(type_name);
        if (it == index->by_type.end())
        {
            return {};
        }

        return it->second;
    }

    inline std::string make_asset_ref_label(const Guid& guid, AssetIndexDataPtr index)
    {
        if (!guid.valid())
        {
            return "<none>";
        }

        if (!index) return guid.to_string(); // fallback

        auto it = index->by_guid.find(guid);
        if (it == index->by_guid.end() || !it->second)
        {
            return guid.to_string(); // unknown guid
        }

        const AssetEntry* entry = it->second;
        return entry->meta.name; // + " [" + guid.to_string() + "]";
    }
}

namespace eeng::editor
{
    template<class T>
    bool asset_picker_popup(
        const char* popup_id,
        eeng::AssetRef<T>& ref,
        AssetIndexDataPtr index)
    {
        bool modified = false;

        if (!ImGui::BeginPopup(popup_id))
        {
            return false;
        }

        static char filter_buf[64] = {};
        ImGui::InputText("Filter", filter_buf, IM_ARRAYSIZE(filter_buf));

        auto assets = detail::get_assets_of_type<T>(index);

        {
            // Debug asset type info
            const std::string type_name = meta::get_meta_type_display_name<T>();
            const std::string full_type_name = meta::get_meta_type_id_string<T>(); // std::string(entt::resolve<T>().info().name());
            ImGui::TextDisabled("Type: %s (%s)", type_name.c_str(), full_type_name.c_str());
            ImGui::TextDisabled("by_type size: %zu", index->by_type.size());
            ImGui::TextDisabled("assets for this type: %zu", assets.size());
            ImGui::Separator();
        }

        ImGui::Separator();

        // Optional "clear" entry
        if (ImGui::Selectable("<none>", !ref.guid.valid()))
        {
            ref.guid = Guid{};
            ref.handle = {}; // unbind; let binder/rm handle later
            modified = true;
        }

        for (const AssetEntry* entry : assets)
        {
            if (!entry)
            {
                continue;
            }

            const std::string& name = entry->meta.name;
            const Guid& guid = entry->meta.guid;

            // Simple substring filter on name
            if (filter_buf[0] != '\0')
            {
                if (name.find(filter_buf) == std::string::npos)
                {
                    continue;
                }
            }

            const bool is_selected = (guid == ref.guid);
            std::string label = name; // + " [" + guid.to_string() + "]";

            if (ImGui::Selectable(label.c_str(), is_selected))
            {
                ref = eeng::AssetRef<T>{guid}; // unbound, bind later
                // ref.guid = guid;
                // ref.handle = {}; // reset handle; rebind below
                modified = true;
            }
        }

        ImGui::EndPopup();

        // immediately rebind here using RM?
        // if (modified && ctx.resource_manager)
        // {
        //     ref.handle = ctx.resource_manager->find_handle_for_guid<T>(ref.guid);
        // }

        return modified;
    }
}