// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef TransformComponent_hpp
#define TransformComponent_hpp

#include <string>

namespace eeng::ecs
{
    struct TransformComponent
    {
        // If stable pointers to Transform are needed, e.g. in scene graph nodes
        // https://github.com/skypjack/entt/blob/master/docs/md/entity.md
        //static constexpr auto in_place_delete = true;

        float x{ 0.0f }, y{ 0.0f }, angle{ 0.0f };

        // Not meta-registered
        float x_parent{ 0.0f }, y_parent{ 0.0f }, angle_parent{ 0.0f };
        float x_global{ 0.0f }, y_global{ 0.0f }, angle_global{ 0.0f };

        // void compute_global_transform()
        // {
        //     x_global = x * cos(angle_parent) - y * sin(angle_parent) + x_parent;
        //     y_global = x * sin(angle_parent) + y * cos(angle_parent) + y_parent;
        //     angle_global = angle + angle_parent;
        // }

    };

    std::string to_string(const TransformComponent& t);

    template<typename Visitor> void visit_asset_refs(TransformComponent& t, Visitor&& visitor) {}

    template<typename Visitor> void visit_entities(TransformComponent& t, Visitor&& visitor) {}

#if 0

    // -> bool inspect(Transform& t, Editor::InspectorState& inspector)
    // Maybe needs #include "editor/InspectType.hpp" + InspectorState

    // + const (e.g. when used as key) ?
    bool inspect_Transform(void* ptr, Editor::InspectorState& inspector)
    {
        Transform* t = static_cast<Transform*>(ptr);
        bool mod = false;

        inspector.begin_leaf("x");
        mod |= Editor::inspect_type(t->x, inspector);
        inspector.end_leaf();

        inspector.begin_leaf("y");
        mod |= Editor::inspect_type(t->y, inspector);
        inspector.end_leaf();

        inspector.begin_leaf("angle");
        mod |= Editor::inspect_type(t->angle, inspector);
        inspector.end_leaf();

        return mod;
}
#endif
}

#endif // TransformComponent_hpp
