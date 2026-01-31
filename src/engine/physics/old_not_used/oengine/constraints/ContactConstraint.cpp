//
//  contact_constraint.cpp
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-08.
//
//

#include <iostream>
#include "ContactConstraint.hpp"

/*
 contact constraint effective mass, iK = 1 / J M^-1 J^T
 M contains the mass matrix (3x3) and inertia tensor (3x3) for body A and B -> dimeension 12x12
 M^-1: since M is a block matrix, M^-1 is the same matrix with the inverse of each respective block
 J is the Jacobian, for a contact constraint (no normal velocity) it contains the contact normal nc, rA x nc andrB x nc
 
 friction:
 http://en.wikipedia.org/wiki/Collision_response
 */

void ContactPoint<3>::pre_solve(const EntityPairType& rb_pair,
                                entt::registry& registry,
                                const ContactPoint<3>* cp_warm,
                                float h)
{
    auto& rbA = registry.get<RigidBody::RigidBody3dComponent>(rb_pair.first);
    auto& rbB = registry.get<RigidBody::RigidBody3dComponent>(rb_pair.second);
    
    // Contact normal that will point collider A -> collider B
    const v3f cneff = cn * cndir;
    // Anchor points in local space
    rA = cp - rbA.X;
    rB = cp - rbB.X;
    // Anchor point velocities in local space
    const v3f rAdot = rbA.V + cross(rbA.W, rA);
    const v3f rBdot = rbB.V + cross(rbB.W, rB);
    // Relative velocity collider A -> collider B
    const v3f rABdot = rBdot - rAdot;
    // Relative velocity in normal direction; is <0 if colliders are approaching
    const float rABdot_n = dot(rABdot, cneff);
    // Clamp to tolerance
    const float rABdot_n_cl = clamp(-rABdot_n - VelocityTolerance3d,
                                    0.0f,
                                    XI_FINF);
    
    //
    // Normal direction:
    // Constraint: zero relative velocity
    // Bias: restitution & error correction
    //
    
    // Bias: restitution
    bias_n = rbA.restitution * rbB.restitution * rABdot_n_cl;
    
    // Bias: position correction
    float C_n = clamp<float>(-dist - PenetrationTolerance3d,
                             0.0f,
                             XI_FINF);
    bias_n += ERP3d * 1.0f/h * C_n;
    
    // Precompute effective mass
    v3f crAcn = cross(rA, cneff);
    v3f crBcn = cross(rB, cneff);
    iK_n = 1.0f/(rbA.im +
                 rbB.im +
                 dot(crAcn*rbA.iI_w, crAcn) +
                 dot(crBcn*rbB.iI_w, crBcn));
    
//#define SOFTCONSTRAINT
#ifdef SOFTCONSTRAINT
    // Experimentation: Soft constraint parameters
    float omega = 2;    // Angular frequency
    float zeta= 0.25;      // Damping ratio
    //    float omega = 6;    // Angular frequency
    //    float zeta= 0.15;      // Damping ratio
    //    if (bodyA->is_static && bodyB->X.x > 0) { omega = 4; zeta = 0.75f; }
    //    else if (bodyB->is_static && bodyA->X.x > 0) { omega = 4; zeta = 0.75f; }
    //
    float k = iK_n * omega*omega;   // Spring constant
    float c = 2.0f*iK_n*zeta*omega;    // Damping constant
    gamma = 1.0f/(c + h*k); // = CFM
    beta = h*k/(c + h*k); // = ERP
    // override bias
    //gamma = 0.1;
    //beta = 0.15;
    bias_n = beta*1.0f/h*C_n;
    //printf("gamma %f, beta %f\n", gamma, beta);
#else
    gamma = 0;
#endif
    
    //
    // Tangent direction:
    // Constraint = friction
    // Bias = traction
    //
    
    // Bias: traction
    bias_t = 0;
    
    // Relative velocity in tangent direction
    v3f ct = rABdot - cneff * rABdot_n;
    ct.normalize();
    
    // Set up tangent basis
    // Todo: handle when |ct| < eps)
    ct0 = ct;
    ct1 = cross(ct, cneff);
    
    // Precompute effective masses in each tangent direction
    v3f crAct0 = cross(rA, ct0);
    v3f crBct0 = cross(rB, ct0);
    iK_t0 = 1.0f/(rbA.im +
                  rbB.im +
                  dot(crAct0 * rbA.iI_w, crAct0) +
                  dot(crBct0 * rbB.iI_w, crBct0));
    v3f crAct1 = cross(rA, ct1);
    v3f crBct1 = cross(rB, ct1);
    iK_t1 = 1.0f/(rbA.im +
                  rbB.im +
                  dot(crAct1 * rbA.iI_w, crAct1) +
                  dot(crBct1 * rbB.iI_w, crBct1));
    
    lambda_t0_acc = 0;
    lambda_t1_acc = 0;
    
    // reset accumulated impulses
    lambda_n_vel_acc = 0;
//    lambda_n_bias_acc = 0;
    lambda_n_tot_acc = 0;
    
//    lambda_n_tot_acc_unbiased = 0;
    
//    if (cp_warm && cp_warm->id.flip != id.flip) {
//        std::cout << "     " << *this;
//        std::cout << "warm " << *cp_warm << std::endl;
//    }
    
    // Warm starting: apply final impulse from last frame.
    if (cp_warm)
    {
        float ln = cp_warm->lambda_n_tot_acc * WarmStartRelaxation3d;
        float lt0 = cp_warm->lambda_t0_acc * WarmStartRelaxation3d;
        float lt1 = cp_warm->lambda_t1_acc * WarmStartRelaxation3d;
        
        const v3f P_n = cneff * ln;
        RigidBody::apply_impulse(rbA, -P_n, rA);
        RigidBody::apply_impulse(rbB, P_n, rB);
        
        const v3f P_t = ct0 * lt0 + ct1 * lt1;
        RigidBody::apply_impulse(rbA, P_t, rA);
        RigidBody::apply_impulse(rbB, -P_t, rB);
        
        lambda_n_tot_acc = ln;
        lambda_t0_acc = lt0;
        lambda_t1_acc = lt1;
        
        age = cp_warm->age;
    }
}

