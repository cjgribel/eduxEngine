
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2DRAGDOLL_H
#define T2DRAGDOLL_H
#pragma once

#include "t2World.h"
#include "t2Body.h"
#include "t2Joint.h"
#include "t2Constraint.h"

/*
	ragdoll class
	model by Mark Bayazit (mnbayazit.com)
*/
class t2Ragdoll
{
public:

	t2Ragdoll(vec2f pos, float scale, float density, unsigned short collisionGroupId, t2World* world);

	void disassemble();

private:

	t2World *world;
	t2Body
		*rfoot, *lfoot,
		*rcalf, *lcalf,
		*rthigh, *lthigh,
		*pelvis, *stomach, *chest,
		*rupperarm, *lupperarm,
		*rforearm, *lforearm,
		*rhand, *lhand,
		*neck, *head;
	t2RevoluteJoint
		*rankle, *lankle,
		*rknee, *lknee,
		*rhip, *lhip,
		*lowerab, *upperab,
		*lowerneck, *upperneck,
		*rshoulder, *lshoulder,
		*relbow, *lelbow,
		*rwrist, *lwrist;
	t2Body* bodyparts[17];
	t2RevoluteJoint* joints[16];
	int bodypart_count, joint_count;
};

#endif