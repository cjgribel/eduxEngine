//
//  LinearMotorConstraint.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-01.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef LinearMotorConstraint_hpp
#define LinearMotorConstraint_hpp

#include "vec.h"
#include "mat.h"
#include "RigidBody.hpp"

using namespace linalg;
using namespace RigidBody;

// Linear motor with operational states
// Modelled as a hydraulic ram controlled by an incompressible fluid:
// i.e. it does not allow movement opposite to the operational direction.
//

enum LinearMotorConstraintMode
{
    Hold,       // Acts as a distance constraint
    Extend,     // Linear motor forward, distance constraint backward
    Contract    // Opposite of EXTEND
};

class LinearMotorConstraint
{
    v3f rA, rB;                 // Local anchor points
    v3f rAw, rBw;               // World anchor points
    v3f un;                     // Anchor points direction
    float bias;                 // bias term (for position correction)
    float lambdaAcc_vel;
    float iK;                   // Inverse effective mass
    
    float L_min, L_max, L_target;
    float V_max, F_max, V_target;
    float F_low_limit, F_high_limit;
    
    bool F_hasLimits;
    LinearMotorConstraintMode act_mode;
    
    float ERP = 0.15f;
    using FloatLimits = std::numeric_limits<float>;
    
public:
    
    /**
     Linear motor with operational states
     @param rA Anchor point in local space of rigid body A
     @param rB Anchor point in local space of rigid body B
     @param L_min Min length
     @param L_max Max length
     @param L_init Initial length
     @param V_max Target velocity
     @param F_max Max driving force
     */
    
    LinearMotorConstraint(const v3f& rA,
                          const v3f& rB,
                          float L_min,
                          float L_max,
                          float L_init,
                          float V_max,
                          float F_max);
    
    void setMode(const RigidBody3dComponent& rbA,
                 const RigidBody3dComponent& rbB,
                 LinearMotorConstraintMode mode);
    
    void pre_solve(float dt_inverse,
                   const RigidBody3dComponent& rbA,
                   const RigidBody3dComponent& rbB);
    
    void solve(float dt,
               float dt_inverse,
               RigidBody3dComponent& rbA,
               RigidBody3dComponent& rbB);
};

#endif /* LinearMotorConstraint_hpp */
