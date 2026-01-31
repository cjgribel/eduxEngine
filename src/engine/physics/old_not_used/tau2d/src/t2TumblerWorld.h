
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2DTUMBLERWORLD_H
#define T2DTUMBLERWORLD_H

#include "t2WorldInstance.h"
//#include "t2dWorldContent.h"
#include "t2World.h"
#include "t2Body.h"
#include "t2Joint.h"
#include "t2Constraint.h"

class t2TumblerWorld : public t2WorldInstance
{
public:
	t2AngularActuatorConstraint *motor;
	float velocity, torque;

	t2TumblerWorld(float dt) : t2WorldInstance(dt)
	{
		velocity = 0.0f;
		torque = 1.0e6;

		t2Body *tumbler = new t2Body(vec2f(3.0f, 6.0f), false);
		tumbler->addGeometry(new t2PolygonGeometry(vec2f(-10.0f, 0.0f), 2.0f, 22.0f));
		tumbler->addGeometry(new t2PolygonGeometry(vec2f(10.0f, 0.0f), 2.0f, 22.0f));
		tumbler->addGeometry(new t2PolygonGeometry(vec2f(0.0f, -10.0f), 18.0f, 2.0f));
		tumbler->addGeometry(new t2PolygonGeometry(vec2f(0.0f, 10.0f), 18.0f, 2.0f));
		tumbler->setMass(1500.0f, 15000.0f);
		tumbler->setColor(0.2f, 0.2f, 0.2f);
		addBody(tumbler);

		joints.push_back(new t2RevoluteConstraint(tumbler, background, vec2f(0.0f, 0.0f), vec2f(3.0f, 6.0f)));

		motor = new t2AngularActuatorConstraint(tumbler, background);
		motor->enableActuator(velocity, torque);
		joints.push_back(motor);
	}

	void keyDown(unsigned char key)
	{
		switch (key)
		{
			case 'a':
				velocity += 0.2f;
				motor->enableActuator(velocity, torque);
				break;
			case 'd':
				velocity -= 0.2f;
				motor->enableActuator(velocity, torque);
				break;
			case 's':
				velocity = 0.0f;
				motor->enableActuator(velocity, torque);
				break;
		}
	}

	void keyUp(unsigned char key) { }
};

#endif