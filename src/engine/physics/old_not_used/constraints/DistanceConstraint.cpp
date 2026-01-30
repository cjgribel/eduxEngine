//
//  DistanceConstraint.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-08-29.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "DistanceConstraint.hpp"

void DistanceConstraint::pre_solve(float dt_inverse,
                                   const RigidBody3dComponent& rbA,
                                   const RigidBody3dComponent& rbB)
{
#define DistanceJointERP 0.15f
    // Anchor points in world space
    rAw = rbA.R * rA;
    rBw = rbB.R * rB;
    
    // Constraint vector
    v3f u = (rbB.X + rBw) - (rbA.X + rAw);
    float u_len = u.norm2();
    if(u_len > std::numeric_limits<float>::epsilon())
        un = u / u_len;
    else
        un = v3f_000;
    
    // Velocity bias term
    float C = u_len - L;
    bias = DistanceJointERP * dt_inverse * C;
    
    // Effective mass
    v3f crAu = rAw % un;
    v3f crBu = rBw % un;
    float K = (u * rbA.im).dot(u);
    K += crAu.dot(rbA.iI_w * crAu); // =?= (crAu * rbA.iI_w).dot(crAu); (see prismatic_constraint_t)
    K += (u * rbB.im).dot(u);
    K += crBu.dot(rbB.iI_w * crBu);
    iK = 1.0f / K;
}

void DistanceConstraint::solve(float dt,
                               float dt_inverse,
                               RigidBody3dComponent& rbA,
                               RigidBody3dComponent& rbB)
{
    // Anchor points velocities: rdot = v + w x r
    v3f rAdot = rbA.V + rbA.W % rAw;
    v3f rBdot = rbB.V + rbB.W % rBw;
    // Anchor points relative velocity
    v3f rABdot = rBdot - rAdot;
    
    // Velocity constraint, Jv = 0 (ideally)
    float Cdot = un.dot(rABdot);
    
    // Calculate and apply constraint impulse
    float lambda = -iK * (Cdot + bias);
    v3f P = un * lambda;
    RigidBody::apply_impulse(rbA, -P, rAw);
    RigidBody::apply_impulse(rbB, P, rBw);
}
