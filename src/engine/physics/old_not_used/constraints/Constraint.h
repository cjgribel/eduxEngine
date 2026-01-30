//
//  constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-21.
//
//

#ifndef tau3d_constraint_h
#define tau3d_constraint_h

#include "vec.h"
#include "mat.h"
#include "quat.h"
#include "math_.h"
#include "body.h"

#if 0
//
// Two-body positional or angular constraint
//
class constraint_t
{
public:
    
    body_t *bodyA, *bodyB;
    
    constraint_t(body_t *bodyA,
                 body_t *bodyB)
    : bodyA(bodyA), bodyB(bodyB)
    {
        
    }
    
    virtual void pre_solve() = 0;
    
    virtual void solve() = 0;
    
    // DBG
    virtual void render() {}
};

//
// Positional constraint
//
class linear_constraint_t : public constraint_t
{
public:
    
    // Local anchor points
    vec3f rA, rB;
    
    linear_constraint_t(body_t *bodyA,
                        body_t *bodyB,
                        const vec3f &rA,
                        const vec3f &rB)
    : constraint_t(bodyA, bodyB), rA(rA), rB(rB)
    {
        
    }
};

//
// Angular constraint
//
class angular_constraint_t : public constraint_t
{
public:
    
    // Local frames
    mat3f frameA = linalg::mat3f_identity;
    mat3f frameB = linalg::mat3f_identity;
    
    angular_constraint_t(body_t *bodyA,
                         body_t *bodyB)
    : constraint_t(bodyA, bodyB)
    {

    }
};

#endif

#endif
