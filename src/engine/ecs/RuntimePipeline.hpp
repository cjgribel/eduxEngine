// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "ShapeRenderer.hpp"
#include "ecs/systems/AnimationGraphSystem.hpp"
#include "ecs/systems/AnimationSystem.hpp"
#include "ecs/systems/ConstraintSystem.hpp"
#include "ecs/systems/DebugRenderSystem.hpp"
#include "ecs/systems/PistonAnimSyncSystem.hpp"
#include "ecs/systems/PistonConstraintDriveSystem.hpp"
#include "ecs/systems/MousePointConstraintSystem.hpp"
#include "ecs/systems/ParticleRenderSystem.hpp"
#include "ecs/systems/ParticleSystem.hpp"
#include "ecs/systems/PhysicsSystem.hpp"
#include "ecs/systems/SpringDamperSystem.hpp"
#include "ecs/systems/TransformSocketSystem.hpp"
#include "ecs/systems/TwoAnchorAlignSystem.hpp"
#include "ecs/systems/RenderSystem.hpp"
#include "ecs/systems/ScriptSystem.hpp"
#include "ecs/systems/StickyNoteSystem.hpp"
#include "ecs/systems/TrailSystem.hpp"
#include "ecs/systems/TransformSystem.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <string>

namespace eeng::ecs
{
    enum class OverlayMode : std::uint8_t
    {
        Edit,
        Play
    };

    struct OverlayModeSettings
    {
        bool show_debug = true;
        bool show_trails = false;
    };

    struct OverlayRenderSettings
    {
        OverlayModeSettings edit{};
        OverlayModeSettings play{ .show_debug = true, .show_trails = true };
    };

    /**
     * @brief Engine-owned runtime pipeline for core ECS systems.
     *
     * This pipeline centralizes system order for shared engine systems so game
     * runtimes can stay focused on game-specific behavior.
     */
    class RuntimePipeline
    {
    public:
        void init(
            EngineContext& ctx,
            const std::string& render_vert_path = "shaders/phong_vert.glsl",
            const std::string& render_frag_path = "shaders/phong_frag.glsl")
        {
            render_system_ = std::make_unique<systems::RenderSystem>();
            render_system_->init(render_vert_path, render_frag_path);

            animation_system_ = std::make_unique<systems::AnimationSystem>();
            animation_graph_system_ = std::make_unique<systems::AnimationGraphSystem>();

            transform_system_ = std::make_unique<systems::TransformSystem>();
            transform_system_->init(ctx);
            transform_socket_system_ = std::make_unique<systems::TransformSocketSystem>();
            physics_system_ = std::make_unique<systems::PhysicsSystem>();
            physics_system_->init(ctx);

            constraint_system_ = std::make_unique<systems::ConstraintSystem>();
            mouse_point_constraint_system_ = std::make_unique<systems::MousePointConstraintSystem>();
            spring_damper_system_ = std::make_unique<systems::SpringDamperSystem>();
            piston_constraint_drive_system_ = std::make_unique<systems::PistonConstraintDriveSystem>();
            piston_anim_sync_system_ = std::make_unique<systems::PistonAnimSyncSystem>();
            two_anchor_align_system_ = std::make_unique<systems::TwoAnchorAlignSystem>();

            script_system_ = std::make_unique<systems::ScriptSystem>();
            script_system_->init(ctx);

            debug_render_system_ = std::make_unique<systems::DebugRenderSystem>();
            debug_render_settings_edit_ = debug_render_system_->settings;
            debug_render_settings_play_ = debug_render_system_->settings;
            particle_system_ = std::make_unique<systems::ParticleSystem>();
            particle_system_->set_threaded_simulation(true);
            particle_render_system_ = std::make_unique<systems::ParticleRenderSystem>();
            particle_render_system_->init();
            trail_system_ = std::make_unique<systems::TrailSystem>();
            sticky_note_system_ = std::make_unique<systems::StickyNoteSystem>();
        }

