//
//  constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-21.
//
//

#ifndef DISTANCE_CONSTRAINT_H
#define DISTANCE_CONSTRAINT_H

#include "constraint.h"
#include "vec.h"
#include "mat.h"
//#include "quat.h"
#include "math.h"
#include "body.h"

//
// Constrain distance between anchor points
//
class distance_constraint_t : public linear_constraint_t
{
    
    distance_constraint_t(body_t *bodyA, body_t *bodyB, const vec3f &rA, const vec3f &rB) : linear_constraint_t(bodyA, bodyB, rA, rB)
    {
        
    }
    
    void pre_solve() {}
    
    void solve() {}
};

#endif // DISTANCE_CONSTRAINT_H
