
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

#include "t2ClausFuhrerTruck.h"

t2ClausFuhrerTruck::t2ClausFuhrerTruck(	vec2f pos,
										t2World *world)
	:	world(world), trailerConnected(true)
{
	float	scale		= 0.75f,					//
			density		= 1.0f,						//
			mass_scale	= density * pow(scale, 2),	// mass scaling factor
			I_scale		= density * pow(scale, 4);	// moment of inertia scaling factor

	act_force		= 5500.0f * mass_scale;			// platform actuator force
	ang_velocity	= -25.0f;						// wheel actuator max angular velocity
	drive_torque	= 2250.0f * I_scale;			// wheel actuator max torque
	brake_torque	= 3000.0f * I_scale;			// wheel actuator max brake torque
	friction_torque = 100.0f * I_scale;				// wheel actuator max idle torque

	unsigned short truckColId = world->generateCollisionGroupId();

	// chassis & cabin
	chassis = new t2Body(pos, false);
	chassis->addGeometry(new t2PolygonGeometry(vec2f(-1.5f, -1.0f)*scale, 10.0f*scale, 1.0f*scale));
	t2PolygonGeometry *cabin = new t2PolygonGeometry(vec2f(1.25f, 1.5f)*scale);
	cabin->addVertex(vec2f(-1.25f, -2.0f)*scale);
	cabin->addVertex(vec2f(2.25f, -2.0f)*scale);
	cabin->addVertex(vec2f(2.25f, 0.0f)*scale);
	cabin->addVertex(vec2f(0.75f, 2.5f)*scale);
	cabin->addVertex(vec2f(-1.25f, 2.5f)*scale);
	chassis->addGeometry(cabin);
	chassis->setMass(150.4f*mass_scale, 1350.0f*I_scale);
	//pos += chassis->setMassFromGeometry(density);
	chassis->setColor(0.0f, 0.0f, 0.4f);
	world->addBody(chassis);
	

	// wheels
	rear_wheel1 = new t2Circle(pos + vec2f(-5.5f, -3.0f)*scale, 1.0f*scale, 4.0f);
	rear_wheel2 = new t2Circle(pos + vec2f(-3.0f, -3.0f)*scale, 1.0f*scale, 4.0f);
	front_wheel = new t2Circle(pos + vec2f(2.5f, -3.0f)*scale, 1.0f*scale, 4.0f);
//	rear_wheel1 = new t2dStudWheel(pos + vec2f(-5.5f, -3.0f)*scale, 4.0f, 0.8f*scale, 10, 0.2f*scale);
//	rear_wheel2 = new t2dStudWheel(pos + vec2f(-3.0f, -3.0f)*scale, 4.0f, 0.8f*scale, 10, 0.2f*scale);
//	front_wheel = new t2dStudWheel(pos + vec2f(2.5f, -3.0f)*scale, 4.0f, 0.8f*scale, 10, 0.2f*scale);

	rear_wheel1->setFriction(3.0f, 3.0f);
	rear_wheel2->setFriction(3.0f, 3.0f); 
	front_wheel->setFriction(3.0f, 3.0f);
	rear_wheel1->restitution = 0.0f;
	rear_wheel2->restitution = 0.0f;
	front_wheel->restitution = 0.0f;
	rear_wheel1->setColor(0.0f, 0.0f, 0.0f);
	rear_wheel2->setColor(0.0f, 0.0f, 0.0f);
	front_wheel->setColor(0.0f, 0.0f, 0.0f);

	world->addBody(rear_wheel1);
	world->addBody(rear_wheel2);
	world->addBody(front_wheel);

	// actuators
	rear_act1 = new t2AngularActuatorConstraint(chassis, rear_wheel1);
	rear_act2 = new t2AngularActuatorConstraint(chassis, rear_wheel2);
	front_act = new t2AngularActuatorConstraint(chassis, front_wheel);

	rear_act1->enableActuator(0.0f, friction_torque);
	rear_act2->enableActuator(0.0f, friction_torque);
	front_act->enableActuator(0.0f, friction_torque);

	world->joints.push_back(rear_act1);	
	world->joints.push_back(rear_act2);	
	world->joints.push_back(front_act);

	// suspension joints
	rear_susp1 = new t2PrismaticConstraint(chassis, rear_wheel1, vec2f(-5.5f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, 0.0f, 0.0f);
	rear_susp2 = new t2PrismaticConstraint(chassis, rear_wheel2, vec2f(-3.0f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, 0.0f, 0.0f);
	front_susp = new t2PrismaticConstraint(chassis, front_wheel, vec2f(2.5f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, 0.0f, 0.0f);

	world->joints.push_back(rear_susp1);
	world->joints.push_back(rear_susp2);
	world->joints.push_back(front_susp);

	// springdampers
	rear_spring1 = new t2CoilSpring(chassis, rear_wheel1, vec2f(-5.5f, -1.5f)*scale, vec2f(0.0f, 0.0f)*scale, 7500.0f*mass_scale*1.2f, 500.0f*mass_scale*1.2f, 2.0f*scale, 5, 0.3f*scale);
	rear_spring2 = new t2CoilSpring(chassis, rear_wheel2, vec2f(-3.0f, -1.5f)*scale, vec2f(0.0f, 0.0f)*scale, 7500.0f*mass_scale*1.2f, 500.0f*mass_scale*1.2f, 2.0f*scale, 5, 0.3f*scale);
	front_spring = new t2CoilSpring(chassis, front_wheel, vec2f(2.5f, -1.5f)*scale, vec2f(0.0f, 0.0f)*scale, 8500.0f*mass_scale*1.2f, 600.0f*mass_scale*1.2f, 2.0f*scale, 5, 0.3f*scale);

	world->forces.push_back(rear_spring1);
	world->forces.push_back(rear_spring2);
	world->forces.push_back(front_spring);

	// platform
	t2Body *platform = new t2Body(pos + vec2f(-3.5f, 1.0f)*scale, false);
	platform->addGeometry(new t2PolygonGeometry(vec2f(0.0f, -0.75f)*scale, 6.5f*scale, 0.5f*scale));
	platform->addGeometry(new t2PolygonGeometry(vec2f(-3.0f, 1.0f)*scale, 0.5f*scale, 3.0f*scale));
	platform->addGeometry(new t2PolygonGeometry(vec2f(3.0f, 1.0f)*scale, 0.5f*scale, 3.0f*scale));

	platform->setMass(40.0f*mass_scale, 110.0f*I_scale);

	platform_joint = new t2ElevatedHinge(
		chassis, platform, vec2f(-6.5f, -0.25f)*scale, vec2f(-2.75f, -1.25f)*scale, 0.0f, PI,
		vec2f(0.0f, -0.25f)*scale, vec2f(0.5f, -0.25f)*scale, vec2f(-0.25f, -0.25f)*scale, vec2f(0.25f, -0.25f)*scale);

	platform_actuator = new t2LinearActuatorJoint(
		chassis, platform, vec2f(-4.0f, -1.0f)*scale, vec2f(0.75f, -0.75f)*scale,
		1.6f*scale, 5.5f*scale, 1.6f*scale, 2.0f, act_force, 0.35f*scale, 1.4f*scale);

	world->addBody(platform);
	world->joints.push_back(platform_joint);
	world->joints.push_back(platform_actuator);
	platform->setColor(0.0f, 0.0f, 0.2f);

	// shovel
	t2Box *shovel = new t2Box(pos + vec2f(4.0f, -1.0f)*scale, 0.5f*scale, 4.0f*scale, 10.0f, false);
	shovel_distjoint = new t2DistanceJoint(chassis, shovel, vec2f(0.0f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, 4.5f*scale);
	shovel_angjoint = new t2AngleConstraint(chassis, shovel, 0.0f);
	shovel_actuator = new t2LinearActuatorJoint(
		chassis, shovel, vec2f(3.5f, 0.5f)*scale, vec2f(0.0f, 0.0f)*scale,
		1.5f*scale, 3.5f*scale, 1.5f*scale, 2.0f, act_force, 0.25f*scale, 1.3f*scale);
	world->addBody(shovel);
	world->joints.push_back(shovel_distjoint);
	world->joints.push_back(shovel_angjoint);
	world->joints.push_back(shovel_actuator);
	shovel->setColor(0.8f, 0.8f, 0.0f);

	// trailer

	vec2f trailer_pos = pos - vec2f(12.0f, 0.0f)*scale;
	
	// trailer chassis
	t2Box *trailer_chassis = new t2Box(trailer_pos, 8.0f*scale, 1.0f*scale, 10.0f);
	trailer_chassis->setColor(0.0f, 0.0f, 0.4f);
	world->addBody(trailer_chassis);

	// trailer platform
	t2Body *trailer_platform = new t2Body(trailer_pos + vec2f(0.0f, 2.25f)*scale, false);
	trailer_platform->addGeometry(new t2PolygonGeometry(vec2f(0.0f, -1.25f)*scale, 8.0f*scale, 0.5f*scale));
	trailer_platform->addGeometry(new t2PolygonGeometry(vec2f(-3.75f, 0.5f)*scale, 0.5f*scale, 3.0f*scale));
	trailer_platform->addGeometry(new t2PolygonGeometry(vec2f(3.75f, 0.5f)*scale, 0.5f*scale, 3.0f*scale));

	trailer_platform->setMass(40.0f*mass_scale, 110.0f*I_scale);
	trailer_platform->setColor(0.0f, 0.0f, 0.2f);

	trailer_platform_joint = new t2ElevatedHinge(
		trailer_chassis, trailer_platform, vec2f(-4.0f, 0.75f)*scale, vec2f(-4.0f, -1.75f)*scale, 0.0f, PI,
		vec2f(0.0f, -0.25f)*scale, vec2f(0.5f, -0.25f)*scale, vec2f(-0.25f, -0.25f)*scale, vec2f(0.0f, -0.25f)*scale);

	trailer_platform_actuator = new t2LinearActuatorJoint(
		trailer_chassis, trailer_platform, vec2f(-1.0f, 0.0f)*scale, vec2f(0.5f, -1.5f)*scale,
		1.8f*scale, 6.6f*scale, 1.8f*scale, 2.0f, act_force, 0.4f*scale, 1.6f*scale);

	world->addBody(trailer_platform);
	world->joints.push_back(trailer_platform_joint);
	world->joints.push_back(trailer_platform_actuator);	

	// trailer wheels
	trailer_rear_wheel1 = new t2Circle(trailer_pos + vec2f(-3.5f, -3.0f)*scale, 1.0f*scale, 4.0f);
	trailer_rear_wheel2 = new t2Circle(trailer_pos + vec2f(-1.0f, -3.0f)*scale, 1.0f*scale, 4.0f);
	trailer_front_wheel = new t2Circle(trailer_pos + vec2f(3.5f, -3.0f)*scale, 1.0f*scale, 4.0f);
	//trailer_rear_wheel1 = new t2dStudWheel(trailer_pos + vec2f(-3.0f, -3.0f)*scale, 4.0f, 0.8f*scale, 10, 0.2f*scale);
	//trailer_rear_wheel2 = new t2dStudWheel(trailer_pos + vec2f(-0.5f, -3.0f)*scale, 4.0f, 0.8f*scale, 10, 0.2f*scale);
	//trailer_front_wheel = new t2dStudWheel(trailer_pos + vec2f(3.0f, -3.0f)*scale, 4.0f, 0.8f*scale, 10, 0.2f*scale);

	trailer_rear_wheel1->setFriction(3.0f, 3.0f);
	trailer_rear_wheel2->setFriction(3.0f, 3.0f); 
	trailer_front_wheel->setFriction(3.0f, 3.0f);
	trailer_rear_wheel1->restitution = 0.0f;
	trailer_rear_wheel2->restitution = 0.0f;
	trailer_front_wheel->restitution = 0.0f;
	trailer_rear_wheel1->setColor(0.0f, 0.0f, 0.0f);
	trailer_rear_wheel2->setColor(0.0f, 0.0f, 0.0f);
	trailer_front_wheel->setColor(0.0f, 0.0f, 0.0f);

	world->addBody(trailer_rear_wheel1);
	world->addBody(trailer_rear_wheel2);
	world->addBody(trailer_front_wheel);

	// trailer actuators
	trailer_rear_act1 = new t2AngularActuatorConstraint(trailer_chassis, trailer_rear_wheel1);
	trailer_rear_act2 = new t2AngularActuatorConstraint(trailer_chassis, trailer_rear_wheel2);
	trailer_front_act = new t2AngularActuatorConstraint(trailer_chassis, trailer_front_wheel);

	trailer_rear_act1->enableActuator(0.0f, friction_torque);
	trailer_rear_act2->enableActuator(0.0f, friction_torque);
	trailer_front_act->enableActuator(0.0f, friction_torque);

	world->joints.push_back(trailer_rear_act1);	
	world->joints.push_back(trailer_rear_act2);	
	world->joints.push_back(trailer_front_act);

	// trailer suspension joints
	trailer_rear_susp1 = new t2PrismaticConstraint(trailer_chassis, trailer_rear_wheel1, vec2f(-3.0f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, 0.0f, 0.0f);
	trailer_rear_susp2 = new t2PrismaticConstraint(trailer_chassis, trailer_rear_wheel2, vec2f(-0.5f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, 0.0f, 0.0f);
	trailer_front_susp = new t2PrismaticConstraint(trailer_chassis, trailer_front_wheel, vec2f(3.0f, -1.0f)*scale, vec2f(0.0f, 0.0f)*scale, 0.0f, 0.0f);

	world->joints.push_back(trailer_rear_susp1);
	world->joints.push_back(trailer_rear_susp2);
	world->joints.push_back(trailer_front_susp);

	// trailer springdampers
	trailer_rear_spring1 = new t2CoilSpring(trailer_chassis, trailer_rear_wheel1, vec2f(-3.0f, -0.5f)*scale, vec2f(0.0f, 0.0f)*scale, 7500.0f*mass_scale*1.2f, 500.0f*mass_scale*1.2f, 2.0f*scale, 5, 0.3f*scale);
	trailer_rear_spring2 = new t2CoilSpring(trailer_chassis, trailer_rear_wheel2, vec2f(-0.5f, -0.5f)*scale, vec2f(0.0f, 0.0f)*scale, 7500.0f*mass_scale*1.2f, 500.0f*mass_scale*1.2f, 2.0f*scale, 5, 0.3f*scale);
	trailer_front_spring = new t2CoilSpring(trailer_chassis, trailer_front_wheel, vec2f(3.0f, -0.5f)*scale, vec2f(0.0f, 0.0f)*scale, 8500.0f*mass_scale*1.2f, 600.0f*mass_scale*1.2f, 2.0f*scale, 5, 0.3f*scale);

	world->forces.push_back(trailer_rear_spring1);
	world->forces.push_back(trailer_rear_spring2);
	world->forces.push_back(trailer_front_spring);

	// truck-trailer link
	truck_trailer_link = new t2ElevatedHinge(
		chassis, trailer_chassis, vec2f(-6.5f, -1.0f)*scale, vec2f(6.0f, 0.0f)*scale, PI*0.5f, -PI*0.5f,
		vec2f(0.0f, 0.0f)*scale, vec2f(0.0f, 0.0f)*scale, vec2f(-0.25f, -2.0f)*scale, vec2f(0.25f, -2.0f)*scale);
	truck_trailer_link->enableAngularLimits(-0.65f, 0.65f, 0.0f);
	world->joints.push_back(truck_trailer_link);
    
    // Done
    
    float total_mass = 0;
    total_mass += chassis->mass + shovel->mass + platform->mass;
    total_mass += rear_wheel1->mass + rear_wheel2->mass + front_wheel->mass;
    total_mass += trailer_front_wheel->mass + trailer_rear_wheel1->mass + trailer_rear_wheel2->mass;
    std::cout << "Truck created. Mass = " << total_mass << std::endl;
}

/*	*/
void t2ClausFuhrerTruck::disassemble()
{
	world->joints.remove(rear_susp1);
	world->joints.remove(rear_susp2);
	world->joints.remove(front_susp);
	world->joints.remove(platform_joint);
	world->joints.remove(platform_actuator);
	world->forces.remove(rear_spring1);
	world->forces.remove(rear_spring2);
	world->forces.remove(front_spring);
	rear_act1->disableActuator();
	rear_act2->disableActuator();
	front_act->disableActuator();
	rear_wheel1->restitution = 0.6f;
	rear_wheel2->restitution = 0.6f;
	front_wheel->restitution = 0.6f;

	world->joints.remove(shovel_distjoint);
	world->joints.remove(shovel_angjoint);
	world->joints.remove(shovel_actuator);

	world->joints.remove(trailer_rear_susp1);
	world->joints.remove(trailer_rear_susp2);
	world->joints.remove(trailer_front_susp);
	world->joints.remove(trailer_platform_joint);
	world->joints.remove(trailer_platform_actuator);
	world->forces.remove(trailer_rear_spring1);
	world->forces.remove(trailer_rear_spring2);
	world->forces.remove(trailer_front_spring);
	trailer_rear_act1->disableActuator();
	trailer_rear_act2->disableActuator();
	trailer_front_act->disableActuator();
	trailer_rear_wheel1->restitution = 0.6f;
	trailer_rear_wheel2->restitution = 0.6f;
	trailer_front_wheel->restitution = 0.6f;
	world->joints.remove(truck_trailer_link);
}

/*	*/
void t2ClausFuhrerTruck::event_KeyDown(unsigned char key)
{
	switch(key)
	{
		case 'e':	shovel_actuator->setMode(LINACTMODE_CONTRACT);			break;

		case 'r':	shovel_actuator->setMode(LINACTMODE_EXTEND);				break;

		case 't':	platform_actuator->setMode(LINACTMODE_EXTEND);			break;

		case 'y':	platform_actuator->setMode(LINACTMODE_CONTRACT);			break;

		case 'g':	trailer_platform_actuator->setMode(LINACTMODE_EXTEND);	break;

		case 'h':	trailer_platform_actuator->setMode(LINACTMODE_CONTRACT);	break;

		case 'b':
			world->joints.remove(truck_trailer_link);
			trailerConnected = false;											break;

		case 'n':	disassemble();												break;

		case 'a':
			rear_act1->enableActuator(ang_velocity, drive_torque);
			rear_act2->enableActuator(ang_velocity, drive_torque);
			front_act->enableActuator(ang_velocity, drive_torque);
			if (trailerConnected)
			{
				trailer_rear_act1->enableActuator(ang_velocity, drive_torque);
				trailer_rear_act2->enableActuator(ang_velocity, drive_torque);
				trailer_front_act->enableActuator(ang_velocity, drive_torque);
			}
																				break;
		case 's':
			rear_act1->enableActuator(0.0f, brake_torque);
			rear_act2->enableActuator(0.0f, brake_torque);
			front_act->enableActuator(0.0f, brake_torque);
			if (trailerConnected)
			{
				trailer_rear_act1->enableActuator(0.0f, brake_torque);
				trailer_rear_act2->enableActuator(0.0f, brake_torque);
				trailer_front_act->enableActuator(0.0f, brake_torque);
			}
																				break;
		case 'd':
			rear_act1->enableActuator(-ang_velocity, drive_torque);
			rear_act2->enableActuator(-ang_velocity, drive_torque);
			front_act->enableActuator(-ang_velocity, drive_torque);
			if (trailerConnected)
			{
				trailer_rear_act1->enableActuator(-ang_velocity, drive_torque);
				trailer_rear_act2->enableActuator(-ang_velocity, drive_torque);
				trailer_front_act->enableActuator(-ang_velocity, drive_torque);
			}
																				break;
		default:	break;
	}
}

/*	*/
void t2ClausFuhrerTruck::event_KeyUp(unsigned char key)
{
	switch(key)
	{
		case 'e': 
		case 'r':
			shovel_actuator->setMode(LINACTMODE_HOLD);
																	break;

		case 't': 
		case 'y': 
			platform_actuator->setMode(LINACTMODE_HOLD);
																	break;
		case 'g': 
		case 'h': 
			trailer_platform_actuator->setMode(LINACTMODE_HOLD);
																	break;
		case 'a':
		case 's':
		case 'd':
			rear_act1->enableActuator(0.0f, friction_torque);
			rear_act2->enableActuator(0.0f, friction_torque);
			front_act->enableActuator(0.0f, friction_torque);

			trailer_rear_act1->enableActuator(0.0f, friction_torque);
			trailer_rear_act2->enableActuator(0.0f, friction_torque);
			trailer_front_act->enableActuator(0.0f, friction_torque);
																	break;

		default:	break;
	}
}
