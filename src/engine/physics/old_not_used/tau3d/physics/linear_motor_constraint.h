//
//  constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-21.
//
//

#ifndef LINEAR_MOTOR_CONSTRAINT_H
#define LINEAR_MOTOR_CONSTRAINT_H

#include "constraint.h"
#include "vec.h"
#include "mat.h"
//#include "quat.h"
#include "math.h"
#include "body.h"

class linear_motor_constraint_t : public linear_constraint_t
{
    
};

// Mimic the t2d version
// Combination of linear motor and distance constraint
// State machine for operation modes
//
// EXTEND   Linear motor forward, distance backward (incompressible)
// CONTRACT Opposite of EXTEND
// HOLD     Distance constraint
//
class linear_actuator_t : public linear_motor_constraint_t
{
    //
    // with top & bottom
};

#endif // LINEAR_MOTOR_CONSTRAINT_H
