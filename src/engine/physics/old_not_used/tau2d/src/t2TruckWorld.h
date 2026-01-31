
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2TRUCKWORLD_H
#define T2TRUCKWORLD_H
#pragma once

#include "t2WorldInstance.h"
#include "t2WorldContent.h"
#include "t2ClausFuhrerTruck.h"

class t2dTruckWorld : public t2WorldInstance
{
public:
	t2ClausFuhrerTruck *truck;

	t2dTruckWorld(float dt) : t2WorldInstance(dt)
	{
		addCourse(vec2f(-5.0f, 2.0f), this);
		truck = new t2ClausFuhrerTruck(vec2f(10.0f, 2.0f), this);
	}

	void keyDown(unsigned char key)
	{
		truck->event_KeyDown(key);
	}

	void keyUp(unsigned char key)
	{
		truck->event_KeyUp(key);
	}
};

#endif