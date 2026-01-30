// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <string>

#include "AssetRef.hpp"
#include "ecs/AnimationGraphInstance.hpp"
#include "assets/types/AnimationGraphAsset.hpp"

namespace entt
{
    class meta_any;
}

namespace eeng
{
    struct EngineContext;
}

namespace eeng::editor
{
    struct MetaFieldPath;
}

namespace eeng::ecs
{
    class Entity;

    struct AnimationGraphComponent
    {
        std::string name;
        AssetRef<assets::AnimationGraphAsset> graph_ref;
        AnimGraphInstance instance;
        bool enabled = true;

        AnimationGraphComponent() = default;
        explicit AnimationGraphComponent(const AssetRef<assets::AnimationGraphAsset>& graph_ref)
            : graph_ref(graph_ref)
        {
        }

        static void on_component_post_bind(entt::meta_any& any, EngineContext& ctx);
        static void on_component_post_assign(
            EngineContext& ctx,
            const ecs::Entity& entity,
            const editor::MetaFieldPath& meta_path,
            bool is_undo);
    };

    inline std::string to_string(const AnimationGraphComponent& t)
    {
        return std::format("AnimationGraphComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(AnimationGraphComponent& c, Visitor&& visitor)
    {
        visitor(c.graph_ref);
    }

    template<typename Visitor>
    void visit_asset_refs(const AnimationGraphComponent& c, Visitor&& visitor)
    {
        visitor(c.graph_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(AnimationGraphComponent&, Visitor&&) {}
}
