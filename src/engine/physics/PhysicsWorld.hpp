// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <memory>

#include <glm/glm.hpp>

// Bullet headers are required here because we own Bullet types by value in std::unique_ptr.
#include <btBulletDynamicsCommon.h>

namespace eeng::physics
{
    // Minimal world settings for the initial Bullet integration.
    struct PhysicsWorldSettings
    {
        // Engine units per meter (1.0f => 1 engine unit = 1 meter).
        float units_per_meter = 1.0f;

        // Gravity expressed in meters/second^2 (Bullet world units).
        glm::vec3 gravity{ 0.0f, -9.81f, 0.0f };

        // Bullet stepping parameters (fixed step with max substeps).
        float fixed_time_step = 1.0f / 60.0f;
        int max_substeps = 8;
    };

    // Owns Bullet world state and provides a thin abstraction layer.
    class PhysicsWorld
    {
    public:
        PhysicsWorld() = default;
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        void init(const PhysicsWorldSettings& settings);
        void shutdown();

        // Step the Bullet simulation using the configured fixed timestep.
        void step_simulation(float delta_time_seconds);

        btDiscreteDynamicsWorld* world() const noexcept { return world_.get(); }

        float units_per_meter() const noexcept { return settings_.units_per_meter; }
        float meters_per_unit() const noexcept
        {
            return settings_.units_per_meter > 0.0f ? 1.0f / settings_.units_per_meter : 1.0f;
        }

        const glm::vec3& gravity() const noexcept { return settings_.gravity; }

    private:
        PhysicsWorldSettings settings_{};

        // Bullet world construction pieces (kept for lifetime management).
        std::unique_ptr<btDefaultCollisionConfiguration> collision_config_;
        std::unique_ptr<btCollisionDispatcher> dispatcher_;
        std::unique_ptr<btBroadphaseInterface> broadphase_;
        std::unique_ptr<btConstraintSolver> solver_;
        std::unique_ptr<btDiscreteDynamicsWorld> world_;
    };
} // namespace eeng::physics