        void update_play(EngineContext& ctx, float delta_time)
        {
            if (!ctx.entity_manager)
                return;

            auto& registry = ctx.entity_manager->registry();

            if (mouse_point_constraint_system_ && physics_system_)
                mouse_point_constraint_system_->update(registry, ctx, *physics_system_);
            if (transform_socket_system_)
                transform_socket_system_->update(registry, ctx, delta_time, false);
            if (piston_constraint_drive_system_)
                piston_constraint_drive_system_->update_pre_physics(registry, ctx, delta_time);
            if (constraint_system_ && physics_system_)
                constraint_system_->update(registry, ctx, *physics_system_, delta_time);
            if (spring_damper_system_ && physics_system_)
                spring_damper_system_->update(registry, ctx, *physics_system_, delta_time);
            if (physics_system_)
                physics_system_->update(registry, ctx, delta_time);
            if (piston_constraint_drive_system_)
                piston_constraint_drive_system_->update_post_physics(registry, ctx, delta_time);
            if (piston_anim_sync_system_)
                piston_anim_sync_system_->update(registry, ctx, delta_time);
            if (animation_graph_system_)
                animation_graph_system_->update(registry, ctx, delta_time);
            if (animation_system_)
                animation_system_->update(registry, ctx, delta_time);
            if (script_system_)
                script_system_->update(registry, ctx, delta_time);
            if (transform_socket_system_)
                transform_socket_system_->update(registry, ctx, delta_time, false);
            if (two_anchor_align_system_)
                two_anchor_align_system_->update(registry, ctx, delta_time);

            update_common(ctx, delta_time);

            if (particle_system_)
                particle_system_->update(registry, ctx, delta_time, physics_system_.get());
            update_particle_monitor_stats(ctx);

            if (trail_system_ && overlay_settings_.play.show_trails)
                trail_system_->update(registry, ctx, delta_time);

            // Push a snapshot for editor UI without requiring direct system access.
            update_physics_monitor_stats(ctx);
        }

        void update_edit(EngineContext& ctx, float delta_time)
        {
            if (!ctx.entity_manager)
                return;

            update_common(ctx, delta_time);

            auto& registry = ctx.entity_manager->registry();
            if (physics_system_)
                physics_system_->update_edit(registry, ctx);
            if (mouse_point_constraint_system_ && physics_system_)
                mouse_point_constraint_system_->update(registry, ctx, *physics_system_);
            if (transform_socket_system_)
                transform_socket_system_->update(registry, ctx, delta_time, true);
            if (two_anchor_align_system_)
                two_anchor_align_system_->update(registry, ctx, delta_time);
            // Keep physics stats fresh in edit mode as well.
            update_physics_monitor_stats(ctx);
            if (sticky_note_system_)
                sticky_note_system_->update(registry, ctx, delta_time);
            if (particle_system_)
                particle_system_->update(registry, ctx, delta_time, physics_system_.get());
            update_particle_monitor_stats(ctx);
            if (trail_system_ && overlay_settings_.edit.show_trails)
                trail_system_->update(registry, ctx, delta_time);
        }

        void render_debug(
            entt::registry& registry,
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer,
            const glm::mat4& vp_p_v,
            int window_height)
        {
            const auto mode = active_overlay_mode(ctx);
            const auto& mode_settings = overlay_mode_settings(mode);
            const auto& debug_settings = debug_mode_settings(mode);
            if (debug_render_system_ && mode_settings.show_debug)
            {
                debug_render_system_->settings = debug_settings;
                debug_render_system_->render(registry, ctx, renderer, vp_p_v, window_height);
            }
            if (sticky_note_system_ && mode_settings.show_debug && debug_settings.show_sticky_notes)
                sticky_note_system_->render(registry, ctx, vp_p_v, window_height);
        }

