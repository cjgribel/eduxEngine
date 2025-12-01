
#pragma once

#include "EngineContext.hpp"
#include "InspectorState.hpp"
#include "ecs/Entity.hpp"
#include "entt/entt.hpp"

namespace eeng::editor
{
    // + ctx
    bool inspect_EntityRef(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        // Check const?

        auto ptr = any.try_cast<ecs::EntityRef>();
        assert(ptr && "inspect_EntityRef: could not cast meta_any to EntityRef");

        inspector.begin_leaf("GUID");
        
        auto& guid = ptr->get_guid();
        ImGui::TextUnformatted(guid.valid() ? guid.to_string().c_str() : "n/a");
        
        //bool guid_modified = editor::inspect_type(guid, inspector);
        inspector.end_leaf();

        inspector.begin_leaf("Live ID");
        auto entity = ptr->get_entity();
        ImGui::TextUnformatted(entity.is_null() ? "n/a" : std::to_string(entity.to_integral()).c_str());
        inspector.end_leaf();

        // auto ptr = any.try_cast<Guid>();
        // assert(ptr && "serialize_Guid: could not cast meta_any to Guid");
        // j = ptr->raw();

        return false;
    }
} // namespace