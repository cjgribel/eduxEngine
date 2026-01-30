// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "physics/PhysicsWorld.hpp"

#include <btBulletDynamicsCommon.h>

namespace eeng::physics
{
    PhysicsWorld::~PhysicsWorld()
    {
        shutdown();
    }

    void PhysicsWorld::init(const PhysicsWorldSettings& settings)
    {
        settings_ = settings;

        // Build the standard Bullet world stack.
        collision_config_ = std::make_unique<btDefaultCollisionConfiguration>();
        dispatcher_ = std::make_unique<btCollisionDispatcher>(collision_config_.get());
        broadphase_ = std::make_unique<btDbvtBroadphase>();
        solver_ = std::make_unique<btSequentialImpulseConstraintSolver>();
        world_ = std::make_unique<btDiscreteDynamicsWorld>(
            dispatcher_.get(),
            broadphase_.get(),
            solver_.get(),
            collision_config_.get());

        // Gravity is in Bullet world units (meters by default).
        world_->setGravity(btVector3(settings_.gravity.x, settings_.gravity.y, settings_.gravity.z));
    }

    void PhysicsWorld::shutdown()
    {
        // Destroy Bullet objects in reverse order to satisfy dependencies.
        world_.reset();
        solver_.reset();
        broadphase_.reset();
        dispatcher_.reset();
        collision_config_.reset();
    }

    void PhysicsWorld::step_simulation(float delta_time_seconds)
    {
        if (!world_)
            return;

        world_->stepSimulation(delta_time_seconds, settings_.max_substeps, settings_.fixed_time_step);
    }
} // namespace eeng::physics
