//
//  angular_motor_constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-27.
//
//

#ifndef tau3d_angular_motor_constraint_h
#define tau3d_angular_motor_constraint_h

#include "constraint.h"

//
//
// u = motor axle wrt to body A
//
// Cdot   = Jv
//        = u.(wA - wB)
//        =   | 0 | | vA |
//            | u | | wA |
//            | 0 | | vB |
//            |-u | | wB |
//
// Inverse effective mass K   = J M^-1 J^T
//                            = J.( 0 IiA*u 0 -IiB*u )
//                            = u.(IiA*u) + u.(IiB*u)
//                            = u.( (IiA + IiB)*u )
// Effective mass iK = 1/K
//
class angular_motor_constraint_t : public angular_constraint_t
{
public:
    vec3f u, uw;             // axle vector
private:
    float iK;               // effective mass
    float lambda_acc;       // accumulated torque over a set of solver steps
    
    float T_max = 0;     // max torque
    float V_target = 0;     // target angular velocity
    bool enabled = false;
    
public:
    angular_motor_constraint_t(body_t *bodyA, body_t *bodyB) : angular_constraint_t(bodyA, bodyB)
    {
        
    }
    
    void enable(float T_max, float V_target)
    {
        this->T_max = T_max;
        this->V_target = V_target;
        enable();
    }
    
    void enable() { enabled = true; }
    
    void disable() { enabled = false; }
    
    void pre_solve()
    {
        if (!enabled) return;
        
        // axle in world space
        u = {0,0,1};
        uw = bodyA->R*u;
        
        // effective mass
        iK = 1.0f/( uw.dot((bodyA->iI_w + bodyB->iI_w)*uw) );
        
        // reset accumulated torque
        lambda_acc = 0;
    }
    
    void solve()
    {
        if (!enabled) return;
        
        // velocity constraint
        float Cdot = uw.dot(bodyA->W-bodyB->W) - V_target;
        
        // impulse magnitude
        float lambda = -Cdot * iK * SIM_IDT; // integrate to force level, so limits can be expressed as torques
        lambda = clamp(lambda_acc + lambda, -T_max, T_max) - lambda_acc;
        lambda_acc += lambda;
        
        // apply angular impulse
        vec3f P = uw * lambda * SIM_DT; // back to velocity level
        bodyA->W += bodyA->iI_w * P;
        bodyB->W -= bodyB->iI_w * P;
    }
};

#endif
