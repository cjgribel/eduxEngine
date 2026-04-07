// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "LogMacros.h"
#include "editor/ProjectConfig.hpp"
#include "ecs/RuntimePipeline.hpp"
#include "ecs/systems/DebugRenderSystem.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace eeng::editor
{
    namespace detail
    {
        struct DebugBoolField
        {
            const char* key;
            bool eeng::ecs::systems::DebugRenderSettings::* member;
        };

        inline constexpr std::array<DebugBoolField, 36> kDebugBoolFields{ {
            { "show_transform_labels", &eeng::ecs::systems::DebugRenderSettings::show_transform_labels },
            { "transform_label_show_name", &eeng::ecs::systems::DebugRenderSettings::transform_label_show_name },
            { "transform_label_show_position", &eeng::ecs::systems::DebugRenderSettings::transform_label_show_position },
            { "transform_label_show_rotation", &eeng::ecs::systems::DebugRenderSettings::transform_label_show_rotation },
            { "transform_label_show_scale", &eeng::ecs::systems::DebugRenderSettings::transform_label_show_scale },
            { "show_colliders", &eeng::ecs::systems::DebugRenderSettings::show_colliders },
            { "show_collider_labels", &eeng::ecs::systems::DebugRenderSettings::show_collider_labels },
            { "collider_label_show_id", &eeng::ecs::systems::DebugRenderSettings::collider_label_show_id },
            { "collider_label_show_type", &eeng::ecs::systems::DebugRenderSettings::collider_label_show_type },
            { "collider_label_show_trigger_state", &eeng::ecs::systems::DebugRenderSettings::collider_label_show_trigger_state },
            { "collider_label_show_local_position", &eeng::ecs::systems::DebugRenderSettings::collider_label_show_local_position },
            { "collider_label_show_dimensions", &eeng::ecs::systems::DebugRenderSettings::collider_label_show_dimensions },
            { "show_rigidbody_labels", &eeng::ecs::systems::DebugRenderSettings::show_rigidbody_labels },
            { "rigidbody_label_show_motion_type", &eeng::ecs::systems::DebugRenderSettings::rigidbody_label_show_motion_type },
            { "rigidbody_label_show_mass", &eeng::ecs::systems::DebugRenderSettings::rigidbody_label_show_mass },
            { "rigidbody_label_show_inertia", &eeng::ecs::systems::DebugRenderSettings::rigidbody_label_show_inertia },
            { "rigidbody_label_show_damping", &eeng::ecs::systems::DebugRenderSettings::rigidbody_label_show_damping },
            { "rigidbody_label_show_com_offset", &eeng::ecs::systems::DebugRenderSettings::rigidbody_label_show_com_offset },
            { "show_rigidbody_com", &eeng::ecs::systems::DebugRenderSettings::show_rigidbody_com },
            { "show_rigidbody_axes", &eeng::ecs::systems::DebugRenderSettings::show_rigidbody_axes },
            { "show_rigidbody_offset", &eeng::ecs::systems::DebugRenderSettings::show_rigidbody_offset },
            { "show_raycast_debug", &eeng::ecs::systems::DebugRenderSettings::show_raycast_debug },
            { "show_springs", &eeng::ecs::systems::DebugRenderSettings::show_springs },
            { "show_constraints", &eeng::ecs::systems::DebugRenderSettings::show_constraints },
            { "show_constraint_hinges", &eeng::ecs::systems::DebugRenderSettings::show_constraint_hinges },
            { "show_constraint_sliders", &eeng::ecs::systems::DebugRenderSettings::show_constraint_sliders },
            { "show_constraint_6dof", &eeng::ecs::systems::DebugRenderSettings::show_constraint_6dof },
            { "show_constraint_points", &eeng::ecs::systems::DebugRenderSettings::show_constraint_points },
            { "show_two_anchor_align", &eeng::ecs::systems::DebugRenderSettings::show_two_anchor_align },
            { "show_demo_shapes", &eeng::ecs::systems::DebugRenderSettings::show_demo_shapes },
            { "show_sticky_notes", &eeng::ecs::systems::DebugRenderSettings::show_sticky_notes },
            { "show_skeleton", &eeng::ecs::systems::DebugRenderSettings::show_skeleton },
            { "show_skeleton_nodes", &eeng::ecs::systems::DebugRenderSettings::show_skeleton_nodes },
            { "show_skeleton_axes", &eeng::ecs::systems::DebugRenderSettings::show_skeleton_axes },
            { "show_skeleton_labels", &eeng::ecs::systems::DebugRenderSettings::show_skeleton_labels },
            { "show_skeleton_bones_only", &eeng::ecs::systems::DebugRenderSettings::show_skeleton_bones_only }
        } };

        inline std::filesystem::path overlay_settings_path(EngineContext& ctx)
        {
            std::filesystem::path project_root = std::filesystem::current_path();
            if (ctx.project_config)
                project_root = ctx.project_config->project_root;
            return project_root / ".editor" / "overlay_rendering.json";
        }

        inline void to_json_overlay_mode(
            nlohmann::json& j,
            const eeng::ecs::OverlayModeSettings& settings)
        {
            j = nlohmann::json::object();
            j["show_debug"] = settings.show_debug;
            j["show_trails"] = settings.show_trails;
        }

        inline void from_json_overlay_mode(
            const nlohmann::json& j,
            eeng::ecs::OverlayModeSettings& settings)
        {
            if (!j.is_object())
                return;
            if (j.contains("show_debug") && j["show_debug"].is_boolean())
                settings.show_debug = j["show_debug"].get<bool>();
            if (j.contains("show_trails") && j["show_trails"].is_boolean())
                settings.show_trails = j["show_trails"].get<bool>();
        }

        inline void to_json_debug_mode(
            nlohmann::json& j,
            const eeng::ecs::systems::DebugRenderSettings& settings)
        {
            j = nlohmann::json::object();
            for (const auto& field : kDebugBoolFields)
                j[field.key] = settings.*(field.member);
        }

        inline void from_json_debug_mode(
            const nlohmann::json& j,
            eeng::ecs::systems::DebugRenderSettings& settings)
        {
            if (!j.is_object())
                return;
            for (const auto& field : kDebugBoolFields)
            {
                auto it = j.find(field.key);
                if (it != j.end() && it->is_boolean())
                    settings.*(field.member) = it->get<bool>();
            }
        }
    } // namespace detail

    // Load per-project overlay/debug visibility settings from:
    //   <project_root>/.editor/overlay_rendering.json
    // Missing fields preserve the caller-provided defaults.
    inline bool load_overlay_render_settings(
        EngineContext& ctx,
        ecs::OverlayRenderSettings& overlay_settings,
        ecs::systems::DebugRenderSettings& debug_edit,
        ecs::systems::DebugRenderSettings& debug_play)
    {
        const auto path = detail::overlay_settings_path(ctx);
        if (!std::filesystem::exists(path))
            return false;

        try
        {
            std::ifstream in(path);
            if (!in)
                return false;

            nlohmann::json j;
            in >> j;

            if (const auto it_edit = j.find("edit"); it_edit != j.end() && it_edit->is_object())
            {
                if (const auto it_overlay = it_edit->find("overlay"); it_overlay != it_edit->end())
                    detail::from_json_overlay_mode(*it_overlay, overlay_settings.edit);
                if (const auto it_debug = it_edit->find("debug"); it_debug != it_edit->end())
                    detail::from_json_debug_mode(*it_debug, debug_edit);
            }

            if (const auto it_play = j.find("play"); it_play != j.end() && it_play->is_object())
            {
                if (const auto it_overlay = it_play->find("overlay"); it_overlay != it_play->end())
                    detail::from_json_overlay_mode(*it_overlay, overlay_settings.play);
                if (const auto it_debug = it_play->find("debug"); it_debug != it_play->end())
                    detail::from_json_debug_mode(*it_debug, debug_play);
            }
        }
        catch (const std::exception& ex)
        {
            EENG_LOG_WARN(&ctx, "Failed to load overlay rendering settings from '%s': %s",
                path.string().c_str(),
                ex.what());
            return false;
        }

        return true;
    }

    // Save per-project overlay/debug visibility settings to:
    //   <project_root>/.editor/overlay_rendering.json
    inline bool save_overlay_render_settings(
        EngineContext& ctx,
        const ecs::OverlayRenderSettings& overlay_settings,
        const ecs::systems::DebugRenderSettings& debug_edit,
        const ecs::systems::DebugRenderSettings& debug_play)
    {
        const auto path = detail::overlay_settings_path(ctx);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
            EENG_LOG_WARN(&ctx, "Failed to create overlay settings directory '%s': %s",
                path.parent_path().string().c_str(),
                ec.message().c_str());
            return false;
        }

        try
        {
            nlohmann::json out = nlohmann::json::object();
            out["version"] = 1;

            nlohmann::json edit = nlohmann::json::object();
            detail::to_json_overlay_mode(edit["overlay"], overlay_settings.edit);
            detail::to_json_debug_mode(edit["debug"], debug_edit);
            out["edit"] = std::move(edit);

            nlohmann::json play = nlohmann::json::object();
            detail::to_json_overlay_mode(play["overlay"], overlay_settings.play);
            detail::to_json_debug_mode(play["debug"], debug_play);
            out["play"] = std::move(play);

            std::ofstream fout(path);
            if (!fout)
                throw std::runtime_error("Could not open file for writing.");
            fout << std::setw(2) << out << '\n';
        }
        catch (const std::exception& ex)
        {
            EENG_LOG_WARN(&ctx, "Failed to save overlay rendering settings to '%s': %s",
                path.string().c_str(),
                ex.what());
            return false;
        }

        return true;
    }
} // namespace eeng::editor
