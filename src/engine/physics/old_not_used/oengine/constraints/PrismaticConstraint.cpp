//
//  PrismaticConstraint.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-01.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "PrismaticConstraint.hpp"

void PrismaticConstraint::pre_solve(float dt_inverse,
                                    const RigidBody3dComponent& rbA,
                                    const RigidBody3dComponent& rbB)
{
#define PrismaticCtr_CMax 5000.0f
#define PrismaticCtr_ERP 0.2f
    
    // World anchor points
    rAw = rbA.R * rA;
    rBw = rbB.R * rB;
    u = rbB.X + rBw - rbA.X - rAw;
    
    // World constraint vector
    lw = rbA.R * l;
    
    // Bias
    float C = lw.dot(u);
    C = clamp(C, -PrismaticCtr_CMax, PrismaticCtr_CMax);
    bias = C * PrismaticCtr_ERP * dt_inverse;
    
    // Effective mass
    v3f crAl = lw % (u + rAw);
    v3f crBl = rBw % lw;
    float K = rbA.im + rbB.im;
    K += (crAl*rbA.iI_w).dot(crAl);
    K += (crBl*rbB.iI_w).dot(crBl);
    iK = 1.0f/K;
}

void PrismaticConstraint::solve(float dt,
                                float dt_inverse,
                                RigidBody3dComponent& rbA,
                                RigidBody3dComponent& rbB)
{
    // Velocity constraint
    float Cdot = -lw.dot(rbA.V);
    Cdot += (lw % (u+rAw)).dot(rbA.W);
    Cdot += lw.dot(rbB.V);
    Cdot += (rBw % lw).dot(rbB.W);
    
    // Apply impulse
    v3f P = lw * (iK * (Cdot + bias));
    RigidBody::apply_impulse(rbA, P, u + rAw);
    RigidBody::apply_impulse(rbB, -P, rBw);
}
