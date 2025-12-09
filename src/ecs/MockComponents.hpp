#ifndef MockComponents_hpp
#define MockComponents_hpp

#include "ResourceTypes.hpp"
#include "Guid.h"
#include "LogGlobals.hpp" // DEBUG


#include "ecs/Entity.hpp"

namespace eeng::ecs::mock
{
    struct MockPlayerComponent
    {
        float position{ 0.0f };
        float health{ 100.0f };
        ecs::EntityRef camera_ref;
        AssetRef<eeng::mock::Model> model_ref;
    };

    template<typename Visitor>
    void visit_asset_refs(MockPlayerComponent& t, Visitor&& visitor)
    {
        LogGlobals::log("[visit_asset_refs<MockPlayerComponent>]");
        visitor(t.model_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(MockPlayerComponent& t, Visitor&& visitor)
    {
        visitor(t.camera_ref);
    }
    struct MockCameraComponent
    {
        float position{ 0.0f };
        float fov{ 60.0f };
        ecs::EntityRef target_ref;
        AssetRef<eeng::mock::Model> model_ref;
    };

    template<typename Visitor>
    void visit_asset_refs(MockCameraComponent& t, Visitor&& visitor)
    {
        LogGlobals::log("[visit_asset_refs<MockCameraComponent>]");
        visitor(t.model_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(MockCameraComponent& t, Visitor&& visitor)
    {
        visitor(t.target_ref);
    }
} // namespace eeng::mock

#endif // MockComponents_hpp
