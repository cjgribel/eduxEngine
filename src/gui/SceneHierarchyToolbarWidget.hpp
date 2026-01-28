// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "EngineContext.hpp"
#include "EventQueue.h"
#include "engineapi/PlayModePolicy.hpp"
#include "editor/EditorActions.hpp"
// #include "ecs/EntityManager.hpp"
#include "engineapi/SelectionManager.hpp"
// #include "ecs/HeaderComponent.hpp"
#include <vector>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

namespace eeng::gui
{
    using namespace eeng::ecs;

    struct SceneTreeToolbarWidget
    {
        EngineContext& ctx;

        SceneTreeToolbarWidget(EngineContext& ctx)
            : ctx(ctx)
        {
        }

        void draw()
        {
            auto& entity_selection = *ctx.entity_selection;

            const bool has_selection = !entity_selection.empty();
            const bool has_multi_selection = entity_selection.size() > 1;
            const bool can_queue = static_cast<bool>(ctx.command_queue);
            const bool is_playing = ctx.services
                && ctx.services->play_mode_active.load(std::memory_order_relaxed);
            const bool allow_edit_actions = can_queue && !is_playing;

            if (!allow_edit_actions)
                ImGui::BeginDisabled();

            // New entity
            if (ImGui::Button("New"))
            {
                Entity entity_parent{};
                if (has_selection)
                    entity_parent = entity_selection.last();

                editor::SceneActions::create_entity(ctx, entity_parent);
            }

            ImGui::SameLine();

            // Destroy selected entities
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Delete"))
            {
                editor::SceneActions::delete_entities(ctx, entity_selection.get_all());

                entity_selection.clear();
            }
            if (!has_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Copy selected entities
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Copy"))
            {
                editor::SceneActions::copy_entities(ctx, entity_selection.get_all());
            }
            if (!has_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Reparent selected entities
            if (!has_multi_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Parent"))
            {
                editor::SceneActions::parent_entities(ctx, entity_selection.get_all());
            }
            if (!has_multi_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Unparent selected entities (set them as roots)
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Unparent"))
            {
                editor::SceneActions::unparent_entities(ctx, entity_selection.get_all());
            }
            if (!has_selection) ImGui::EndDisabled();

            if (!allow_edit_actions)
                ImGui::EndDisabled();

            const bool can_toggle = static_cast<bool>(ctx.event_queue);
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(12.0f, 0.0f));
            ImGui::SameLine();
            ImGui::TextUnformatted("Mode:");
            ImGui::SameLine();
            const ImVec4 mode_color = is_playing ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
                                                 : ImVec4(0.9f, 0.8f, 0.2f, 1.0f);
            ImGui::TextColored(mode_color, "%s", is_playing ? "Play" : "Edit");
            ImGui::SameLine();
            if (!can_toggle)
                ImGui::BeginDisabled();
            if (ImGui::Button(is_playing ? "Stop" : "Play"))
            {
                ctx.event_queue->dispatch(TogglePlayModeEvent{});
            }
            if (!can_toggle)
                ImGui::EndDisabled();

            const bool has_services = (ctx.services != nullptr);
            int policy_choice = 0; // 0 = Game, 1 = Preview, 2 = Strict
            if (has_services
                && ctx.services->play_mode_policy_override_enabled.load(std::memory_order_relaxed))
            {
                const auto override_policy = ctx.services->play_mode_policy_override.load(
                    std::memory_order_relaxed);
                policy_choice = (override_policy == PlayModePolicy::Strict) ? 2 : 1;
            }

            ImGui::SameLine();
            ImGui::Dummy(ImVec2(8.0f, 0.0f));
            ImGui::SameLine();
            ImGui::TextUnformatted("Policy:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            if (!has_services)
                ImGui::BeginDisabled();
            if (ImGui::Combo("##PlayPolicy", &policy_choice, "Game\0Preview\0Strict\0"))
            {
                if (policy_choice == 0)
                {
                    ctx.services->play_mode_policy_override_enabled.store(
                        false, std::memory_order_relaxed);
                }
                else
                {
                    ctx.services->play_mode_policy_override_enabled.store(
                        true, std::memory_order_relaxed);
                    ctx.services->play_mode_policy_override.store(
                        (policy_choice == 2) ? PlayModePolicy::Strict : PlayModePolicy::Preview,
                        std::memory_order_relaxed);
                }
            }
            if (!has_services)
                ImGui::EndDisabled();
        }
    };

} // namespace eeng::gui
