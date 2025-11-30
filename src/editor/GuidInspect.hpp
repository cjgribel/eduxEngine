
#pragma once

#include "InspectorState.hpp"
#include "Guid.h"
#include "entt/entt.hpp"

namespace eeng::editor
{
    // 

    bool inspect_Guid(entt::meta_any& any, InspectorState& inspector)
    {
        // Check const?
        // any.type();

        auto ptr = any.try_cast<Guid>();
        assert(ptr && "inspect_Guid: could not cast meta_any to Guid");

        // inspector.begin_leaf("GUID");
        
        auto& guid = *ptr;
        ImGui::TextUnformatted(guid.valid() ? guid.to_string().c_str() : "n/a");
        
        // inspector.end_leaf();

        return false;
    }
} // namespace