        void render_runtime_overlays(
            entt::registry& registry,
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer)
        {
            const auto mode = active_overlay_mode(ctx);
            const auto& mode_settings = overlay_mode_settings(mode);
            if (!mode_settings.show_trails)
                return;

            if (trail_system_)
                trail_system_->render(registry, ctx, renderer);
        }

        void render_entities(
            entt::registry& registry,
            EngineContext& ctx,
            const glm::mat4& proj_view,
            const glm::vec3& light_pos,
            const glm::vec3& light_color,
            const glm::vec3& eye_pos)
        {
            if (!render_system_ || !render_system_->initialized())
                return;

            render_system_->render(
                registry,
                ctx,
                [&](GLuint program)
                {
                    glUniformMatrix4fv(
                        glGetUniformLocation(program, "ProjViewMatrix"),
                        1,
                        0,
                        glm::value_ptr(proj_view));
                    glUniform3fv(glGetUniformLocation(program, "lightpos"), 1, glm::value_ptr(light_pos));
                    glUniform3fv(glGetUniformLocation(program, "lightColor"), 1, glm::value_ptr(light_color));
                    glUniform3fv(glGetUniformLocation(program, "eyepos"), 1, glm::value_ptr(eye_pos));
                });
        }

        void render_particles(
            entt::registry& registry,
            EngineContext& ctx,
            const glm::mat4& proj_view,
            const glm::vec3& camera_right,
            const glm::vec3& camera_up)
        {
            if (!particle_render_system_ || !particle_system_ || !particle_render_system_->initialized())
                return;

            particle_render_system_->render(
                registry,
                ctx,
                *particle_system_,
                proj_view,
                camera_right,
                camera_up);
            update_particle_render_monitor_stats(ctx);
        }

        void shutdown()
        {
            if (render_system_)
                render_system_->shutdown();
            if (physics_system_)
                physics_system_->shutdown();
            if (script_system_)
                script_system_->shutdown();
            if (particle_render_system_)
                particle_render_system_->shutdown();
            if (particle_system_)
                particle_system_->clear();
        }

        systems::PhysicsSystem* physics_system() { return physics_system_.get(); }
        const systems::PhysicsSystem* physics_system() const { return physics_system_.get(); }
        systems::ParticleSystem* particle_system() { return particle_system_.get(); }
        const systems::ParticleSystem* particle_system() const { return particle_system_.get(); }
        systems::DebugRenderSettings* debug_render_settings()
        {
            return &debug_render_settings_edit_;
        }
        const systems::DebugRenderSettings* debug_render_settings() const
        {
            return &debug_render_settings_edit_;
        }
        systems::DebugRenderSettings* debug_render_settings_edit()
        {
            return &debug_render_settings_edit_;
        }
        const systems::DebugRenderSettings* debug_render_settings_edit() const
        {
            return &debug_render_settings_edit_;
        }
        systems::DebugRenderSettings* debug_render_settings_play()
        {
            return &debug_render_settings_play_;
        }
        const systems::DebugRenderSettings* debug_render_settings_play() const
        {
            return &debug_render_settings_play_;
        }
        OverlayRenderSettings* overlay_render_settings()
        {
            return &overlay_settings_;
        }
        const OverlayRenderSettings* overlay_render_settings() const
        {
            return &overlay_settings_;
        }

    private:
        static OverlayMode active_overlay_mode(const EngineContext& ctx)
        {
            if (ctx.services && ctx.services->play_mode_active.load(std::memory_order_relaxed))
                return OverlayMode::Play;
            return OverlayMode::Edit;
        }

        OverlayModeSettings& overlay_mode_settings(OverlayMode mode)
        {
            return mode == OverlayMode::Play ? overlay_settings_.play : overlay_settings_.edit;
        }

        const OverlayModeSettings& overlay_mode_settings(OverlayMode mode) const
        {
            return mode == OverlayMode::Play ? overlay_settings_.play : overlay_settings_.edit;
        }

        systems::DebugRenderSettings& debug_mode_settings(OverlayMode mode)
        {
            return mode == OverlayMode::Play ? debug_render_settings_play_ : debug_render_settings_edit_;
        }

