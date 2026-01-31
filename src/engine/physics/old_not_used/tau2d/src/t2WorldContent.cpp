
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

#include "t2WorldContent.h"
#include "t2World.h"

/*	*/
void addMisc(t2World *world)
{
	//addPyramid(20, 0.4f, 8.0f, 0.05f, 5.0f, -3.0f, GeometryType::GEOMTYPE_POLY, world);
	//addPyramid(7, 1.0f, 0.1f, 7.5f, GeometryType::GEOMTYPE_CIRCLE, world);
	//addStack(15, 0.7f, 0.8f, 0.1f, -8.0f, -8.0f, GeometryType::GEOMTYPE_POLY, world); // 170
	//addStack(10, 0.7f, 0.8f, 0.1f, -1.0f, 15.0f, GeometryType::GEOMTYPE_POLY, world);
	//addCircleLink(vec2f(-7.5f, 0.0f), vec2f(7.5f, 0.0f), 17, 0.5f, world);

	//// three connected bodies
	t2Circle *b1 = new t2Circle(vec2f(8.0f, 3.0f), 0.5f, 10.0f); world->addBody(b1);
	t2Box *b2 = new t2Box(vec2f(10.0f, 3.0f), 3.0f, 1.0f, 10.0f, false); world->addBody(b2);
	t2Box *b3 = new t2Box(vec2f(12.0f, 3.0f), 3.0f, 1.0f, 10.0f, false); world->addBody(b3);
	world->joints.push_back(new t2DistanceJoint(b1, b2, vec2f(0.0f, 0.0f), vec2f(1.5f, 0.5f), 4.0f));
	world->joints.push_back(new t2DistanceJoint(b2, b3, vec2f(-1.5f, -0.5f), vec2f(1.5f, 0.5f), 4.0f));
	world->joints.push_back(new t2AngleConstraint(b2, b3, 0.0f));

	//// Newton craddle
	t2Circle *nb1 = new t2Circle(vec2f(-0.6f, 4.0f), 0.5f, 10.0f);
	t2Circle *nb2 = new t2Circle(vec2f(-1.7f, 4.0f), 0.5f, 10.0f);
	t2Circle *nb3 = new t2Circle(vec2f(0.6f, 4.0f), 0.5f, 10.0f);
	t2Circle *nb4 = new t2Circle(vec2f(1.7f, 4.0f), 0.5f, 10.0f);
	nb1->iI = 0.0f; nb2->iI = 0.0f; nb3->iI = 0.0f; nb4->iI = 0.0f;
	nb1->setFriction(0.0f, 0.0f); nb2->setFriction(0.0f, 0.0f); nb3->setFriction(0.0f, 0.0f); nb4->setFriction(0.0f, 0.0f);
	nb1->restitution = 0.95f;  nb2->restitution = 0.95f; nb3->restitution = 0.95f; nb4->restitution = 0.95f;
	world->addBody(nb1); world->addBody(nb2); world->addBody(nb3); world->addBody(nb4);
	world->joints.push_back(new t2DistanceJoint(world->background, nb1, vec2f(-0.5f, 8.0f), vec2f(0.0f, 0.0f), 4.0f));
	world->joints.push_back(new t2DistanceJoint(world->background, nb2, vec2f(-1.5f, 8.0f), vec2f(0.0f, 0.0f), 4.0f));
	world->joints.push_back(new t2DistanceJoint(world->background, nb3, vec2f(0.5f, 8.0f), vec2f(0.0f, 0.0f), 4.0f));
	world->joints.push_back(new t2DistanceJoint(world->background, nb4, vec2f(1.5f, 8.0f), vec2f(0.0f, 0.0f), 4.0f));

	// double pendulum with angular springdampers
	vec2f bpos(-2.0f, 7.5f);
	unsigned short col_group = world->generateCollisionGroupId();
	t2Box *board1 = new t2Box(bpos + vec2f(2.5f, 0.0f), 5.0f, 0.8f, 40.0f, false);
	board1->collisionGroupId = col_group;
	board1->collisionFilter = T2_COLLISION_GROUP_ALL ^ col_group;
	world->addBody(board1);
	world->joints.push_back(new t2RevoluteJoint(world->background, board1, bpos, vec2f(-2.5f, 0.0f)));
	world->forces.push_back(new t2dSpringDamperForce(world->background, board1, vec2f(), vec2f(), 0.0f, 0.0f, 0.0f, 25000.0f, 2000.0f, PI/2.0f));
	
	t2Box *board2 = new t2Box(bpos + vec2f(6.5f, 0.0f), 5.0f, 0.6f, 40.0f, false);
	board2->collisionGroupId = col_group;
	board2->collisionFilter = T2_COLLISION_GROUP_ALL ^ col_group;
	world->addBody(board2);
	world->joints.push_back(new t2RevoluteJoint(board1, board2, vec2f(2.0f, 0.0f), vec2f(-2.0f, 0.0f)));
	world->forces.push_back(new t2dSpringDamperForce(board1, board2, vec2f(), vec2f(), 0.0f, 0.0f, 0.0f, 13000.0f, 1550.0f, 0.0f));

	// angular limits test
	vec2f bpos2(3.0f, 6.5f);
	t2Box *board3 = new t2Box(bpos2 + vec2f(2.5f, 0.0f), 5.0f, 0.8f, 40.0f, false);
    col_group = world->generateCollisionGroupId();
	board3->collisionGroupId = col_group;
	board3->collisionFilter = T2_COLLISION_GROUP_ALL ^ col_group;
	t2RevoluteJoint *rev1 = new t2RevoluteJoint(world->background, board3, bpos2, vec2f(-2.5f, 0.0f));
	rev1->enableAngularLimits(-PI/2.0f, 0, PI/2.0f);
	world->addBody(board3);
	world->joints.push_back(rev1);

	t2Box *board4 = new t2Box(bpos2 + vec2f(6.5f, 0.0f), 5.0f, 0.6f, 40.0f, false);
	board4->collisionGroupId = col_group;
	board4->collisionFilter = T2_COLLISION_GROUP_ALL ^ col_group;
	t2RevoluteJoint *rev2 = new t2RevoluteJoint(board3, board4, vec2f(2.0f, 0.0f), vec2f(-2.0f, 0.0f));
	rev2->enableAngularLimits(-PI/6.0f, PI/6.0f, 0.0f);
	world->addBody(board4);
	world->joints.push_back(rev2);

	//// circles with different restitution
	// //note: need to set restitution for course also, or use values > 1
	//t2Circle *c1 = new t2Circle(vec2f(-2.0f, 4.0f), 0.5f, 10.0f); c1->restitution = 0.3f;
	//t2Circle *c2 = new t2Circle(vec2f(0.0f, 4.0f), 0.5f, 10.0f); c2->restitution = 0.5f;
	//t2Circle *c3 = new t2Circle(vec2f(2.0f, 4.0f), 0.5f, 10.0f); c3->restitution = 0.7f;
	//t2Circle *c4 = new t2Circle(vec2f(4.0f, 4.0f), 0.5f, 10.0f); c4->restitution = 0.9f;
	//world->addBody(c1); world->addBody(c2); world->addBody(c3); world->addBody(c4);

	// boxes with different friction
	t2Box *c1 = new t2Box(vec2f(4.0f, 1.0f), 1.0f, 1.0f, 8.0f); c1->setFriction(0.0f, 0.0f);
	t2Box *c2 = new t2Box(vec2f(6.0f, 1.0f), 1.0f, 1.0f, 8.0f); c2->setFriction(0.5f, 0.5f);
	t2Box *c3 = new t2Box(vec2f(8.0f, 1.0f), 1.0f, 1.0f, 8.0f); c3->setFriction(0.8f, 0.8f);
	world->addBody(c1); world->addBody(c2); world->addBody(c3);

	//// contact point demo
	//t2Box *c1 = new t2Box(vec2f(-2.0f, 1.0f), 1.0f, 1.0f, 8.0f); c1->gravity.set(0.0f, 0.0f);
	//t2Box *c2 = new t2Box(vec2f(0.0f, 1.0f), 1.0f, 1.0f, 8.0f); c2->gravity.set(0.0f, 0.0f);
	//t2Box *c3 = new t2Box(vec2f(2.0f, 1.0f), 1.0f, 1.0f, 8.0f); c3->gravity.set(0.0f, 0.0f);
	//world->addBody(c1); world->addBody(c2); world->addBody(c3);

	//// dominos
	//for(int i = 0; i < 50; i++)
	//{
	//	t2Box *box = new t2Box(vec2f(20.0f + i*2.0f, -6.0f), 0.5f, 4.0f, 20.0f, false);
	//	//box->setFriction(0.0f, 0.0f);
	//	//box->restitution = 0.1f;
	//	world->addBody(box);
	//}
	
	// a few bodies
	world->addBody(new t2dStudWheel(vec2f(-4.0f, -2.0f), 8.0f, 1.0f, 10, 0.2f));
	world->addBody(new t2dRegularPoly(vec2f(2.5f, -2.1f), 6, 0.9f, 8.0f));
	world->addBody(new t2dRegularPoly(vec2f(2.5f, -0.5f), 6, 0.8f, 8.0f));
	world->addBody(new t2dRegularPoly(vec2f(2.5f, 0.9f), 6, 0.7f, 8.0f));
	world->addBody(new t2dRegularPoly(vec2f(2.5f, 2.1f), 6, 0.6f, 8.0f));
	
	world->addBody(new t2dRegularPoly(vec2f(0.5f, -2.1f), 6, 0.9f, 8.0f));
	world->addBody(new t2dRegularPoly(vec2f(0.5f, -0.5f), 6, 0.8f, 8.0f));
	world->addBody(new t2dRegularPoly(vec2f(0.5f, 0.9f), 6, 0.7f, 8.0f));
	world->addBody(new t2dRegularPoly(vec2f(0.5f, 2.1f), 6, 0.6f, 8.0f));
}

