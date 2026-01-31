
/*
	Tau2D Dynamics Engine
	CJ Gribel (c) 2008-2009, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2WORLD_H
#define T2WORLD_H

#include "float.h"
#include "tau2d.h"
#include "t2Body.h"
#include "t2Joint.h"
#include "t2Force.h"
#include "t2Collision.h"
#include "t2Geometry.h"
#include "config.h"

using namespace std;
class t2SweepPrune;

/*
	world object
*/
class t2World
{
public:

	t2Body*					bodies[T2_MAX_BODIES];
	list<t2Force*>			forces;
	list<t2Joint*>			joints;
	vector<t2ContactJoint>	contact_listA, contact_listB;
    vector<t2ContactJoint>	*contacts, *contacts_prev;
    bool                    warm_starting;
    unsigned int            iterations;

	// world::step: set 'old' contacts list from previous contacts
	// during CD: use addContact(c), which cheches old contacts and uses old j if found
	//	addContact searches old c's, if found: add to new & remove from old (fast if persistent)
	// how identify? body_i-edge_i-body_j-edge-j
	//	(1) cp = vertex of body 4, edges 1-2, inside other body: 4142
	//	(2) cp = isection of body 3 edge 4 and body 6 edge 2: 3462
	//	order? cc? or rely on consistency in CD ? or min(edge) (1) min(body) (2) first
	//t2Contact
	//f32 retrieveInitialGuess(t2ContactJoint *cj);

	// *** generate c id's for simple scene and print 

	// addContact(c)
	//		if exist in old c's, copy lambda
	// add previous impulse in joint-init
	// if (lambda != 0) addImpulse()
	// contacts: like this; ordinary joints: keep previous lambda within class
	// introduce it termination criteria: remember,
	//		still need to check since impulses may be added from elsewhere

	float						dt, idt;					// timestep / inverted timestep
	int							nbrBodies;					// 
	t2SweepPrune				*sweepPrune;				// 
	t2Body						*background;				// 
	unsigned short				colGroupId_inc;				// incrementing collision group ID
	int							colGroupNbr_inc;			// incrementing group number (for display only)
	
	/*
		ctor
	*/
	t2World(float dt);

	/*
		print some world stats
	*/
	void printStats();

	/*
		add body to world
	*/
	void addBody(t2Body* body);

	/*
		remove body from world
	*/
	void removeBody(t2Body* body);
    
    /*
        clear forces, joints & bodies
     */
    void clear_all();

	/*
		detect body at coordinate
	*/
	t2Body* find(vec2f &p);

	/*
		return the number of bodies
	*/
	int getNbrBodies();

	/*
		return the number of geometries
	*/
	int getNbrGeometries();

	/*
		generate a unique collision group id
	*/
	unsigned short generateCollisionGroupId();

	/*
		perform timestep
	*/
	void step();

	/*
		todo: delete constraints & forces (need to iterate lists?)
	*/
	virtual ~t2World(void);
};

#endif /* T2WORLD_H */