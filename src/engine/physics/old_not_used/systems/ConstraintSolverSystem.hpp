//
//  ConstraintSolverSystem.hpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-08-24.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef ConstraintSolverSystem_hpp
#define ConstraintSolverSystem_hpp

#include <stdio.h>
#include <entt/entt.hpp>
#include "ConstraintComponent.hpp"

class ConstraintSolverSystem
{
public:
    static void pre_solve(entt::registry& registry,
                          float dt,
                          float dt_inverse)
    {
        auto view_ballsocket = registry.view<ConstraintComponent<BallSocketConstraint>>();
        auto view_conetwist = registry.view<ConstraintComponent<ConeTwistConstraint>>();
        auto view_angle = registry.view<ConstraintComponent<AngleConstraint>>();
        auto view_dist = registry.view<ConstraintComponent<DistanceConstraint>>();
        auto view_prism = registry.view<ConstraintComponent<PrismaticConstraint>>();
        auto view_angmotor = registry.view<ConstraintComponent<AngularMotorConstraint>>();
        auto view_linmotor = registry.view<ConstraintComponent<LinearMotorConstraint>>();
        
        for(auto entity: view_ballsocket)
            _pre_solve<BallSocketConstraint>(registry, entity, dt_inverse);
        
        for(auto entity: view_conetwist)
            _pre_solve<ConeTwistConstraint>(registry, entity, dt_inverse);

        for(auto entity: view_angle)
            _pre_solve<AngleConstraint>(registry, entity, dt_inverse);
        
        for(auto entity: view_dist)
            _pre_solve<DistanceConstraint>(registry, entity, dt_inverse);
        
        for(auto entity: view_prism)
            _pre_solve<PrismaticConstraint>(registry, entity, dt_inverse);

        for(auto entity: view_angmotor)
            _pre_solve<AngularMotorConstraint>(registry, entity, dt_inverse);
        
        for(auto entity: view_linmotor)
            _pre_solve<LinearMotorConstraint>(registry, entity, dt_inverse);
        
        // + other constraint types here
    }
    
    static inline void solve(entt::registry& registry,
                      float dt,
                      float dt_inverse)
    {
        auto view_ballsocket = registry.view<ConstraintComponent<BallSocketConstraint>>();
        auto view_conetwist = registry.view<ConstraintComponent<ConeTwistConstraint>>();
        auto view_angle = registry.view<ConstraintComponent<AngleConstraint>>();
        auto view_dist = registry.view<ConstraintComponent<DistanceConstraint>>();
        auto view_prism = registry.view<ConstraintComponent<PrismaticConstraint>>();
        auto view_angmotor = registry.view<ConstraintComponent<AngularMotorConstraint>>();
        auto view_linmotor = registry.view<ConstraintComponent<LinearMotorConstraint>>();
        
        for(auto entity: view_ballsocket)
            _solve<BallSocketConstraint>(registry, entity, dt, dt_inverse);
        
        for(auto entity: view_conetwist)
            _solve<ConeTwistConstraint>(registry, entity, dt, dt_inverse);
        
        for(auto entity: view_angle)
            _solve<AngleConstraint>(registry, entity, dt, dt_inverse);
        
        for(auto entity: view_dist)
            _solve<DistanceConstraint>(registry, entity, dt, dt_inverse);
        
        for(auto entity: view_prism)
            _solve<PrismaticConstraint>(registry, entity, dt, dt_inverse);
        
        for(auto entity: view_angmotor)
            _solve<AngularMotorConstraint>(registry, entity, dt, dt_inverse);
        
        for(auto entity: view_linmotor)
            _solve<LinearMotorConstraint>(registry, entity, dt, dt_inverse);
        
        // + other constraint types here
    }
    
private:
    template<class ConstraintType>
    static inline void _pre_solve(entt::registry& registry,
                                  entt::entity entity,
                                  float dt_inverse)
    {
        auto& ctr = registry.get<ConstraintComponent<ConstraintType>>(entity);
        EntityPairType& rb_pair = ctr.rb_pair.pair;
        auto& rbA = registry.get<RigidBody3dComponent>(rb_pair.first);
        auto& rbB = registry.get<RigidBody3dComponent>(rb_pair.second);
        ctr.constraint.pre_solve(dt_inverse, rbA, rbB);
    }
    
    template<class ConstraintType>
    static inline void _solve(entt::registry& registry,
                              entt::entity entity,
                              float dt,
                              float dt_inverse)
    {
        auto& ctr = registry.get<ConstraintComponent<ConstraintType>>(entity);
        EntityPairType& rb_pair = ctr.rb_pair.pair;
        auto& rbA = registry.get<RigidBody3dComponent>(rb_pair.first);
        auto& rbB = registry.get<RigidBody3dComponent>(rb_pair.second);
        ctr.constraint.solve(dt, dt_inverse, rbA, rbB);
    }
};

#endif /* ConstraintSolverSystem_hpp */