//void ContactPoint3d::solve(RigidBody::RigidBody3dComponent& rbA,
//                         RigidBody::RigidBody3dComponent& rbB,
//                         float h)
void ContactPoint<3>::solve(const EntityPairType& rb_pair,
                           entt::registry& registry,
                           float h)
{
    auto& rbA = registry.get<RigidBody::RigidBody3dComponent>(rb_pair.first);
    auto& rbB = registry.get<RigidBody::RigidBody3dComponent>(rb_pair.second);
    
    // Contact normal that will point collider A -> collider B
    const v3f cneff = cn * cndir;
    // Anchor point velocities in local space
    const v3f rAdot = rbA.V + rbA.W % rA;
    const v3f rBdot = rbB.V + rbB.W % rB;
    // Relative velocity collider A -> collider B
    const v3f rABdot = rBdot - rAdot;
    
    //
    // Normal direction
    //
    // Constraint = zero relative velocity
    // bias = restitution & error correction
    //
    
    // Velocity constraint impulse + bias
    
    // Relative velocity in normal direction; is <0 if colliders are approaching
    const float rABdot_n = dot(rABdot, cneff);
    // Magnitude of velocity constraint impulse
    // This impulse may be <0 or >0 at different iterations, but the total
    // accumulated impulse will be >0.
    const float lambda_n_vel = iK_n * (-rABdot_n - gamma*lambda_n_tot_acc);
    // Bias term of the constraint
    const float lambda_n_bias = iK_n * bias_n;
    // Total impulse magnitude
    const float lambda_n_tot = clamp(lambda_n_tot_acc + lambda_n_vel + lambda_n_bias,
                                     0.0f,
                                     XI_FINF) - lambda_n_tot_acc;
    // Accumulate impulse
    lambda_n_tot_acc += lambda_n_tot;

#if 0
    // IDEA
    // Have a second impulse accumualation which does not take bias (from
    // penetration & restitution) into account, and use it for friction.
    //
    // Friction now seems too low - but inconsistent between surfaces (!)
    // For a box on a static surface, it seems to be almost zero,
    // while it seems ok on another non-static surface. Note that static
    // objects are not subject to gravity
    float lambda_n_tot_unbiased = clamp(lambda_n_tot_acc_unbiased + lambda_n_vel,
                                        0.0f,
                                        XI_FINF) - lambda_n_tot_acc_unbiased;
    lambda_n_tot_acc_unbiased += lambda_n_tot_unbiased;
#endif
    
    // Create and apply normal impulse
    const v3f P_n = cneff * lambda_n_tot;
    RigidBody::apply_impulse(rbA, -P_n, rA);
    RigidBody::apply_impulse(rbB, P_n, rB);
    
    //
    // Tangent direction:
    //
    // Constraint: friction
    // Bias: traction
    //
    
    // Relative velocity in tangent direction
//    v3f ct = rABdot - cn * rABdot_n;
//    ct.normalize(); // TODO: handle when length = 0
    
    // Components in tangent basis
    float rABdot_t0 = dot(rABdot, ct0);
    float rABdot_t1 = dot(rABdot, ct1);
    float lambda_t0 = iK_t0 * rABdot_t0;
    float lambda_t1 = iK_t1 * rABdot_t1;
    
    // Friction limits
    const float maxFriction_d = fmin(rbA.my_d, rbB.my_d) * lambda_n_tot_acc;
//    float maxFriction_d = fmin(rbA.my_d, rbB.my_d) * lambda_n_tot_acc_unbiased;
    //    float maxFriction_d = (rbA.my_d+rbB.my_d)*0.5f * lambda_n_tot_acc;
    
    // Clamp and and accumulate
    lambda_t0 = clamp(lambda_t0 + lambda_t0_acc,
                      0.0f, // -maxFriction_d
                      maxFriction_d) - lambda_t0_acc;
    
    lambda_t1 = clamp(lambda_t1 + lambda_t1_acc,
                      0.0f, // -maxFriction_d
                      maxFriction_d) - lambda_t1_acc;
    
    // "Normalize" the tangent magnitudes to the allowed length
//    float f = sqrt(maxFriction_d*maxFriction_d) / sqrt(lambda_t0*lambda_t0 + lambda_t1*lambda_t1);
//    if (f < 1.0f) {
//        lambda_t0 *= f;
//        lambda_t1 *= f;
////        std::cout << f << "_" << sqrt(lambda_t0*lambda_t0 + lambda_t1*lambda_t1)/sqrt(maxFriction_d*maxFriction_d) << ", ";
//    }
    lambda_t0_acc += lambda_t0;
    lambda_t1_acc += lambda_t1;
    
    // Create and apply tangent impulses
    const v3f P_t = ct0 * lambda_t0 + ct1 * lambda_t1;
//    if (P_t == P_t) {
    RigidBody::apply_impulse(rbA, P_t, rA);
    RigidBody::apply_impulse(rbB, -P_t, rB);
//    }
}