/*	*/
void addPyramid(int levels, float dim, bool rand_dim, float density, float spacing,float x, float y, GeometryType type, t2World *world)
{
	float
		x_val = x - dim*(float)levels/2.0f - (float)(levels-1)/2.0f*spacing + dim/2.0f,
												// initial x-value
		x_inc = dim + spacing,					// x-increment
		y_val = y + dim/2.0f + spacing,			// initial y-value
		y_inc = dim + 0.0f*spacing,				// y-increment
		x_yval = x_val,							// initial leftmost x-value
		x_yinc = dim/2.0f + spacing/2.0f;		// increment of leftmost x-value
    float dim_x = dim, dim_y = dim;
		
	for(int hi = 0; hi < levels; hi++)
	{
		for(int wi = 0; wi < levels - hi; wi++)
		{   
			if(type == GEOMTYPE_POLY)
			{
                if (rand_dim)
                {
                    dim_x = 0.3f*dim + float(rand() % int(7*dim))/10.0f;
                    dim_y = 0.3f*dim + float(rand() % int(7*dim))/10.0f;
                }
                
				t2Box *b = new t2Box(vec2f(x_val, y_val), dim_x, dim_y, dim*dim*density, false);
				b->setColor(0, 0.25f, 0.8f);
				world->addBody(b);
			}
			else
			{
                if (rand_dim)
                    dim_x = 0.5f*dim + float(rand() % int(5*dim))/10.0f;
                
				t2Circle *c = new t2Circle(vec2f(x_val, y_val), PI*dim_x*dim_x/4.0f*density, dim_x/2.0f, false);
				c->setColor(0, 0.25f, 0.8f);
				world->addBody(c);
			}
			x_val += x_inc;
		}
		y_val += y_inc;
		x_yval += x_yinc;
		x_val = x_yval;
	}
}

