// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "ShapeRenderer.hpp"
#include "ecs/systems/AnimationGraphSystem.hpp"
#include "ecs/systems/AnimationSystem.hpp"
#include "ecs/systems/DebugRenderSystem.hpp"
#include "ecs/systems/PhysicsSystem.hpp"
#include "ecs/systems/RenderSystem.hpp"
#include "ecs/systems/ScriptSystem.hpp"
#include "ecs/systems/StickyNoteSystem.hpp"
#include "ecs/systems/TransformSystem.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <string>

namespace eeng::ecs
{
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

            physics_system_ = std::make_unique<systems::PhysicsSystem>();
            physics_system_->init(ctx);

            script_system_ = std::make_unique<systems::ScriptSystem>();
            script_system_->init(ctx);

            debug_render_system_ = std::make_unique<systems::DebugRenderSystem>();
            sticky_note_system_ = std::make_unique<systems::StickyNoteSystem>();
        }

        void update_play(EngineContext& ctx, float delta_time)
        {
            if (!ctx.entity_manager)
                return;

            auto& registry = ctx.entity_manager->registry();

            if (animation_graph_system_)
                animation_graph_system_->update(registry, ctx, delta_time);
            if (animation_system_)
                animation_system_->update(registry, ctx, delta_time);
            if (physics_system_)
                physics_system_->update(registry, ctx, delta_time);
            if (script_system_)
                script_system_->update(registry, ctx, delta_time);

            update_common(ctx, delta_time);
        }

        void update_edit(EngineContext& ctx, float delta_time)
        {
            if (!ctx.entity_manager)
                return;

            update_common(ctx, delta_time);

            auto& registry = ctx.entity_manager->registry();
            if (sticky_note_system_)
                sticky_note_system_->update(registry, ctx, delta_time);
        }

        void render_debug(
            entt::registry& registry,
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer,
            const glm::mat4& vp_p_v,
            int window_height)
        {
            if (debug_render_system_)
                debug_render_system_->render(registry, ctx, renderer, vp_p_v, window_height);
            if (sticky_note_system_)
                sticky_note_system_->render(registry, ctx, vp_p_v, window_height);
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

        void shutdown()
        {
            if (render_system_)
                render_system_->shutdown();
            if (physics_system_)
                physics_system_->shutdown();
            if (script_system_)
                script_system_->shutdown();
        }

        systems::PhysicsSystem* physics_system() { return physics_system_.get(); }
        const systems::PhysicsSystem* physics_system() const { return physics_system_.get(); }

    private:
        void update_common(EngineContext& ctx, float delta_time)
        {
            if (transform_system_)
                transform_system_->update(ctx, delta_time);
        }

        std::unique_ptr<systems::RenderSystem> render_system_;
        std::unique_ptr<systems::AnimationSystem> animation_system_;
        std::unique_ptr<systems::AnimationGraphSystem> animation_graph_system_;
        std::unique_ptr<systems::TransformSystem> transform_system_;
        std::unique_ptr<systems::PhysicsSystem> physics_system_;
        std::unique_ptr<systems::ScriptSystem> script_system_;
        std::unique_ptr<systems::DebugRenderSystem> debug_render_system_;
        std::unique_ptr<systems::StickyNoteSystem> sticky_note_system_;
    };
} // namespace eeng::ecs
