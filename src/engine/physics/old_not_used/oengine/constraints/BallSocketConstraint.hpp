//
//  BallSocketConstraint.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-08-25.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef BallSocketConstraint_hpp
#define BallSocketConstraint_hpp

#include "vec.h"
#include "mat.h"
#include "RigidBody.hpp"

using namespace linalg;
using namespace RigidBody;

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

class BallSocketConstraint // : public linear_constraint_t
{
private:
    m3f iK;
    v3f bias;
    m3f skewA, skewB;
    
public:
    v3f rA, rB;     // Local anchor points
    v3f rAw, rBw;   // World anchor points
    
    BallSocketConstraint(const v3f &rA,
                         const v3f &rB)
    : rA(rA), rB(rB)
    {
    }
    
    void pre_solve(float dt_inverse,
                   const RigidBody3dComponent& rbA,
                   const RigidBody3dComponent& rbB);
    
    void solve(float dt,
               float dt_inverse,
               RigidBody3dComponent& rbA,
               RigidBody3dComponent& rbB);
};

#endif /* BallSocketConstraint_hpp */
