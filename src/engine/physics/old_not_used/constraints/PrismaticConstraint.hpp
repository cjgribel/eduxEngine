//
//  PrismaticConstraint.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-01.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef PrismaticConstraint_hpp
#define PrismaticConstraint_hpp

#include "vec.h"
#include "mat.h"
#include "RigidBody.hpp"

using namespace linalg;
using namespace RigidBody;

/*
 * "Prismatic/slider" constraint
 *
 * Remove two linear DoF's from rel motion of bodies (constrain to vector)
 *
 * Formulation:
 *
 * l = reference direction
 * u = joint vector = (xA + rA) - (xB + rB)
 *
 * Position level constraint: C = l.d = 0
 *
 * Velocity level constraint: Jv = 0, where
 *
 * d/dt( l.u ) =
 * dl/dt.u + l.du/dt =
 * (wA*l).u + l.(vB + wB x rB - vA - wA x rA) =
 * | -l         |T | vA |
 * | l x (u+rA) |  | wA | = Jv = 0      [wrong sign of cross product in my MSc thesis]
 * | l          |  | vB |
 * | rB x l     |  | wB |               [wrong index in my MSc thesis]
 *
 * Inverse of effective mass: iK = ( J * M^-1 * J^T )^-1
 */
class PrismaticConstraint
{
private:
    float iK, bias;
public:
    v3f rA, rB;     // Local anchor points
    v3f rAw, rBw;   // World anchor points
    v3f l, lw;      // Constraint vector
    v3f u;          // Vector between anchor points
    
    PrismaticConstraint(const vec3f &rA,
                        const vec3f &rB,
                        const vec3f &l)
    : rA(rA), rB(rB)
    {
        this->l = l;
        this->l.normalize();
    }
    
    void pre_solve(float dt_inverse,
                   const RigidBody3dComponent& rbA,
                   const RigidBody3dComponent& rbB);
    
    void solve(float dt,
               float dt_inverse,
               RigidBody3dComponent& rbA,
               RigidBody3dComponent& rbB);
};

#endif /* PrismaticConstraint_hpp */