/*	*/
void addStack(int levels, float dim, float density, float spacing, float x, float y, GeometryType type, t2World *world)
{
	float
		y_val = y + dim/2.0f + spacing,		// initial y-value
		y_inc = dim + spacing;				// y-increment

	for(int hi = 0; hi < levels; hi++)
	{
		if(type == GEOMTYPE_POLY)
		{
			t2Body *body = new t2Box(vec2f(x, y_val), dim, dim, dim*dim*density, false);
			// stack test (>10 it)
			//body->restitution = 0.25f; body->setFriction(0.25f, 0.25f);
			world->addBody(body);
		}
		else
			world->addBody(new t2Circle(vec2f(x, y_val), PI*dim*dim/4.0f*density, dim/2.0f, false));
		y_val += y_inc;
	}
}

/*	*/
void addCircleLink(vec2f leftAnchor, vec2f rightAnchor, int nbr, float radius, float density, t2World *world)
{
	vec2f v = rightAnchor - leftAnchor;			// 
	float v_len = v.norm();						// 
	vec2f
		vn = v / v_len,								// 
		pos_inc = vn * (v_len / (nbr+1)),			// position increment per link
		pos = leftAnchor + pos_inc;					// initial circle position
	t2Circle
		*cA, *cB;									// the two circles currently being linked together
	
	// create first link, connect to initial anchor
	cB = new t2Circle(pos, 1.0f, radius, false);
	cB->restitution = 0.1f; world->addBody(cB);
	world->joints.push_back(new t2RevoluteJoint(world->background, cB, leftAnchor, vec2f(-radius, 0.0f)));

	// create links, connect to previously created
	for(int i = 0; i < nbr-1; i++)
	{
		pos += pos_inc;
		cA = cB;		
		cB = new t2Circle(pos, PI*radius*radius*density, radius, false);
		cB->restitution = 0.1f; world->addBody(cB);
		world->joints.push_back(new t2RevoluteJoint(cA, cB, vec2f(radius, 0.0f), vec2f(-radius, 0.0f)));
	}
	// connect final link to final anchor
	world->joints.push_back(new t2RevoluteJoint(world->background, cB, rightAnchor, vec2f(radius, 0.0f)));
}

