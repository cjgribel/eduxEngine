//
//  DistanceConstraint.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-08-29.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef DistanceConstraint_hpp
#define DistanceConstraint_hpp

#include <iostream>
#include "vec.h"
#include "mat.h"
#include "RigidBody.hpp"

using namespace linalg;
using namespace RigidBody;

class DistanceConstraint
{
public:
    
    DistanceConstraint(const v3f &rA,
                       const v3f &rB,
                       float L) :
    rA(rA), rB(rB), L(L) { }
        
    void pre_solve(float dt_inverse,
                   const RigidBody3dComponent& rbA,
                   const RigidBody3dComponent& rbB);
        
    void solve(float dt,
               float dt_inverse,
               RigidBody3dComponent& rbA,
               RigidBody3dComponent& rbB);
    
//private:
    v3f rA, rB;
private:
    float L;
    v3f rAw, rBw;   // Anchor points in world space
    v3f un;         // Normalized anchor points vector
    float bias;     // Correction term
    float iK;       // Inverse constraint effective mass
};

#endif /* DistanceConstraint_hpp */
