
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

#pragma once
#ifndef T2WORLDINSTANCE_H
#define T2WORLDINSTANCE_H

#include "t2World.h"
#include "t2UIEvent.h"

/**
 * A world instance.
 */
class t2WorldInstance : public t2World
{
public:
	t2Body	*camera;

	t2UIEvent		*UIEvents[6];
	t2UIEvent		*currentUIEvent;
	enum UIEventType {	UIEVENT_POINTER = 0,
						UIEVENT_ADD_BODY,
						UIEVENT_ADD_SPRING,
						UIEVENT_ADD_DISTJOINT,
						UIEVENT_ADD_POLYGON,
						UIEVENT_UNIFY_BODIES};
	UIEventType	currentUIEventType;
	

	t2WorldInstance(float dt) : t2World(dt)
	{
		camera = new t2Body(vec2f(3.0f, 6.0f), false);
		camera->gravity.set(0.0f, 0.0f);
		addBody(camera);

		UIEvents[0] = new t2UIPointer(this, camera);
		UIEvents[1]	= new t2UIAddBody(this);
		UIEvents[2]	= new t2UIAddSpring(this, 500.0f, 85.0f, 0.15f);
		UIEvents[3]	= new t2UIAddDistanceJoint(this);
		UIEvents[4]	= new t2UIAddPolygon(this);
		UIEvents[5]	= new t2UIUnifyBodies(this);

		currentUIEventType = UIEVENT_POINTER;
		setUIEventType( currentUIEventType );
	}

	void setUIEventType(UIEventType type)
	{
		currentUIEventType = type;
		currentUIEvent = UIEvents[(int)type];
		currentUIEvent->reset();
	}

	virtual void keyDown(unsigned char key) { }

	virtual void keyUp(unsigned char key) { }

	~t2WorldInstance()
	{
		for (int i = 0; i < 4; i++) delete UIEvents[i];
	}
};

#endif