/*	*/
void addCompound(vec2f pos, t2World *world)
{
	// open box
	t2Body *cbody4 = new t2Body(pos + vec2f(6.0f, 0.0f), false);
	cbody4->addGeometry(new t2PolygonGeometry(vec2f(-1.5f, 0.3676f), 0.5f, 3.0f));
	cbody4->addGeometry(new t2PolygonGeometry(vec2f(1.5f, 0.3676f), 0.5f, 3.0f));
	cbody4->addGeometry(new t2PolygonGeometry(vec2f(0.0f, -0.8824f), 2.5f, 0.5f));
	//cbody4->setMass(4.25f, 11.6183f);
	cbody4->setMass(34.0f, 92.95);	// density 8
	world->addBody(cbody4);

	// star
	t2Body *cbody3 = new t2Body(pos + vec2f(2.0f, 0.0f), false);
	cbody3->addGeometry(new t2PolygonGeometry(vec2f(0.0f, 0.0f), 1.0f, 3.0f));
	cbody3->addGeometry(new t2PolygonGeometry(vec2f(0.0f, 0.0f), 3.0f, 1.0f));
	//cbody3->setMass(4.0f, 3.3333f);
	cbody3->setMass(40.0f, 33.33f);	// density 8
	world->addBody(cbody3);

	//// disjoint boxes
	//t2Body *cbody2 = new t2Body(pos + vec2f(-2.0f, 0.0f), false);
	//cbody2->addGeometry(new t2PolygonGeometry(vec2f(-2.0f, 0.0f), 1.0f, 1.0f));
	//cbody2->addGeometry(new t2PolygonGeometry(vec2f(2.0f, 0.0f), 1.0f, 1.0f));
	////cbody2->setMass(2.0f, 8.1667f);
	//cbody2->setMass(16.0f, 65.33);	// density 8
	//cbody2->R = PI/2.0f;
	//world->addBody(cbody2);

	// right angle set
	t2Body *cbody1 = new t2Body(pos + vec2f(-2.0f, 0.0f), false);
	cbody1->addGeometry(new t2PolygonGeometry(vec2f(-0.6f, 0.4f), 1.0f, 3.0f));
	cbody1->addGeometry(new t2PolygonGeometry(vec2f(0.9f, -0.6f), 2.0f, 1.0f));
	//cbody1->setMass(5.0f, 7.2333f);
	cbody1->setMass(40.0f, 57.87f);	// density 8
	world->addBody(cbody1);
}

