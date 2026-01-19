//
//  AngleConstraint.cpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-08-25.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "AngleConstraint.hpp"

void AngleConstraint::set_limits(float angle_max,
                                 const mat3f &frameA,
                                 const mat3f &frameB)
{
    theta_max = angle_max;
    limits_active = true;
    
    this->frameA = frameA;
    this->frameB = frameB;
}

void AngleConstraint::pre_solve(float dt_inverse,
                                const RigidBody3dComponent& rbA,
                                const RigidBody3dComponent& rbB)
{
#define AngleCtr_ERP 0.6f /* 0.2f */
#define AngleCtr_limitsERP 0.6f /* 0.2f */
#define AngleCtr_tol (5.0f*fTO_RAD)
    
    m3f frameAw = rbA.R * frameA;
    m3f frameBw = rbB.R * frameB;
    v3f xA = frameAw.col[0];
    zA = frameAw.col[2];
    xB = frameBw.col[0];
    yB = frameBw.col[1];
    
    //
    // Rotation constraint
    //
    
    // Position constraint, C = 0
    float Cu = zA.dot(xB);
    float Cv = zA.dot(yB);
    // bias
    biasu = Cu * AngleCtr_ERP * dt_inverse;
    biasv = Cv * AngleCtr_ERP * dt_inverse;
    
    // Effective mass
    v3f cru = xB % zA;
    v3f crv = yB % zA;
    m3f iI_w_sum = rbA.iI_w + rbB.iI_w;
    iKu = 1.0f/( cru.dot(iI_w_sum * cru) );
    iKv = 1.0f/( crv.dot(iI_w_sum * crv) );
    
    //
    // Angle limits constraint
    //
    
    if (limits_active)
    {
        
        // Angle
        // No need to transform bases (as in conetwist),
        // because the z-axes should be aligned by design of this ctr
        float theta = acos( xB.dot(xA) );
        float theta_err = theta - theta_max;
        
        // Activate
        limits_ctr_active = (theta_err > 0);
        
        // Constraint vector = xA % aB
        // Undefined for small angles, but then limits are not active
        v_limits = -xA % xB;
        
        // Position constraint: C = 0
        float C_limits = -theta_err + AngleCtr_tol; /* - init_theta */;
        bias_limits = C_limits * AngleCtr_limitsERP * dt_inverse;
        
        // Inverse effective mass
        iK_limits = 1.0f/( v_limits.dot(iI_w_sum * v_limits) );
        
        lambda_limits_acc = 0;
    }
}

void AngleConstraint::solve(float dt,
                            float dt_inverse,
                            RigidBody3dComponent& rbA,
                            RigidBody3dComponent& rbB)
{
    v3f P_rotation = {0,0,0};
    v3f P_limits = {0,0,0};
    
    //
    // Rotation constraint
    //
    
    // Velocity constraint, Cdot = Jv = 0
    float Cdotu = (xB % zA).dot(rbB.W - rbA.W);
    float Cdotv = (yB % zA).dot(rbB.W - rbA.W);
    // impulse, lambda = -(Jv+bias)/(J*M^-1*J^T)
    float lambdau = -(Cdotu + biasu)*iKu;
    float lambdav = -(Cdotv + biasv)*iKv;
    
    // Create impulses, P = J^T lambda
    v3f Pu = (zA % xB) * lambdau;
    v3f Pv = (zA % yB) * lambdav;
    P_rotation = Pu + Pv;
    
    //
    // Angle limits constraint
    //
    
    if (limits_active && limits_ctr_active)
    {
        // Velocity constraint
        float Cdot_limits = (rbB.W - rbA.W).dot(v_limits);
        float lambda_limits = -(Cdot_limits + bias_limits)*iK_limits;
        
        // Impulse magnitude
        float lambda_limits_tot;
        
        // Constraint is unilateral to clamp
        lambda_limits_tot = clamp(lambda_limits_acc + lambda_limits, 0.0f, (float)fINF) - lambda_limits_acc;
        lambda_limits_acc += lambda_limits_tot;
        
        // Create impulse, P = J^T lambda
        P_limits = -v_limits * lambda_limits_tot;
    }
    
    // Apply impulse
    v3f P = P_rotation + P_limits;
    RigidBody::apply_angular_impulse(rbA, P);
    RigidBody::apply_angular_impulse(rbB, -P);
    //        bodyA->apply_angular_impulse(P);
    //        bodyB->apply_angular_impulse(-P);
}
