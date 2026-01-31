
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2CRANEWORLD_H
#define T2CRANEWORLD_H
#pragma once

#include "t2WorldInstance.h"
#include "t2WorldContent.h"
#include "t2Crane.h"

class t2CraneWorld : public t2WorldInstance
{
public:
	t2Crane *crane;

	t2CraneWorld(float dt) : t2WorldInstance(dt)
	{
		addShortCourse(vec2f(-5.0f, 2.0f), this);
		crane = new t2Crane(vec2f(-5.0f, 2.5f), this);

		// pit stopper
		t2Box *stopper = new t2Box(vec2f(13.0f, 0.0f), 1.0f, 5.0f, INF, true);
		stopper->setColor(0.1f, 0.1f, 0.1f);
		addBody(stopper);
	}

	void keyDown(unsigned char key)
	{
		crane->event_KeyDown(key);
	}

	void keyUp(unsigned char key)
	{
		crane->event_KeyUp(key);
	}
};

#endif