/*	*/
void addHeart(vec2f pos, t2World *world)
{
	t2Body *heart = new t2Body(pos, false);

	t2PolygonGeometry *pl = new t2PolygonGeometry(vec2f(0.0f, 0.0f));
	pl->addVertex(vec2f(0.0f, -2.5f)); pl->addVertex(vec2f(0.0f, 1.0f));
	pl->addVertex(vec2f(-0.1f, 1.5f)); pl->addVertex(vec2f(-0.5f, 1.9f));
	pl->addVertex(vec2f(-1.0f, 2.2f));	pl->addVertex(vec2f(-1.5f, 2.2f));
	pl->addVertex(vec2f(-2.0f, 1.9f));	pl->addVertex(vec2f(-2.4f, 1.5f));
	pl->addVertex(vec2f(-2.5f, 1.0f));	pl->addVertex(vec2f(-2.5f, 0.5f));
	pl->addVertex(vec2f(-0.5f, -2.5f));

	t2PolygonGeometry *pr = new t2PolygonGeometry(vec2f(0.0f, 0.0f));
	pr->addVertex(vec2f(0.5f, -2.5f)); pr->addVertex(vec2f(2.5f, 0.5f));
	pr->addVertex(vec2f(2.5f, 1.0f)); pr->addVertex(vec2f(2.4f, 1.5f));
	pr->addVertex(vec2f(2.0f, 1.9f)); pr->addVertex(vec2f(1.5f, 2.2f));
	pr->addVertex(vec2f(1.0f, 2.2f)); pr->addVertex(vec2f(0.5f, 1.9f));
	pr->addVertex(vec2f(0.1f, 1.5f)); pr->addVertex(vec2f(0.0f, 1.0f));
	pr->addVertex(vec2f(0.0f, -2.5f));

	heart->addGeometry(pl);
	heart->addGeometry(pr);
	//heart->setMass(5.0f, 5.0f * 2.5f * 2.5f / 2.0f);
	//heart->setMass(50.0f, 10.0f * 5.0f * 2.5f * 2.5f / 2.0f);
	heart->setMassFromGeometry(5);
	world->addBody(heart);
}