void ContactPoint<2>::pre_solve(const EntityPairType& rb_pair,
                               entt::registry& registry,
                               const ContactPoint<2>* cp_warm,
                               float h)
{
    auto& rbA = registry.get<RigidBody::RigidBody2dComponent>(rb_pair.first);
    auto& rbB = registry.get<RigidBody::RigidBody2dComponent>(rb_pair.second);
    
    // Contact normal that will point collider A -> collider B
    const v2f cneff = cn * cndir;
    // Anchor points
    rA = cp - rbA.X;
    rB = cp - rbB.X;
    // Anchor points velocities: rdot = v + w x r
    const v2f rAdot = rbA.V + cross((float)rbA.W, rA);
    const v2f rBdot = rbB.V + cross((float)rbB.W, rB);
    // Anchor points relative velocity
    const v2f rABdot = rBdot - rAdot;

    // Normal constraint (non-penetration)

    // Normal effective mass
    const float crAcn = cross(rA, cneff);
    const float crBcn = cross(rB, cneff);
    const float K_n = rbA.im + rbA.iI * crAcn * crAcn + rbB.im + rbB.iI * crBcn * crBcn;
    iK_n = 1.0f / K_n;

    // Normal bias (position correction and restitution)
    const float C_n = clamp<float>(dist - PenetrationTolerance2d,
                                   0.0f,
                                   XI_FINF);
    rABdot_n_initial = dot(rABdot, cneff);
    const float rABdot_n_cl = clamp(-rABdot_n_initial - VelocityTolerance2d,
                                    0.0f,
                                    XI_FINF);
    bias_n = ERP2d * 1.0f/h * C_n + rbA.restitution * rbB.restitution * rABdot_n_cl;

    // Tangential constraint (friction)

    /* tangential effective mass */
//    ct = normalize(rABdot - cneff * rABdot_n_initial);
    ct = rABdot - cneff * rABdot_n_initial;
    rABdot_t_initial = length(ct);
    ct = normalize(ct);
    const float crAct = cross(rA, ct);
    const float crBct = cross(rB, ct);
    iK_t = 1.0f/(rbA.im + rbA.iI * crAct * crAct + rbB.im + rbB.iI * crBct * crBct);

    // Tangential bias
    bias_t = 0;


    lambdaAcc_n = 0.0f;
    lambdaAcc_t = 0.0f;

    // Warm starting: apply final impulse from last frame.
#if 1
    if (cp_warm)
    {
        float ln = cp_warm->lambdaAcc_n * WarmStartRelaxation2d;
        float lt = cp_warm->lambdaAcc_t * WarmStartRelaxation2d;

        const v2f P_n = cneff * ln;
        RigidBody::apply_impulse(rbA, -P_n, rA);
        RigidBody::apply_impulse(rbB, P_n, rB);
        
        const v2f P_t = ct * lt;
        RigidBody::apply_impulse(rbA, P_t, rA);
        RigidBody::apply_impulse(rbB, -P_t, rB);
        
        lambdaAcc_n = ln;
        lambdaAcc_t = lt;
        age = cp_warm->age;
    }
#endif
}

