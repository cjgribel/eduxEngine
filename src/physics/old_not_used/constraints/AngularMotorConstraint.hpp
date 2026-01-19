//
//  AngularMotorConstraint.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-01.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef AngularMotorConstraint_hpp
#define AngularMotorConstraint_hpp

#include "vec.h"
#include "mat.h"
#include "RigidBody.hpp"

using namespace linalg;
using namespace RigidBody;

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
class AngularMotorConstraint //: public angular_constraint_t
{
public:
    // Local frames
    //    mat3f frameA = linalg::mat3f_identity;
    //    mat3f frameB = linalg::mat3f_identity;
    
    // Local anchor points - these are not need or used, but can be useful
    // when debug drawing since they locate the constraint in space
    v3f rA, rB;
    
    vec3f u, uw;             // axle vector
private:
    float iK;               // effective mass
    float lambda_acc;       // accumulated torque over a set of solver steps
    
    float T_max = 0;     // max torque
    float V_target = 0;     // target angular velocity
    bool enabled = false;
    
public:
    AngularMotorConstraint(const v3f& rA = v3f_000,
                           const v3f& rB = v3f_000)
    : rA(rA), rB(rB)
    {
        
    }
    
    void enable(float T_max,
                float V_target);
    
    void enable();
    
    void disable();
    
    float get_applied_torque() { return lambda_acc; }
    
    void pre_solve(float dt_inverse,
                   const RigidBody3dComponent& rbA,
                   const RigidBody3dComponent& rbB);
    
    void solve(float dt,
               float dt_inverse,
               RigidBody3dComponent& rbA,
               RigidBody3dComponent& rbB);
};

#endif /* AngularMotorConstraint_hpp */