void addCourse(vec2f pos, t2World *world)
{
	vec2f
		v_glob = pos,						// global course starting point
		v_next, vn;							// 

	//float segm_height = 0.5f;
	//const int steps = 50;
	//int i = 0;
	//float step_size = 2.0f;
	//vec2f vecs[2*steps+3];
	//vecs[i++] = vec2f(0.0f, 0.0f);
	//vecs[i++] = vec2f(20.0f, 0.0f);
	//for(int j = 0; j < steps; j++)
	//{
	//	vecs[i++] = vec2f(step_size, 0.0f);
	//	vecs[i++] = vec2f(0.0f, -1.0f*step_size);
	//}
	//vecs[i++] = vec2f(20.0f, 0.0f);
	
	float segm_height = 1.0f;
	vec2f
		vecs[] = {							// course vertices (each relative to the next):
			vec2f(0.0f, 0.0f),				// start depot
			vec2f(0.0f, -5.0f),

			//vec2f(20.0f, 0.0f),
			//vec2f(0.0f, 5.0f),

			vec2f(60.0f, 0.0f),			// (see-saw) 10060

//            vec2f(100000.0f, -5.0f),			// (bridge) (set x high for long stretch)
			vec2f(0.0f, -5.0f),			// (bridge)
			vec2f(10.0f, 0.0f),
			vec2f(0.0f, 5.0f),
			vec2f(10.0f, 0.0f),

			vec2f(20.0f, 5.0f),			// elevation
			vec2f(20.0f, -10.0f),
			vec2f(20.0f, -5.0f),
			vec2f(20.0f, 0.0f),
			//vec2f(20.0f, 5.0f),
			//vec2f(20.0f, 0.0f),

			vec2f(10.0f, 3.0f),			// elevation & ditch
			vec2f(15.0f, 10.0f),
			vec2f(2.0f, 0.0f),
			vec2f(0.0f, -20.0f),
			vec2f(5.0f, 0.0f),
			vec2f(0.0f, 20.0f),
			vec2f(2.0f, 0.0f),
			vec2f(15.0f, -10.0f),
			vec2f(10.0f, -3.0f),
			vec2f(20.0f, 0.0f),

			vec2f(2.0f, 0.7f),				// 4 bumps
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, -0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, 0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, -0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, 0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, -0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, 0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, -0.7f),

			vec2f(2.0f, 0.7f),				// 4 bumps
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, -0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, 0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, -0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, 0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, -0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, 0.7f),
			vec2f(1.5f, 0.0f),
			vec2f(2.0f, -0.7f),

			vec2f(20.0f, 0.0f),			// end depot
			vec2f(0.0f, 150.0f)
		};
	int	nbrVecs = sizeof(vecs) / sizeof(vec2f);

	// create course-body
	t2Body *road = new t2Body(vec2f(), true);
	road->setMass(INF, INF);
	road->setColor(0.2f, 0.2f, 0.2f);
	// stack test
	//road->restitution = 0.25f;
	//road->setFriction(0.25f, 0.25f);
	// restitution test
	//road->restitution = 1.0f;
	// friction test
	//road->setFriction(0.20f, 0.20f);

	for(int i = 0; i < nbrVecs-1; i++)
	{
		v_next = vecs[i+1];
		vn = vec2f::getNormal(v_next, vec2f()).normalize() * segm_height;

		// create road segment geometry
		t2PolygonGeometry *segment = new t2PolygonGeometry(v_glob);
		segment->addVertex(vec2f());
		segment->addVertex(-vn);
		segment->addVertex(-vn + v_next);
		segment->addVertex(v_next);
		road->addGeometry(segment);

		// see-saw
		if(i == 1)
		{
			float pos_y = 1.5f;
			t2Box *seesaw = new t2Box(v_glob + vec2f(25.0f, pos_y), 12.0f, 0.4f, 200.0f, false);
			// friction test
			seesaw->setFriction(0.20f, 0.20f);
			world->addBody(seesaw);
			world->joints.push_back(new t2ElevatedHinge(
				road, seesaw, v_glob + vec2f(25.0f, pos_y), vec2f(0.0f, -0.2f), 0.0f, PI, 
				vec2f(-0.5f, -pos_y), vec2f(0.5f, -pos_y), vec2f(0.0f, 0.0f), vec2f(0.0f, 0.0f)));

			// 50% smaller seesaw
			//float pos_y = 0.5f;
			//t2Box *seesaw = new t2Box(v_glob + vec2f(30.0f, pos_y), 5.0f, 0.2f, 200.0f, false);
			//world->addBody(seesaw);
			//world->joints.push_back(new t2ElevatedHinge(
			//	road, seesaw, v_glob + vec2f(30.0f, pos_y), vec2f(0.0f, -0.1f), 0.0f, PI, 
			//	vec2f(-0.2f, -pos_y), vec2f(0.2f, -pos_y), vec2f(0.0f, 0.0f), vec2f(0.0f, 0.0f)));
		}

		// bridge
		if(i == 2) addCircleLink(v_glob, v_glob + vec2f(10.0f, 0.0f), 16, 0.3f, 20.0f, world);

		v_glob += v_next;
	}
	world->addBody(road);
}

void addShortCourse(vec2f pos, t2World *world)
{
	vec2f
		v_glob = pos,						// global course starting point
		v_next, vn;							// 

	float segm_height = 1.0f;
	vec2f
		vecs[] = {							// course vertices (each relative to the next):
			vec2f(0.0f, 0.0f),				// start depot
			vec2f(0.0f, -5.0f),

			//vec2f(20.0f, 0.0f),
			//vec2f(0.0f, 5.0f),

			vec2f(60.0f, 0.0f)			// (see-saw) 10060
		};
	int	nbrVecs = sizeof(vecs) / sizeof(vec2f);

	// create course-body
	t2Body *road = new t2Body(vec2f(), true);
	road->setMass(INF, INF);
	road->setColor(0.2f, 0.2f, 0.2f);

	for(int i = 0; i < nbrVecs-1; i++)
	{
		v_next = vecs[i+1];
		vn = vec2f::getNormal(v_next, vec2f()).normalize() * segm_height;

		// create road segment geometry
		t2PolygonGeometry *segment = new t2PolygonGeometry(v_glob);
		segment->addVertex(vec2f());
		segment->addVertex(-vn);
		segment->addVertex(-vn + v_next);
		segment->addVertex(v_next);
		road->addGeometry(segment);

		v_glob += v_next;
	}
	world->addBody(road);
}