
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2DMISCWORLD_H
#define T2DMISCWORLD_H

#include "t2WorldInstance.h"
#include "t2WorldContent.h"

class t2dMiscWorld : public t2WorldInstance
{
public:

	t2dMiscWorld(float dt) : t2WorldInstance(dt)
	{
//		addCourse(vec2f(-5.0f, 2.0f), this);
//		addMisc(this);
	}
};

#endif