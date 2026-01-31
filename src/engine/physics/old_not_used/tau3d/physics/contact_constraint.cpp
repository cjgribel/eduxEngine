//
//  contact_constraint.cpp
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-08.
//
//

#include "contact_constraint.h"

/*
 contact constraint effective mass, iK = 1 / J M^-1 J^T
 M contains the mass matrix (3x3) and inertia tensor (3x3) for body A and B -> dimeension 12x12
 M^-1: since M is a block matrix, M^-1 is the same matrix with the inverse of each respective block
 J is the Jacobian, for a contact constraint (no normal velocity) it contains the contact normal nc, rA x nc andrB x nc
 
 friction:
 http://en.wikipedia.org/wiki/Collision_response
 */

void contact_point_t::pre_solve(body_t *bodyA, body_t *bodyB, contact_point_t* c_warm, float h)
{
    // Anchor points in local space
    rA = cp - bodyA->X;
    rB = cp - bodyB->X;
    vec3f rAdot = bodyA->V + bodyA->W % rA, rBdot = bodyB->V + bodyB->W % rB;
    vec3f rABdot = (rAdot - rBdot)*cndir;
    // Relative velocity in normal direction
    float rABdot_n = rABdot.dot(cn);
    // Clamp to tolerance
    float rABdot_n_cl = clamp(rABdot_n - VEL_TOL, 0.0f, INFINITY);
    
    //
    // Normal direction:
    // Constraint: zero relative velocity
    // Bias: restitution & error correction
    //
    
    // Bias: restitution
    bias_n = bodyA->restitution * bodyB->restitution * rABdot_n_cl;
    
    // Bias: position correction
    float C_n = clamp<float>(-dist - PENET_TOL, 0.0f, fINF);
    bias_n += ERP_n * SIM_IDT * C_n;
    
    // Precompute effective mass
    vec3f crAcn = rA % cn;
    vec3f crBcn = rB % cn;
    iK_n = 1.0f/( bodyA->im + bodyB->im + (crAcn*bodyA->iI_w).dot(crAcn) + (crBcn*bodyB->iI_w).dot(crBcn) );
    
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
    gamma = 1.0f/(c + SIM_DT*k); // = CFM
    beta = SIM_DT*k/(c + SIM_DT*k); // = ERP
    // override bias
    //gamma = 0.1;
    //beta = 0.15;
    bias_n = beta*SIM_IDT*C_n;
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
    vec3f ct = rABdot - cn * rABdot_n;
    ct.normalize(); // Todo: handle when length = 0
    
    // Set up tangent basis
    // Todo: handle when |ct| < eps)
    ct0 = ct;
    ct1 = ct % cn;
    
    // Precompute effective masses in each tangent direction
    vec3f crAct0 = rA % ct0;
    vec3f crBct0 = rB % ct0;
    iK_t0 = 1.0f/(bodyA->im + bodyB->im + (crAct0*bodyA->iI_w).dot(crAct0) + (crBct0*bodyB->iI_w).dot(crBct0));
    vec3f crAct1 = rA % ct1;
    vec3f crBct1 = rB % ct1;
    iK_t1 = 1.0f/(bodyA->im + bodyB->im + (crAct1*bodyA->iI_w).dot(crAct1) + (crBct1*bodyB->iI_w).dot(crBct1));
    
    lambda_t0_acc = 0;
    lambda_t1_acc = 0;
    
    // reset accumulated impulses
    lambda_n_vel_acc = 0;
    lambda_n_bias_acc = 0;
    lambda_n_tot_acc = 0;
    
    // warm starting: apply solution from previous iteration

    if (c_warm)
    {
        const float warm_frac = WARM_START_RELAXATION;
        float ln = c_warm->lambda_n_tot_acc * warm_frac;
        float lt0 = c_warm->lambda_t0_acc * warm_frac;
        float lt1 = c_warm->lambda_t1_acc * warm_frac;
        
        vec3f P_n = cn * ln;
        bodyA->apply_impulse(-P_n*cndir, c_warm->rA);
        bodyB->apply_impulse(P_n*cndir, c_warm->rB);
        
        vec3f P_t = ct0 * lt0 + ct1 * lt1;
        bodyA->apply_impulse(-P_t*cndir, c_warm->rA);
        bodyB->apply_impulse(P_t*cndir, c_warm->rB);
        
        lambda_n_tot_acc = ln;
        lambda_t0_acc = lt0;
        lambda_t1_acc = lt1;
    }
}

void contact_point_t::solve(body_t *bodyA, body_t *bodyB, float h)
{
    // Anchor points in local space
    vec3f rAdot = bodyA->V + bodyA->W % rA, rBdot = bodyB->V + bodyB->W % rB;
    vec3f rABdot = (rAdot - rBdot)*cndir;
    
    //
    // Normal direction
    //
    // Constraint = zero relative velocity
    // bias = restitution & error correction
    //
    
    // Velocity constraint impulse + bias
    float rABdot_n = rABdot.dot(cn);
    //float lambda_n_vel = iK_n * (rABdot_n + 0);
    float lambda_n_vel = iK_n * (rABdot_n - gamma*lambda_n_tot_acc);
    float lambda_n_bias = iK_n * bias_n;
    float lambda_n_tot = clamp(lambda_n_tot_acc + lambda_n_vel + lambda_n_bias, 0.0f, INFINITY) - lambda_n_tot_acc;
    // Accumulate impulse
    lambda_n_tot_acc += lambda_n_tot;
    
    // Create and apply normal impulse
    vec3f P_n = cn * lambda_n_tot;
    bodyA->apply_impulse(-P_n*cndir, rA);
    bodyB->apply_impulse(P_n*cndir, rB);
    
    //
    // Tangent direction:
    //
    // Constraint: friction
    // Bias: traction
    //
    
    // Relative velocity in tangent direction
    vec3f ct = rABdot - cn * rABdot_n;
    ct.normalize(); // TODO: handle when length = 0
    
    // Components in tangent basis
    float rABdot_t0 = rABdot.dot(ct0);
    float rABdot_t1 = rABdot.dot(ct1);
    float lambda_t0 = iK_t0 * rABdot_t0;
    float lambda_t1 = iK_t1 * rABdot_t1;
    
    // Friction limits
    float maxFriction_d = (bodyA->my_d+bodyB->my_d)*0.5f * lambda_n_tot_acc;
    
    // Clamp and and accumulate
    lambda_t0 = clamp(lambda_t0 + lambda_t0_acc, -maxFriction_d, maxFriction_d) - lambda_t0_acc;
    lambda_t0_acc += lambda_t0;
    lambda_t1 = clamp(lambda_t1 + lambda_t1_acc, -maxFriction_d, maxFriction_d) - lambda_t1_acc;
    lambda_t1_acc += lambda_t1;
    
    // Create and apply tangent impulses
    vec3f P_t = ct0 * lambda_t0 + ct1 * lambda_t1;
    bodyA->apply_impulse(-P_t*cndir, rA);
    bodyB->apply_impulse(P_t*cndir, rB);
}
