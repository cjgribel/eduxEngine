
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

#include "t2World.h"
#include "t2BroadPhase.h"

/*	*/
t2World::t2World(float dt)	:	dt(dt), idt(1.0f/dt), //iterations(20),
								nbrBodies(0),
								colGroupId_inc(2),
								colGroupNbr_inc(2),
                                iterations(T2_SOLVER_ITERATIONS)
	//restingLinVelSquared_tol(0.08f*0.08f), restingAngVel_tol(5.0f*DEG_TO_RAD), framesAtRest_req(10)
{
    contacts = &contact_listA;
    contacts_prev = &contact_listB;
    warm_starting = true;
	sweepPrune = new t2SweepPrune(this);

	background = new t2Body(vec2f(0.0f, 0.0f), true);
	background->setMass(INF, INF);
	addBody(background);
}

/*	*/
void t2World::printStats()
{
	printf("stats:\nbodies %i (%i)\ngeometries %i (%i)\njoints %i\ncollision groups %i (%i)\n, solver iterations: %i\n",
		getNbrBodies(),
		(int)T2_MAX_BODIES,
		getNbrGeometries(),
		(int)T2_MAX_SAP_ELEMENTS/2,
		(int)joints.size(),
		colGroupNbr_inc,
		8*(int)sizeof(colGroupId_inc),
        (int)T2_SOLVER_ITERATIONS);
}

/*	*/
void t2World::addBody(t2Body* body)
{
	if(nbrBodies < T2_MAX_BODIES)
	{
		if(sweepPrune->addBody(body))
			bodies[nbrBodies++] = body;
	}
}

/*
	TODO: remove bodies & geometries
*/
void t2World::removeBody(t2Body* body)
{

}

void t2World::clear_all()
{
	for (std::list<t2Force*>::iterator f_it = forces.begin(); f_it != forces.end(); f_it++) delete *f_it;
    forces.clear();
    
	for (std::list<t2Joint*>::iterator j_it = joints.begin(); j_it != joints.end(); j_it++) delete *j_it;
    joints.clear();
    
	for (int i = 0; i < nbrBodies; i++) delete bodies[i];
    nbrBodies = 0;
    
    sweepPrune->candidates.clear();
    sweepPrune->nbrElements = 0;
    contacts->clear();
    contacts_prev->clear();
}

/*	*/
t2Body* t2World::find(vec2f &p)
{
	for(int i = 0; i < nbrBodies; i++)
	{
		if(VertexInsideBody(p, bodies[i]))
			return bodies[i];
	}
	return NULL;
}

/*	*/
int t2World::getNbrBodies() { return nbrBodies; }

/*	*/
int t2World::getNbrGeometries()
{ 
	int nbrGeoms = 0;
	for(int i = 0; i < nbrBodies; nbrGeoms += bodies[i++]->nbrGeometries);
	return nbrGeoms;
}

/*	*/
unsigned short t2World::generateCollisionGroupId()
{
	unsigned short id = colGroupId_inc;
	colGroupId_inc *= 2;
	colGroupNbr_inc++;

	return id;
}

/*	
 * 150216: switched order of update steps according to tau3d:
 * 1. vertex-shade geometries
 * 2. collision detection
 * 3. apply external forces
 * 4. integrate velocities from forces
 * 5. solve contacts and constraints
 * 6. integrate positions from velocities
 */
