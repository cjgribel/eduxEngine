
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2DCLAUSFUHRERTRUCK_H
#define T2DCLAUSFUHRERTRUCK_H
#pragma once

#include "t2World.h"
#include "t2Body.h"
#include "t2Joint.h"
#include "t2Constraint.h"

class t2World;
struct vec2f;
enum GeometryType;
class t2CoilSpring;

/*
	the Claus Führer Truck
*/
class t2ClausFuhrerTruck
{
public:

	t2ClausFuhrerTruck(vec2f pos, t2World *world);

	void event_KeyDown(unsigned char key);

	void event_KeyUp(unsigned char key);

	void disassemble();

	t2Body *chassis;				// 

private:

	t2World *world;				// 

	// truck

	t2Circle
    //t2dStudWheel
                            *rear_wheel1,				// rear wheel 1
							*rear_wheel2,				// rear wheel 2
							*rear_wheel3,				// rear wheel 3
							*front_wheel;				// front wheel

	t2PrismaticConstraint	*rear_susp1,				// rear wheel joint 1
							*rear_susp2,				// rear wheel joint 2
							*rear_susp3,				// rear wheel joint 3
							*front_susp;				// front wheel joint
	t2AngularActuatorConstraint
							*rear_act1,					// rear actuator 1
							*rear_act2,					// rear actuator 2
							*rear_act3,					// rear actuator 3
							*front_act;					// front actuator

	t2CoilSpring			*rear_spring1,				// rear springdamper 1
							*rear_spring2,				// rear springdamper 2
							*rear_spring3,				// rear springdamper 3
							*front_spring;				// front springdamper
	t2LinearActuatorJoint
							*platform_actuator;			// platform actuator
	t2ElevatedHinge		*platform_joint;			// platform joint
	
	// trailer

	t2Circle					*trailer_rear_wheel1,		// rear wheel 1
								*trailer_rear_wheel2,		// rear wheel 2
								*trailer_rear_wheel3,		// rear wheel 3
								*trailer_front_wheel;		// front wheel

	t2PrismaticConstraint		*trailer_rear_susp1,		// trailer rear wheel joint 1
								*trailer_rear_susp2,		// trailer rear wheel joint 2
								*trailer_front_susp;		// trailer front wheel joint
	t2AngularActuatorConstraint
								*trailer_rear_act1,			// trailer rear actuator 1
								*trailer_rear_act2,			// trailer rear actuator 2
								*trailer_front_act;			// trailer front actuator

	t2CoilSpring				*trailer_rear_spring1,		// trailer rear springdamper 1
								*trailer_rear_spring2,		// trailer rear springdamper 2
								*trailer_front_spring;		// trailer front springdamper

	t2LinearActuatorJoint	*trailer_platform_actuator;	// trailer platform actuator
	t2ElevatedHinge			*trailer_platform_joint;	// trailer platform joint
	t2ElevatedHinge			*truck_trailer_link;		// truck-trailer link
	bool						trailerConnected;			// 

	// shovel

	t2DistanceJoint             *shovel_distjoint;			// shovel distance joint
	t2AngleConstraint			*shovel_angjoint;			// shovel angular joint
	t2LinearActuatorJoint	*shovel_actuator;			// shovel actuator

	float	act_force,					// platform actuator force
			ang_velocity,				// wheel actuator max angular velocity
			drive_torque,				// wheel actuator max torque
			brake_torque,				// wheel actuator max brake torque
			friction_torque;			// wheel actuator max idle torque
};

#endif
