//
//  ballsocket_constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-30.
//
//

#ifndef tau3d_ballsocket_constraint_h
#define tau3d_ballsocket_constraint_h

#include "constraint.h"

//
// Ball-and-Socket constraint
// Removes all linear dof's from rel motion of bodies
//
// Position level:
// C = pB + rB - pA - rA = 0
//
// Velocity level:
// Cdot   = d/dt C
//        = vB + wB x rB - vA - wA x rA
//        = vB - [rB]*wB - vA + [rA]*wA
//            | -I3  |T | vA |
//        =   | [rA] |  | wA |
//            |  I3  |  | vB |
//            | -[rB]|  | wB |
//        = Jv
//        = 0
//
// Constraint mass matrix
// K  = J * M^(-1) * J^T
//    = 1/mA*I3 + [rA]*IiA*[rA]^T+ 1/mB*I3 + [rB]*IiB*[rB]^T
//
// Derivation: page 12
// http://danielchappuis.ch/download/ConstraintsDerivationRigidBody3D.pdf
//

class ballsocket_constraint_t : public linear_constraint_t
{
private:
    mat3f iK;
    vec3f bias;
    mat3f skewA, skewB;
    
public:
    vec3f rAw, rBw;  // world pivot points
    
    ballsocket_constraint_t(body_t *bodyA, body_t *bodyB, const vec3f &rA, const vec3f &rB) : linear_constraint_t(bodyA, bodyB, rA, rB)
    {
    }
    
    void pre_solve()
    {
#define ballsocket_ctr_ERP 0.2f
        
        // World anchor points
        rAw = bodyA->R * rA;
        rBw = bodyB->R * rB;
        
        // Bias
        vec3f C = bodyB->X + rBw - bodyA->X - rAw;
        bias = C * (ballsocket_ctr_ERP * SIM_IDT);
        
        // Effective mass
        skewA = mat3f::skew(rAw);
        mat3f skewAT = skewA; skewAT.transpose();
        skewB = mat3f::skew(rBw);
        mat3f skewBT = skewB; skewBT.transpose();
        iK = ( mat3f(bodyA->im) + mat3f(bodyB->im) + skewA*bodyA->iI_w*skewAT + skewB*bodyB->iI_w*skewBT ).inverse();
    }
    
    void solve()
    {
        // Velocity constraint Cdot = Jv = 0
        vec3f Cdot = bodyB->V - skewB*bodyB->W - bodyA->V + skewA*bodyA->W;
        
        // Setup and apply impulse P = J^T lambda
        // This is lambda; J^T is applied via apply_impulse on both bodies
        vec3f P = iK * (Cdot + bias);
        bodyA->apply_impulse(P, rAw);
        bodyB->apply_impulse(-P, rBw);
    }
};

#endif
