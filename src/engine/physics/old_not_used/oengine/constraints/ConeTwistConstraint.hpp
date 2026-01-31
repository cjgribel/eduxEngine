//
//  ConeTwistConstraint.hpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-08-25.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef ConeTwistConstraint_hpp
#define ConeTwistConstraint_hpp

#include "vec.h"
#include "mat.h"
#include "RigidBody.hpp"

using namespace linalg;
using namespace RigidBody;

class ConeTwistConstraint //: public angular_constraint_t
{
public:
    
    ConeTwistConstraint(const vec3f &rA = v3f_000,
                        const vec3f &rB = v3f_000)
    : rA(rA), rB(rB)
    {
        
    }
    
    // Local anchor points - these are not needed or used, but can be useful
    // when debug drawing since they locate the constraint in space
    vec3f rA, rB;
    
    // Local frames
    m3f frameA = m3f_1;
    m3f frameB = m3f_1;
    
    m3f frameAw;                                // World frames
    m3f frameBw;
    
    v3f Axw, Azw, Byw, Bzw;                     // World frame axes
    v3f Ayw, Bxw;
    
    float cone_max = 0;                         // Angle limits
    float twist_max = 0;
    
    v3f v_cone, v_twist;                        // Constraint vectors
    float iK_cone, iK_twist;                    // Effective mass
    float bias_cone, bias_twist;                // Baumgarte bias
    float lambda_cone_acc, lambda_twist_acc;    // Impulse accumulation
    bool cone_ctr_active, twist_ctr_active;     // Activation flags
    
    //linear_constraint_t* pos_ctr = nullptr;     // Optional ptr to a accompanying
    // positional constraint,
    // providing a reference point
    
    void set_limits(float cone_max,
                    float twist_max,
                    const m3f &frameA = m3f_1,
                    const m3f &frameB = m3f_1);
    
    void pre_solve(float dt_inverse,
                   const RigidBody3dComponent& rbA,
                   const RigidBody3dComponent& rbB);
    
    void solve(float dt,
               float dt_inverse,
               RigidBody3dComponent& rbA,
               RigidBody3dComponent& rbB);
};

#endif /* ConeTwistConstraint_hpp */