        const systems::DebugRenderSettings& debug_mode_settings(OverlayMode mode) const
        {
            return mode == OverlayMode::Play ? debug_render_settings_play_ : debug_render_settings_edit_;
        }

        void update_common(EngineContext& ctx, float delta_time)
        {
            if (transform_system_)
                transform_system_->update(ctx, delta_time);
        }

        void update_physics_monitor_stats(EngineContext& ctx)
        {
            // Only write stats when the shared snapshot is available.
            if (!physics_system_ || !ctx.services || !ctx.services->physics_monitor_stats)
                return;

            const auto stats = physics_system_->get_stats();
            auto& out = *ctx.services->physics_monitor_stats;
            out.body_count = stats.body_count;
            out.collision_objects = stats.collision_objects;
            out.manifolds = stats.manifolds;
            out.contact_points = stats.contact_points;
            out.dirty_entities = stats.dirty_entities;
            out.event_entities = stats.event_entities;
            out.tracked_contacts = stats.tracked_contacts;
            out.valid = true;
        }

        void update_particle_monitor_stats(EngineContext& ctx)
        {
            if (!particle_system_ || !ctx.services || !ctx.services->particle_monitor_stats)
                return;

            const auto& stats = particle_system_->frame_stats();
            auto& out = *ctx.services->particle_monitor_stats;
            out.emitter_count = stats.emitter_count;
            out.visible_emitter_count = stats.visible_emitter_count;
            out.live_particles = stats.live_particles;
            out.rendered_particles = stats.rendered_particles;
            out.draw_batches = 0;
            out.hit_events_emitted = stats.hit_events_emitted;
            out.collisions_requested = stats.collisions_requested;
            out.threaded_simulation_enabled = stats.threaded_simulation_enabled;
            out.threaded_simulation_used = stats.threaded_simulation_used;
            out.valid = true;
        }

        void update_particle_render_monitor_stats(EngineContext& ctx)
        {
            if (!particle_render_system_ || !ctx.services || !ctx.services->particle_monitor_stats)
                return;

            auto& out = *ctx.services->particle_monitor_stats;
            out.draw_batches = particle_render_system_->last_draw_batch_count();
            out.valid = true;
        }

        std::unique_ptr<systems::RenderSystem> render_system_;
        std::unique_ptr<systems::AnimationSystem> animation_system_;
        std::unique_ptr<systems::AnimationGraphSystem> animation_graph_system_;
        std::unique_ptr<systems::TransformSystem> transform_system_;
        std::unique_ptr<systems::TransformSocketSystem> transform_socket_system_;
        std::unique_ptr<systems::PhysicsSystem> physics_system_;
        std::unique_ptr<systems::ConstraintSystem> constraint_system_;
        std::unique_ptr<systems::MousePointConstraintSystem> mouse_point_constraint_system_;
        std::unique_ptr<systems::SpringDamperSystem> spring_damper_system_;
        std::unique_ptr<systems::PistonConstraintDriveSystem> piston_constraint_drive_system_;
        std::unique_ptr<systems::PistonAnimSyncSystem> piston_anim_sync_system_;
        std::unique_ptr<systems::TwoAnchorAlignSystem> two_anchor_align_system_;
        std::unique_ptr<systems::ScriptSystem> script_system_;
        std::unique_ptr<systems::DebugRenderSystem> debug_render_system_;
        std::unique_ptr<systems::ParticleSystem> particle_system_;
        std::unique_ptr<systems::ParticleRenderSystem> particle_render_system_;
        systems::DebugRenderSettings debug_render_settings_edit_{};
        systems::DebugRenderSettings debug_render_settings_play_{};
        OverlayRenderSettings overlay_settings_{};
        std::unique_ptr<systems::TrailSystem> trail_system_;
        std::unique_ptr<systems::StickyNoteSystem> sticky_note_system_;
    };
} // namespace eeng::ecs
