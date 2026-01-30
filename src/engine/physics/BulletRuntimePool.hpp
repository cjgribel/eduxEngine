// Created by Codex 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <cstddef>

#include "util/PoolAllocatorTFH.h"

// Bullet headers are required because this pool stores Bullet types by value.
#include <btBulletDynamicsCommon.h>

namespace eeng::physics
{
    // BulletRuntimePool
    // - Purpose: optional pooled allocation for Bullet runtime objects to reduce heap churn.
    // - Roadmap:
    //   1) Add a compile flag (e.g. EENG_USE_BULLET_POOL) and swap BodyRuntime pointers to handles.
    //   2) Create/destroy bodies + motion states via this pool (remove from world before destroy).
    //   3) Add per-shape pools for common collider types if shape allocations are a hotspot.
    //      (Pooling btCollisionShape directly is unsafe because derived shapes vary in size.)
    class BulletRuntimePool
    {
    public:
        using RigidBodyHandle = Handle<btRigidBody>;
        using MotionStateHandle = Handle<btDefaultMotionState>;
        using CompoundShapeHandle = Handle<btCompoundShape>;

        BulletRuntimePool(
            std::size_t body_capacity = 0,
            std::size_t motion_state_capacity = 0,
            std::size_t compound_capacity = 0)
            : bodies_(body_capacity),
              motion_states_(motion_state_capacity),
              compounds_(compound_capacity)
        {
        }

        BulletRuntimePool(const BulletRuntimePool&) = delete;
        BulletRuntimePool& operator=(const BulletRuntimePool&) = delete;

        RigidBodyHandle create_body(const btRigidBody::btRigidBodyConstructionInfo& info)
        {
            return bodies_.create(info);
        }

        MotionStateHandle create_motion_state(const btTransform& start_transform)
        {
            return motion_states_.create(start_transform);
        }

        CompoundShapeHandle create_compound_shape()
        {
            return compounds_.create();
        }

        btRigidBody* get(RigidBodyHandle handle)
        {
            return handle ? &bodies_.get(handle) : nullptr;
        }

        btDefaultMotionState* get(MotionStateHandle handle)
        {
            return handle ? &motion_states_.get(handle) : nullptr;
        }

        btCompoundShape* get(CompoundShapeHandle handle)
        {
            return handle ? &compounds_.get(handle) : nullptr;
        }

        void destroy(RigidBodyHandle handle)
        {
            if (handle)
                bodies_.destroy(handle);
        }

        void destroy(MotionStateHandle handle)
        {
            if (handle)
                motion_states_.destroy(handle);
        }

        void destroy(CompoundShapeHandle handle)
        {
            if (handle)
                compounds_.destroy(handle);
        }

    private:
        PoolAllocatorTFH<btRigidBody> bodies_;
        PoolAllocatorTFH<btDefaultMotionState> motion_states_;
        PoolAllocatorTFH<btCompoundShape> compounds_;
    };
} // namespace eeng::physics
