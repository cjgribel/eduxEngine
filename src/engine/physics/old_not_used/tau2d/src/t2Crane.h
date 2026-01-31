
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2DCRANE_H
#define T2DCRANE_H
#pragma once

#include "t2World.h"
#include "t2Body.h"
#include "t2Joint.h"
#include "t2Constraint.h"

/*
	articulated crane
*/
class t2Crane
{
public:

	t2Crane(vec2f pos, t2World *world);

	void event_KeyDown(unsigned char key);

	void event_KeyUp(unsigned char key);

private:

	t2World *world;
	t2LinearActuatorJoint
		*lower_actuator,
		*upper_actuator,
		*bucket_actuator;
	float
		lower_act_force,
		upper_act_force,
		bucket_act_force;
};

#endif