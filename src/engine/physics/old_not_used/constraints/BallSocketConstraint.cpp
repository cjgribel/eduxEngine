//
//  BallSocketConstraint.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-08-25.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "BallSocketConstraint.hpp"

void BallSocketConstraint::pre_solve(float dt_inverse,
               const RigidBody3dComponent& rbA,
               const RigidBody3dComponent& rbB)
{
#define ballsocket_ctr_ERP 0.6f /* 0.2f */
    
    // World anchor points
    rAw = rbA.R * rA;
    rBw = rbB.R * rB;
    
    // Bias
    v3f C = rbB.X + rBw - rbA.X - rAw;
    bias = C * (ballsocket_ctr_ERP * dt_inverse);
    
    // Effective mass
    skewA = mat3f::skew(rAw);
    m3f skewAT = skewA; skewAT.transpose();
    skewB = mat3f::skew(rBw);
    m3f skewBT = skewB; skewBT.transpose();
    iK = (m3f(rbA.im) + m3f(rbB.im) + skewA*rbA.iI_w*skewAT + skewB*rbB.iI_w*skewBT).inverse();
}

void BallSocketConstraint::solve(float dt,
           float dt_inverse,
           RigidBody3dComponent& rbA,
           RigidBody3dComponent& rbB)
{
    // Velocity constraint Cdot = Jv = 0
    vec3f Cdot = rbB.V - skewB*rbB.W - rbA.V + skewA*rbA.W;
    
    // Setup and apply impulse P = J^T lambda
    // This is lambda; J^T is applied via apply_impulse on both bodies
    vec3f P = iK * (Cdot + bias);
    RigidBody::apply_force(rbA, P, rAw);
    RigidBody::apply_force(rbB, -P, rBw);
}
