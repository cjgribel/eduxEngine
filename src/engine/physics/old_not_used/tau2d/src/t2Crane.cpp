
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

#include "t2Crane.h"

t2Crane::t2Crane(vec2f pos, t2World *world)
	: 
//lower_act_force(125e3), upper_act_force(30e3), bucket_act_force(10e3),
	world(world)
{
	float	scale = 1.0f,							//
			density = 1.0f,							//
			mass_scale = density * pow(scale, 2),	// mass scaling factor
			I_scale = density * pow(scale, 4);		// moment of inertia scaling factor

	lower_act_force = 150000.0f * mass_scale;
	upper_act_force = 30000.0f * mass_scale;
	bucket_act_force = 20000.0f * mass_scale;

	// boom & support
	vec2f boom_pos = pos + vec2f(-0.4f, 3.5f)*scale;
	t2Box *boom = new t2Box(boom_pos, 0.8f*scale, 6.0f*scale, 2000.0f*mass_scale, false); world->addBody(boom);
	world->joints.push_back(new t2ElevatedHinge(
		world->background, boom, pos, vec2f(0.4f, -3.5f)*scale, 0.0f, PI,
		vec2f(-1.0f, -0.5f)*scale, vec2f(0.0f, -0.5f)*scale, vec2f(0.0f, -0.5f)*scale, vec2f(0.8f, -0.5f)*scale));
	world->joints.push_back(new t2ElevatedHingeDummy(
		world->background, world->background, pos + vec2f(1.0f, -2.0f)*scale, pos + vec2f(1.0f, -2.0f)*scale, -PI/2.0f, 0.0f,
		vec2f(-0.3f, -1.0f)*scale, vec2f(0.3f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, vec2f(0.0f, 0.0f)*scale));
	world->joints.push_back(new t2ElevatedHingeDummy(
		boom, boom, vec2f(1.4f, -2.0f)*scale, vec2f(1.4f, -2.0f)*scale, -PI/2.0f, 0.0f,
		vec2f(-0.3f, -1.0f)*scale, vec2f(0.3f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, vec2f(0.0f, 0.0f)*scale));
	world->joints.push_back(new t2ElevatedHingeDummy(
		boom, boom, vec2f(-1.4f, 0.0f)*scale, vec2f(-1.4f, 0.0f)*scale, PI/2.0f, 0.0f,
		vec2f(-0.3f, -1.0f)*scale, vec2f(0.3f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, vec2f(0.0f, 0.0f)*scale));
	lower_actuator = new t2LinearActuatorJoint(
		world->background, boom, pos + vec2f(1.0f, -2.0f)*scale, vec2f(1.4f, -2.0f)*scale,
		1.5f*scale, 3.5f*scale, 3.5f*scale, 1.0f, lower_act_force, 0.5f*scale, 1.2f*scale);
	world->joints.push_back(lower_actuator);
	boom->setColor(0.0f, 0.0f, 0.0f);

	// jib
	vec2f jib_pos = boom_pos + vec2f(2.9f, 4.4f)*scale;  //pos + vec2f(2.5f, 7.9f)
	t2Box *jib = new t2Box(jib_pos, 9.0f*scale, 0.8f*scale, 500.0f*mass_scale, false); world->addBody(jib);
	world->joints.push_back(new t2ElevatedHinge(
		boom, jib, vec2f(0.0f, 3.5f)*scale, vec2f(-2.5f, -0.9f)*scale, 0.0f, PI,
		vec2f(-0.4f, -0.5f)*scale, vec2f(0.4f, -0.5f)*scale, vec2f(-0.4f, -0.5f)*scale, vec2f(0.4f, -0.5f)*scale));
	upper_actuator = new t2LinearActuatorJoint(boom, jib, vec2f(-1.4f, 0.0f)*scale, vec2f(-4.5f, 0.0f)*scale,
		2.0f*scale, 5.5f*scale, 3.0f*scale, 1.0f, upper_act_force, 0.4f*scale, 1.7f*scale);
	//upper_actuator->beta = 0.05f;
	world->joints.push_back(upper_actuator);
	jib->setColor(0.0f, 0.0f, 0.0f);

	// bucket
	vec2f bucket_pos = jib_pos + vec2f(4.5f, -0.4f)*scale;
	
	t2Body *left_bucket = new t2Body(bucket_pos + vec2f(-1.5f, -3.0f)*scale, false);
	left_bucket->addGeometry(new t2PolygonGeometry(vec2f(0.25f, 0.75f)*scale, 2.5f*scale, 0.5f*scale));
	//left_bucket->addGeometry(new t2PolygonGeometry(vec2f(-0.75f, -0.5f)*scale, 0.5f*scale, 2.0f*scale));
	vec2f lbucket_v[3] = { vec2f(-0.25f, 1.0f)*scale, vec2f(0.25f, -1.0f)*scale, vec2f(0.25f, 1.0f)*scale };
	left_bucket->addGeometry(new t2PolygonGeometry(lbucket_v, 3, vec2f(-0.75f, -0.5f)*scale));
	left_bucket->setMass(50.0f*mass_scale, 75.0f*I_scale);
	world->addBody(left_bucket);

	t2Body *right_bucket = new t2Body(bucket_pos + vec2f(1.5f, -3.0f)*scale, false);
	right_bucket->addGeometry(new t2PolygonGeometry(vec2f(-0.25f, 0.75f)*scale, 2.5f*scale, 0.5f*scale));
	//right_bucket->addGeometry(new t2PolygonGeometry(vec2f(0.75f, -0.5f)*scale, 0.5f*scale, 2.0f*scale));
	vec2f rbucket_v[3] = { vec2f(-0.25f, 1.0f)*scale, vec2f(-0.25f, -1.0f)*scale, vec2f(0.25f, 1.0f)*scale };
	right_bucket->addGeometry(new t2PolygonGeometry(rbucket_v, 3, vec2f(0.75f, -0.5f)*scale));
	right_bucket->setMass(50.0f*mass_scale, 75.0f*I_scale);
	world->addBody(right_bucket);

	left_bucket->setFriction(0.0f, 0.0f);
	right_bucket->setFriction(0.0f, 0.0f);
	left_bucket->setColor(0.0f, 0.0f, 0.0f);
	right_bucket->setColor(0.0f, 0.0f, 0.0f);

	world->joints.push_back(new t2ElevatedHinge(
		left_bucket, right_bucket, vec2f(2.0f, 0.75f)*scale, vec2f(-2.0f, 0.75f)*scale, PI*3.0f/2.0f, PI/2.0f, 
		vec2f(-0.25f, -0.5f)*scale, vec2f(0.25f, -0.5f)*scale, vec2f(-0.25f, -0.5f)*scale, vec2f(0.25f, -0.5f)*scale));
	world->joints.push_back(new t2DistanceJoint(jib, left_bucket, vec2f(4.5f, -0.4f)*scale, vec2f(-1.0f, 1.0f)*scale, 4.45f*scale));
	world->joints.push_back(new t2DistanceJoint(jib, right_bucket, vec2f(4.5f, -0.4f)*scale, vec2f(1.0f, 1.0f)*scale, 4.45f*scale));
	bucket_actuator = new t2LinearActuatorJoint(jib, left_bucket, vec2f(4.5f, -0.4f)*scale, vec2f(2.0f, 0.75f)*scale,
		1.8f*scale, 3.3f*scale, 2.0f*scale, 1.0f, bucket_act_force, 0.3f*scale, 1.5f*scale);
	world->joints.push_back(bucket_actuator);

    // Bucket angular damping
//    world->forces.push_back(new t2dSpringDamperForce(world->background, left_bucket, pos, vec2f(0.0f, 0.0f)*scale, 0.0f, 250.0f*mass_scale*0, 10.0f*scale, 0, 2500, 1));

    // Bucket linear damping
//	world->forces.push_back(new t2dSpringDamperForce(world->background, left_bucket, pos, vec2f(0.0f, 0.0f)*scale, 0.0f, 250.0f*mass_scale, 10.0f*scale));
}

/*	*/
void t2Crane::event_KeyDown(unsigned char key)
{
	switch (key)
	{
		case 'h': 
			lower_actuator->setMode(LINACTMODE_EXTEND);
											break;
		case 'j': 
			lower_actuator->setMode(LINACTMODE_CONTRACT);
											break;
		case 'k': 
			upper_actuator->setMode(LINACTMODE_CONTRACT);
											break;
		case 'l': 
			upper_actuator->setMode(LINACTMODE_EXTEND);
											break;
		case 'n': 
			bucket_actuator->setMode(LINACTMODE_CONTRACT);
											break;
		case 'm': 
			bucket_actuator->setMode(LINACTMODE_EXTEND);
											break;
		default:
											break;
	}
}

/*	*/
void t2Crane::event_KeyUp(unsigned char key)
{
	switch (key)
	{
		case 'h':
		case 'j': 
			lower_actuator->setMode(LINACTMODE_HOLD);
											break;
		case 'k': 
		case 'l': 
			upper_actuator->setMode(LINACTMODE_HOLD);
											break;
		case 'n': 
		case 'm': 
			bucket_actuator->setMode(LINACTMODE_HOLD);
											break;
		default:
											break;
	}
}
