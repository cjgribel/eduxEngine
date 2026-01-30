//
//  AngularMotorConstraint.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-01.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "AngularMotorConstraint.hpp"

void AngularMotorConstraint::enable(float T_max,
                                    float V_target)
{
    this->T_max = T_max;
    this->V_target = V_target;
    enable();
}

void AngularMotorConstraint::enable() { enabled = true; }

void AngularMotorConstraint::disable() { enabled = false; }

void AngularMotorConstraint::pre_solve(float dt_inverse,
                                       const RigidBody3dComponent& rbA,
                                       const RigidBody3dComponent& rbB)
{
    if (!enabled) return;
    
    // Axle in world space
    u = {0,0,1};
    uw = rbA.R * u;
    
    // Effective mass
    iK = 1.0f/( uw.dot((rbA.iI_w + rbB.iI_w)*uw) );
    
    // Reset accumulated torque
    lambda_acc = 0;
}

void AngularMotorConstraint::solve(float dt,
                                   float dt_inverse,
                                   RigidBody3dComponent& rbA,
                                   RigidBody3dComponent& rbB)
{
    if (!enabled) return;
    
    // Velocity constraint
    float Cdot = uw.dot(rbA.W - rbB.W) - V_target;
    
    // Impulse magnitude
    float lambda = -Cdot * iK * dt_inverse; // integrate to force level, so limits can be expressed as torques
    lambda = clamp(lambda_acc + lambda, -T_max, T_max) - lambda_acc;
    lambda_acc += lambda;
    
    // Apply angular impulse
    v3f P = uw * lambda * dt; // back to velocity level
    RigidBody::apply_angular_impulse(rbA, P);
    RigidBody::apply_angular_impulse(rbB, -P);
}