void ContactPoint<2>::solve(const EntityPairType& rb_pair,
                            entt::registry& registry,
                            float h)
{
    auto& rbA = registry.get<RigidBody::RigidBody2dComponent>(rb_pair.first);
    auto& rbB = registry.get<RigidBody::RigidBody2dComponent>(rb_pair.second);
    
    // Contact normal that will point collider A -> collider B
    const v2f cneff = cn * cndir;
    // Anchor points velocities: rdot = v + w x r
    const v2f rAdot = rbA.V + cross((float)rbA.W, rA);
    const v2f rBdot = rbB.V + cross((float)rbB.W, rB);
    // Anchor points relative velocity
    const v2f rABdot = rBdot - rAdot;
    // Anchor points relative normal and tangential velocities
    const float rABdot_n = dot(rABdot, cneff);
    const float rABdot_t = dot(rABdot, ct);

    // Normal constraint (restitution)

    // Normal velocity constraint
    const float Cdot_n = rABdot_n;
    float lambda_n = iK_n * (-Cdot_n + bias_n);
    // clamp lambda, add to accumulated
    lambda_n = clamp(lambdaAcc_n + lambda_n,
                     0.0f,
                     XI_FINF) - lambdaAcc_n;
    lambdaAcc_n += lambda_n;
    // Calculate and apply normal impulse
    const v2f P_n = cneff * lambda_n;
    RigidBody::apply_impulse(rbA, -P_n, rA);
    RigidBody::apply_impulse(rbB, P_n, rB);

    // Tangential constraint (friction)

    // Tangential velocity constraint
    const float Cdot_t = rABdot_t;
    // Calculate lambda, identify type of friction, add to accumulated
    float lambda_t = iK_t * (Cdot_t + bias_t);
    const float maxFriction_d = fmin(rbA.my_d, rbB.my_d)*0.5f * lambdaAcc_n;
    const float maxFriction_s = fmin(rbA.my_s, rbB.my_s)*0.5f * lambdaAcc_n;
    // If tangential impule larger than threshold for static friction,
    // clamp to bounds for dynamic friction,
    // otherwise, keep lambda to eliminate all relative tangential velocity
    if(fabs(lambda_t) > fabs(maxFriction_s))
        lambda_t = clamp(lambdaAcc_t + lambda_t,
                         -maxFriction_d,
                         maxFriction_d) - lambdaAcc_t;
    lambdaAcc_t += lambda_t;
    // Calculate and apply tangential impulse
    const v2f P_t = ct * lambda_t;
    RigidBody::apply_impulse(rbA, P_t, rA);
    RigidBody::apply_impulse(rbB, -P_t, rB);
}