void t2World::step()
{
	t2Body *body;
    
    /* 5. update positions using updated velocities (semi-implicit Euler integration) */
    
    for( int i = 0; i < nbrBodies; i++ )
    {
        body = bodies[i];
        if( !body->isStatic )
        {
            if( body->V.normSquared()	> T2_RESTING_LIN_VEL_TOL_SQUARED ||
               fabs(body->W)			> T2_RESTING_ANG_VEL_TOL )
            {
                // rest condition is breached
                body->framesAtRest = 0;
            }
            else
            {
                // rest another frame
                body->framesAtRest++;
            }
            
            // skip simulation of body if in pseudo-sleep
            // note: the 2nd derivative (forces) are still applied,
            // so body may "wake up" when 1st derivatives (velocities) grow large enough
            if( body->framesAtRest < T2_FRAMES_AT_REST_REQUIRED )
            {
                body->X += body->V * dt;
                body->R += body->W * dt;
            }
        }
        
        // apply translation and rotation
        mat2 rot(body->R);
        for (int j = 0; j < body->nbrGeometries; j++)
        {
            body->geometries[j]->transform(rot, body->X);
        }
    }



	/* 2. collision detection (broad + narrow) */

    // swap contact lists
    vector<t2ContactJoint> *contacts_tmp = contacts;
    contacts = contacts_prev;
    contacts_prev = contacts_tmp;
    
	contacts->clear();
#ifdef CD_SAP
	sweepPrune->sweep();
#else
	t2dCDWorld(this);
#endif
    
    /* 1. apply external forces, update velocities */
    
    for(std::list<t2Force*>::iterator f_it = forces.begin(); f_it != forces.end(); f_it++)
        (*f_it)->applyForce();
    
    float defD = 0.0f; // 0.05
    for(int i = 0; i < nbrBodies; i++)
    {
        body = bodies[i];
        if(!body->isStatic)
        {
            body->V += (body->F + body->gravity * body->mass - body->V * defD) * dt * body->imass;
            body->W += (body->T - body->W * defD) * dt * body->iI;
        }
        body->F.set(0.0f, 0.0f);
        body->T = 0.0f;
    }
    
    /* 3. Warm starting & constraint setup */
    if (warm_starting)
    {
        for (int i = 0; i < contacts->size(); i++)
        {
            t2ContactJoint *c = &(contacts->at(i));
            t2ContactJoint *c_warm = NULL;
            for (int j = 0; j < contacts_prev->size(); j++)
            {
                t2ContactJoint *c_prev = &(contacts_prev->at(j));  
                if (c->bodyA == c_prev->bodyA && c->bodyB == c_prev->bodyB &&
                    c->id_geomA == c_prev->id_geomA &&
                    c->id_geomB == c_prev->id_geomB &&
                    c->id_vertex == c_prev->id_vertex &&
                    c->id_edge == c_prev->id_edge)
                {
                    c_warm = c_prev;
                    break;
                }
            }
            c->pre_solve(dt, idt, c_warm);
        }
    }
    else
    {
        for (int i = 0; i < contacts->size(); i++)
            contacts->at(i).pre_solve(dt, idt, NULL);
    }
    
    /* constraint solve init */

	for( std::list<t2Joint*>::iterator j_it = joints.begin(); j_it != joints.end(); j_it++ )
		(*j_it)->init_solve(dt, idt);

	/* 4. solve constraints */
    
	for(int i = 0; i < iterations; i++)
	{
		for( std::list<t2Joint*>::iterator j_it = joints.begin(); j_it != joints.end(); j_it++ )
			(*j_it)->solve(dt, idt);

		for( std::vector<t2ContactJoint>::iterator c_it = contacts->begin(); c_it != contacts->end(); c_it++ )
			c_it->solve(dt, idt);
	}
    
    for( std::vector<t2ContactJoint>::iterator c_it = contacts->begin(); c_it != contacts->end(); c_it++ )
        c_it->post_solve();
    
#if 0
    /* break constraints */
    for( std::list<t2Joint*>::iterator j_it = joints.begin(); j_it != joints.end(); j_it++ ) {
        float joint_force = (*j_it)->joint_force();
        if (joint_force > 200 ) { 
//            printf("%f\n", joint_force);
            joints.remove(*j_it);
        }
    }
#endif


}	/* step() */

t2World::~t2World(void)
{
    clear_all();

	delete sweepPrune;
}