// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/Entity.hpp"
#include "editor/MetaFieldPath.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace eeng::editor
{
    struct FieldChangedEvent;
}

namespace eeng::ecs::systems
{
    class TransformSystem
    {
    public:
        void init(EngineContext& ctx);

        void update(EngineContext& ctx, float delta_time);

        static void on_component_post_assign(
            EngineContext& ctx,
            const ecs::Entity& entity,
            const editor::MetaFieldPath& meta_path,
            bool is_undo);

    private:
        void handle_field_changed_event(const editor::FieldChangedEvent& event);

        // Choose one path to avoid double-updating dirty flags.
        static constexpr bool kDirtyViaMetaHook = true;
        static constexpr bool kDirtyViaEvent = false;
    };
}
