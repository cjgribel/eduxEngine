//
//  LinearMotorConstraint.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-01.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "LinearMotorConstraint.hpp"

LinearMotorConstraint::LinearMotorConstraint(const v3f& rA,
                                             const v3f& rB,
                                             float L_min,
                                             float L_max,
                                             float L_init,
                                             float V_max,
                                             float F_max) :
rA(rA), rB(rB),
L_min(L_min), L_max(L_max), L_target(L_init),
V_max(V_max),
F_max(F_max),
act_mode(LinearMotorConstraintMode::Hold)
{
    
}

void LinearMotorConstraint::setMode(const RigidBody3dComponent& rbA,
                                    const RigidBody3dComponent& rbB,
                                    LinearMotorConstraintMode mode)
{
    if(mode == Hold)
    {
        v3f u = (rbA.X + rbA.R * rA) - (rbB.X + rbB.R * rB);
        L_target = clamp(u.norm2(), L_min, L_max);
    }
    act_mode = mode;
}

void LinearMotorConstraint::pre_solve(float dt_inverse,
                                      const RigidBody3dComponent& rbA,
                                      const RigidBody3dComponent& rbB)
{
    /* transformed anchor points */
    rAw = rbA.R * rA;
    rBw = rbB.R * rB;
    
    /* constraint direction */
    v3f u = (rbB.X + rBw) - (rbA.X + rAw);
    float u_len = u.norm2();
    if(u_len > FloatLimits::epsilon())
        un = u / u_len;
    else
        un = v3f_000;
    
    float C = 0.0f;
    if(act_mode == LinearMotorConstraintMode::Hold)
    {
        V_target = 0.0f;
        F_hasLimits = false;
        C = u_len - L_target;
    }
    else if(act_mode == LinearMotorConstraintMode::Extend)
    {
        if(u_len < L_min)
        {
            C = u_len - L_min;
            V_target = V_max;
            F_hasLimits = false;
        }
        else if(u_len <= L_target)
        {
            C = u_len - L_target;
            V_target = V_max;
            F_hasLimits = true; F_low_limit = -F_max; F_high_limit = F_max;
        }
        else if(u_len >= L_max)
        {
            C = u_len - L_max;
            V_target = 0.0f;
            F_hasLimits = false;
        }
        else
        {
            C = 0.0f;
            V_target = V_max;
            F_hasLimits = true;    F_low_limit = FloatLimits::min(); F_high_limit = F_max;
            L_target = clamp(u_len, L_min, L_max);
        }
    }
    else if(act_mode == LinearMotorConstraintMode::Contract)
    {
        if(u_len > L_max)
        {
            C = u_len - L_max;
            V_target = -V_max;
            F_hasLimits = false;
        }
        else if(u_len >= L_target)
        {
            C = u_len - L_target;
            V_target = -V_max;
            F_hasLimits = true; F_low_limit = -F_max; F_high_limit = F_max;
        }
        else if(u_len <= L_min)
        {
            C = u_len - L_min;
            V_target = 0.0f;
            F_hasLimits = false;
        }
        else
        {
            C = 0.0f;
            V_target = -V_max;
            F_hasLimits = true; F_low_limit = -F_max; F_high_limit = FloatLimits::max();
            L_target = clamp(u_len, L_min, L_max);
        }
    }
    
    
    //float Cc;
    //if(C > 0.0f)
    //    Cc = clampf(C - 0.005f, 0.0f, INF);
    //else
    //    Cc = -clampf(-C - 0.005f, 0.0f, INF);
    bias = ERP * dt_inverse * C;
    
    // Effective mass. Same as Distance constraint
    v3f crAu = rAw % un;
    v3f crBu = rBw % un;
    float K = (u * rbA.im).dot(u);
    K += crAu.dot(rbA.iI_w * crAu); // =?= (crAu * rbA.iI_w).dot(crAu); (see prismatic_constraint_t)
    K += (u * rbB.im).dot(u);
    K += crBu.dot(rbB.iI_w * crBu);
    iK = 1.0f / K;
    
    /* effective mass */
    //        v3f crAu = rAw % un;
    //        v3f crBu = rBw % un;
    //        float K = bodyA->imass + bodyA->iI * crAu * crAu + bodyB->imass + bodyB->iI * crBu * crBu;
    //        iK = 1.0f / K;
    
    lambdaAcc_vel = 0.0f;
}

void LinearMotorConstraint::solve(float dt,
                                  float dt_inverse,
                                  RigidBody3dComponent& rbA,
                                  RigidBody3dComponent& rbB)
{
    //
    // Velocity correcting impulse (clamped to limits)
    //
    
    // Anchor points velocities: rdot = v + w x r
    v3f rAdot = rbA.V + rbA.W % rAw;
    v3f rBdot = rbB.V + rbB.W % rBw;
    // Anchor points relative velocity
    v3f rABdot = rBdot - rAdot;
    
    // Velocity constraint, Jv = 0 (ideally)
    float Cdot = un.dot(rABdot) - V_target;
    // Calculate & clamp lambda, add to accumulated
    float lambda_vel = -iK * dt_inverse * Cdot;
    if(F_hasLimits)
    {
        lambda_vel = clamp(lambdaAcc_vel + lambda_vel, F_low_limit, F_high_limit) - lambdaAcc_vel;
        lambdaAcc_vel += lambda_vel;
    }
    // Calculate and apply velocity correcting impulse
    v3f P_vel = un * dt * lambda_vel;
    RigidBody::apply_impulse(rbA, -P_vel, rAw);
    RigidBody::apply_impulse(rbB, P_vel, rBw);
    //        bodyA->applyImpulse(-P_vel, rA);
    //        bodyB->applyImpulse(P_vel, rB);
    
    //
    // Position correcting impulse (unclamped)
    //
    
    // Calculate and apply impulse
    float lambda_pos = -iK * bias;
    v3f P_pos = un * lambda_pos;
    RigidBody::apply_impulse(rbA, -P_pos, rAw);
    RigidBody::apply_impulse(rbB, P_pos, rBw);
    //        bodyA->applyImpulse(-P_pos, rA);
    //        bodyB->applyImpulse(P_pos, rB);
}
