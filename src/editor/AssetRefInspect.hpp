
#pragma once

#include "EngineContext.hpp"
#include "InspectorState.hpp"
// #include "ecs/Entity.hpp"
#include "AssetRef.hpp"
#include "MetaAux.h"
#include "entt/entt.hpp"

namespace eeng::editor
{
    // 

    template <class T>
    bool inspect_AssetRef(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        // Check const?

        auto ptr = any.try_cast<eeng::AssetRef<T>>();
        assert(ptr && "inspect_EntityRef: could not cast meta_any to AssetRef");

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
} // namespace