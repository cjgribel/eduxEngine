
/*
	Tau2D Dynamics Engine
	CJ Gribel (c) 2008-2009, cjgribel@gmail.com
*/

#pragma once
#ifndef T2UIEVENT_H
#define T2UIEVENT_H

#include "t2World.h"
#include "t2Geometry.h"
#include "t2BroadPhase.h"
#include "t2Math.h"

/*

*/
class t2UIEvent
{

public:

	t2UIEvent(t2World* world)
		: world(world), mouseLeftIsDown(false), mouseRightIsDown(false)
	{ }

	void mouseClick(vec2f p, int button, int state)
	{
		if(button == GLUT_LEFT_BUTTON)
		{
			if(state == GLUT_DOWN)
			{
				mouseLeftDown(p);
				mouseLeftIsDown = true;
			}
			else
			{
				mouseLeftUp(p);
				mouseLeftIsDown = false;
			}
		}
		else if(button == GLUT_RIGHT_BUTTON)
		{
			if(state == GLUT_DOWN)
			{
				mouseRightDown(p);
				mouseRightIsDown = true;
			}
			else
			{
				mouseRightUp(p);
				mouseRightIsDown = false;
			}
		}
	}

	virtual void mouseMove(vec2f p) { }

	virtual void keyDown(unsigned char key, vec2f p) { }

	virtual void keyUp(unsigned char key, vec2f p) { }

	virtual void reset() { }

	virtual void render() { }

	virtual ~t2UIEvent() { }

	t2World *world;

protected:

	bool mouseLeftIsDown, mouseRightIsDown;

	virtual void mouseLeftDown(vec2f p) { }

	virtual void mouseLeftUp(vec2f p) { }

	virtual void mouseRightDown(vec2f p) { }

	virtual void mouseRightUp(vec2f p) { }
};

/*

*/
class t2UIPointer : public t2UIEvent
{

public:

	t2UIPointer(t2World* world, t2Body *camera)
		: t2UIEvent(world), camera(camera)
	{
		camera_force = new t2dCameraTrackingForce(camera, world->background, vec2f(0.0f, 0.0f), camera->X, 40.0f, 10.0f, 0.0f);
		mouse_force = new t2dMouseTrackingForce(world->background, 500.0f, 85.0f);
		world->forces.push_back(camera_force);
		world->forces.push_back(mouse_force);
	}

	void mouseMove(vec2f p)
	{
		if(mouse_force->isEngaged())
		{
			mouse_force->setPointer(p);
		}
	}

	void keyDown(unsigned char key, vec2f p)
	{
		if(key == 'f' && mouse_force->isEngaged())
		{
			camera_force->bodyB = mouse_force->bodyB;
			camera_force->anchorB.set(0.0f, 0.0f);
		}
	}

protected:

	void mouseLeftDown(vec2f p)
	{
		vec2f anchor;
		t2Body *body = world->find(p);
		if(body != NULL)
		{
			anchor = mat2(-body->R) * (p - body->X);
		}
		else
		{
			body = world->background;
			anchor = p;
		}
		mouse_force->engage(body, anchor);
		mouse_force->setPointer(p);
		pA = p;
	}

	void mouseLeftUp(vec2f p)
	{
		if(mouse_force->isEngaged())
			mouse_force->disengage();
	}

	void mouseRightDown(vec2f p)
	{
		camera_force->bodyB = world->background;
		camera_force->anchorB = p;
		camera->X = p;
	}

public:

	t2Body *camera;

//private:

	vec2f pA;
	t2dCameraTrackingForce *camera_force;
	t2dMouseTrackingForce *mouse_force;
};

/*

*/
class t2UIAddBody : public t2UIEvent
{

public:

	t2UIAddBody(t2World* world)
		: t2UIEvent(world), size_eps(0.2f)
	{ }

	void mouseMove(vec2f p)
	{
		if(mouseLeftIsDown)
			pB = p;
	}

	void reset()
	{
		mouseLeftIsDown = false;
	}

	void render()
	{
		if(mouseLeftIsDown)
		{
			vec2f v = pB - pA;

			// box
			if(fabs(v.x) > size_eps && fabs(v.y) > size_eps)	glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
			else												glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
			glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
			glBegin(GL_LINE_LOOP);
			glVertex2d(pA.x, pA.y);
			glVertex2d(pA.x + v.x, pA.y);
			glVertex2d(pB.x, pB.y);
			glVertex2d(pA.x, pA.y + v.y);
			glEnd();

			// circle
//			int steps = 18;
//			float rad = 0.0f, drad = 2.0f * PI/steps, radius = v.norm()/2.0f;
//			vec2f center = pA + v*0.5f;
//			if(fabs(v.x) > size_eps || fabs(v.y) > size_eps)	glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
//			else												glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
//			glBegin(GL_LINE_LOOP);
//			for(int k = 0; k < steps; k++)
//			{
//				glVertex2d(center.x + radius*cos(rad), center.y + radius*sin(rad));
//				rad += drad;
//			}
//			glEnd();
		}
	}

protected:

	void mouseLeftDown(vec2f p)
	{
		pA = pB = p;
	}

	void mouseLeftUp(vec2f p)
	{
		if(mouseLeftIsDown)
		{
			vec2f v = p - pA;

			// box
			if(fabs(v.x) > size_eps && fabs(v.y) > size_eps)
			{
				t2Box *box = new t2Box(pA + v*0.5f, fabs(v.x), fabs(v.y), 10.0f);
				box->setMassFromGeometry( 8.0f );
				world->addBody(box);
			}

			// circle
//			if(fabs(v.x) > size_eps || fabs(v.y) > size_eps)
//			{
//				t2Circle *circle = new t2Circle(pA + v*0.5f, v.norm()/2.0f, 10.0f);
//				circle->setMassFromGeometry( 10.0f );
//				//circle->V.set(0.0f, 0.0f); circle->W = 0.0f;	// sometimes the circles get ghost velocities...
//				//circle->setColor(0.5f, 0.25f, 0.0f);
//				world->addBody(circle);
//				world->printStats();
//			}
		}
	}

private:

	vec2f pA, pB;
	const float size_eps;
};

/*

*/
class t2UIAddPolygon : public t2UIEvent
{
public:

	t2UIAddPolygon(t2World* world)
		: t2UIEvent(world), v_move(), proximity_eps(0.5f), vmax(100)
	{
		vec = new vec2f[vmax];
		reset();
	}

	void reset()
	{
		vc = 0;
	}

	void mouseMove(vec2f p)
	{
		v_move = p;
	}

	void render()
	{
		if( vc == 0 ) return;

		/* render polygon */
		glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
		glBegin(GL_LINE_STRIP);
		for( int i = 0; i < vc; i++ )
			glVertex2d( vec[i].x, vec[i].y );
		glEnd();

		/* highlight first vertex if eligible for closure */
		vec2f v0 = vec[0];
		bool near_start_vertex = vec2f::getNorm( v_move - v0 ) < proximity_eps;
		if (near_start_vertex && vc > 2)
		{
			glBegin(GL_POLYGON);
			glVertex2d(v0.x - 0.1f, v0.y - 0.1f);
			glVertex2d(v0.x + 0.1f, v0.y - 0.1f);
			glVertex2d(v0.x + 0.1f, v0.y + 0.1f);
			glVertex2d(v0.x - 0.1f, v0.y + 0.1f);
			glEnd();
		}

		/* render last edge */
		if( yield_intersection( v_move ) )	glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
		glBegin(GL_LINE_STRIP);
		glVertex2d( vec[vc-1].x, vec[vc-1].y );
		glVertex2d(v_move.x, v_move.y);
		glEnd();
	}

	~t2UIAddPolygon()
	{
		delete vec;
	}

protected:

	void mouseLeftDown(vec2f p)
	{
		if ( vc < 3 )
		{
			add_vertex(p);
		}
		else
		{
			if ( yield_intersection( p ) )
				return;

			bool near_start_vertex = vec2f::getNorm( p - vec[0] ) < proximity_eps;

			if ( near_start_vertex )
			{
				t2Body *body = new t2Body( vec2f(), false );
				triangulate_polygon( vec, vc, body );
				vec2f vdiff = body->setMassFromGeometry( 5.0f );
				body->setOutline( vec, vc, vdiff );
				world->addBody( body );

				//reset();
			}
			else
			{
				add_vertex(p);
			}
		}
	}

	void mouseRightDown(vec2f p)
	{
		reset();
	}

private:

	void add_vertex( vec2f v )
	{
		if ( vc < vmax )
			vec[vc++] = v;
	}

	bool yield_intersection( vec2f p )
	{
		for (int i = 0; i < vc-2; i++)
		{
			if ( intersect( p, vec[vc-1], vec[i], vec[i+1] ) )
				return true;
		}
		return false;
	}

	bool intersect( vec2f u1, vec2f u2, vec2f v1, vec2f v2 )
	{
		vec2f	v		= v2 - v1,
				v1u1	= u1 - v1,
				v1u2	= u2 - v1;
		if( (v % v1u1) * (v % v1u2) < 0.0f )
		{
			vec2f	vn = vec2f( -v.y, v.x );
			float	dot1 = abs( vn.dot( v1u1 ) ),
					dot2 = abs( vn.dot( v1u2 ) );
			vec2f	cp = u1 + (u2 - u1) * dot1/(dot1+dot2);

			if ( v.dot( cp - v1 ) > 0.0f && v.dot( cp - v2 ) < 0.0f  )
				return true;
		}
		return false;
	}

	vec2f v_move;
	float proximity_eps;
	int vmax;
	vec2f *vec;
	int vc;
};

/*

*/
#if 0
class t2UIAddPolygon : public t2UIEvent
{

public:

	t2UIAddPolygon(t2World* world)
		: t2UIEvent(world), p_move(), acc_angle(0.0f), proximity_eps(0.5f)
	{
		reset();
	}

	void reset()
	{
		poly_geom = new t2PolygonGeometry();
		acc_angle = 0.0f;
	}

	void mouseMove(vec2f p)
	{
		p_move = p;
	}

	void render()
	{
		if( poly_geom->nbrVertices == 0 ) return;

		/* render polygon */
		glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
		glBegin(GL_LINE_STRIP);
		for( int i = 0; i < poly_geom->nbrVertices; i++ )
			glVertex2d( poly_geom->vertices_local[i].x, poly_geom->vertices_local[i].y );
		glEnd();

		/* highlight first vertex if eligible for closure */
		vec2f v0 = poly_geom->vertices_local[0];
		bool near_start_vertex = vec2f::getNorm( p_move - v0 ) < proximity_eps;
		if (near_start_vertex && poly_geom->nbrVertices > 2)
		{
			glBegin(GL_POLYGON);
			glVertex2d(v0.x - 0.1f, v0.y - 0.1f);
			glVertex2d(v0.x + 0.1f, v0.y - 0.1f);
			glVertex2d(v0.x + 0.1f, v0.y + 0.1f);
			glVertex2d(v0.x - 0.1f, v0.y + 0.1f);
			glEnd();
		}

		/* render last edge */
		if( !isConvex(p_move) )	glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
		glBegin(GL_LINE_STRIP);
		glVertex2d( poly_geom->vertices_local[poly_geom->nbrVertices-1].x, poly_geom->vertices_local[poly_geom->nbrVertices-1].y );
		glVertex2d(p_move.x, p_move.y);
		glEnd();
	}

protected:

	void mouseLeftDown(vec2f p)
	{
		if ( poly_geom->nbrVertices == 0 )
		{
			addVertex(p);
		}
		else
		{
			bool near_start_vertex = vec2f::getNorm( p - poly_geom->vertices_local[0] ) < proximity_eps;
			if ( !near_start_vertex )
			{
				addVertex(p);
			}
			else if ( poly_geom->nbrVertices > 2 )
			{
				t2Body *body = new t2Body( vec2f(), false );
				body->addGeometry( poly_geom );
				body->setMassFromGeometry( 5.0f );
				world->addBody( body );
				//printf("new polygon area %1.2f\n", body_area(body->geometries, body->nbrGeometries));

				reset();
			}
		}
	}

	void mouseRightDown(vec2f p)
	{
		reset();
	}

	void addVertex(vec2f p)
	{
		if ( isConvex(p) )
		{
			if( poly_geom->nbrVertices > 1 )
			{
				vec2f	v1 = poly_geom->vertices_local[poly_geom->nbrVertices-1] - poly_geom->vertices_local[poly_geom->nbrVertices-2],
						v2 = p - poly_geom->vertices_local[poly_geom->nbrVertices-1];
				acc_angle += vec2f::getAngle(v1,v2);
			}
			poly_geom->addVertex( p );
		}
	}

	bool isConvex(vec2f p)
	{
		if( poly_geom->nbrVertices > 1 )
		{
			vec2f	v1 = poly_geom->vertices_local[poly_geom->nbrVertices-1] - poly_geom->vertices_local[poly_geom->nbrVertices-2],
					v2 = p - poly_geom->vertices_local[poly_geom->nbrVertices-1],
					v3 = poly_geom->vertices_local[0] - p;

			float	angle1 = vec2f::getAngle(v1,v2),
					angle2 = vec2f::getAngle(v2,v3);
			bool	dir1_ok = vec2f::cross( v1, v2 ) >= -0.001f,
					dir2_ok = vec2f::cross( v2, v3 ) >= -0.001f;

			return	acc_angle + angle1 + angle2 <= 2*PI + 0.01f &&
					dir1_ok &&
					dir2_ok;
		}
		else return true;
	}

private:

	t2PolygonGeometry *poly_geom;
	vec2f p_move;
	float acc_angle, proximity_eps;
};
#endif

#if 0
/*

*/
class t2UIAddConcavePolygon : public t2UIEvent
{

public:

	t2UIAddConcavePolygon(t2World* world)
		: t2UIEvent(world), p_move(), acc_angle(0.0f), proximity_eps(0.5f)
	{
		reset();
	}

	void reset()
	{
		poly_body = new t2Body( vec2f(), false );
		poly_geom = new t2PolygonGeometry();
		acc_angle = 0.0f;
	}

	void mouseMove(vec2f p)
	{
		p_move = p;
	}

	void render()
	{
		if( poly_geom->nbrVertices == 0 ) return;

		/* render polygon */
		glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
		glBegin(GL_LINE_STRIP);
		for( int i = 0; i < poly_geom->nbrVertices; i++ )
			glVertex2d( poly_geom->vertices_local[i].x, poly_geom->vertices_local[i].y );
		glEnd();

		/* highlight first vertex if eligible for closure */
		vec2f v0 = poly_geom->vertices_local[0];
		bool near_start_vertex = vec2f::getNorm( p_move - v0 ) < proximity_eps;
		if (near_start_vertex && poly_geom->nbrVertices > 2)
		{
			glBegin(GL_POLYGON);
			glVertex2d(v0.x - 0.1f, v0.y - 0.1f);
			glVertex2d(v0.x + 0.1f, v0.y - 0.1f);
			glVertex2d(v0.x + 0.1f, v0.y + 0.1f);
			glVertex2d(v0.x - 0.1f, v0.y + 0.1f);
			glEnd();
		}

		/* render last edge */
		if( !isConvex(p_move) )	glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
		glBegin(GL_LINE_STRIP);
		glVertex2d( poly_geom->vertices_local[poly_geom->nbrVertices-1].x, poly_geom->vertices_local[poly_geom->nbrVertices-1].y );
		glVertex2d(p_move.x, p_move.y);
		glEnd();
	}

protected:

	void mouseLeftDown(vec2f p)
	{
		if ( poly_geom->nbrVertices == 0 )
		{
			//addVertex(p);
			poly_geom->addVertex( p );
		}
		else
		{
			bool	near_prev_vertex = vec2f::getNorm( p - poly_geom->vertices_local[ poly_geom->nbrVertices-1 ] ) < proximity_eps,
					near_start_vertex = vec2f::getNorm( p - poly_geom->vertices_local[0] ) < proximity_eps;

			if ( poly_geom->nbrVertices == 1 )
			{
				if ( !near_start_vertex )
					poly_geom->addVertex( p );
			}
			else
			{
				if ( near_prev_vertex )
				{
					return;
				}
				else if ( !near_start_vertex )
				{
					addVertex(p);
				}
				else if ( poly_geom->nbrVertices > 2 )
				{
					//t2Body *body = new t2Body( vec2f(), false );
					poly_body->addGeometry( poly_geom );
					poly_body->setMassFromGeometry( 5.0f );
					world->addBody( poly_body );

					reset();
				}

			}
		}

			//if ( near_prev_vertex )
			//{
			//	return;
			//}
			//else if ( !near_start_vertex )
			//{
			//	addVertex(p);
			//}
			//else if ( poly_geom->nbrVertices > 2 )
			//{
			//	//t2Body *body = new t2Body( vec2f(), false );
			//	poly_body->addGeometry( poly_geom );
			//	poly_body->setMassFromGeometry( 5.0f );
			//	world->addBody( poly_body );

			//	reset();
			//}
		//}
	}

	void mouseRightDown(vec2f p)
	{
		reset();
	}

	/* >= 2 vertices */
	void addVertex(vec2f p)
	{
		vec2f	v1 = poly_geom->vertices_local[poly_geom->nbrVertices-1] - poly_geom->vertices_local[poly_geom->nbrVertices-2],
				v2 = p - poly_geom->vertices_local[poly_geom->nbrVertices-1],
				v3 = poly_geom->vertices_local[0] - p;

		float	angle1 = vec2f::getAngle(v1,v2),
				angle2 = vec2f::getAngle(v2,v3),
				angle_tot = acc_angle + angle1 + angle2;

		bool	dir1_ok = vec2f::cross( v1, v2 ) >= -0.001f,
				dir2_ok = vec2f::cross( v2, v3 ) >= -0.001f;

		if ( ! (dir1_ok && angle_tot < 2.0f*PI + 0.01f) )
		{
			poly_body->addGeometry( poly_geom );

			t2PolygonGeometry *new_geom = new t2PolygonGeometry();
			new_geom->addVertex( poly_geom->vertices_local[0] );
			new_geom->addVertex( poly_geom->vertices_local[ poly_geom->nbrVertices-1 ] );
			new_geom->addVertex( p );
			poly_geom = new_geom;

			acc_angle = vec2f::getAngle(	poly_geom->vertices_local[1] - poly_geom->vertices_local[0],
											poly_geom->vertices_local[2] - poly_geom->vertices_local[1] );
		}
		else if ( dir2_ok && angle_tot < 2.0f*PI + 0.01f )
		{
			poly_geom->addVertex( p );
			acc_angle += angle1;
		}


		//if ( isConvex(p) )
		//{
		//	if( poly_geom->nbrVertices > 1 )
		//	{
		//		vec2f	v1 = poly_geom->vertices_local[poly_geom->nbrVertices-1] - poly_geom->vertices_local[poly_geom->nbrVertices-2],
		//				v2 = p - poly_geom->vertices_local[poly_geom->nbrVertices-1];
		//		acc_angle += vec2f::getAngle(v1,v2);
		//	}
		//	poly_geom->addVertex( p );
		//}
		// else: 
	}

	bool isConvex(vec2f p)
	{
		if( poly_geom->nbrVertices > 1 )
		{
			vec2f	v1 = poly_geom->vertices_local[poly_geom->nbrVertices-1] - poly_geom->vertices_local[poly_geom->nbrVertices-2],
					v2 = p - poly_geom->vertices_local[poly_geom->nbrVertices-1],
					v3 = poly_geom->vertices_local[0] - p;

			float	angle1 = vec2f::getAngle(v1,v2),
					angle2 = vec2f::getAngle(v2,v3);
			bool	dir1_ok = vec2f::cross( v1, v2 ) >= -0.001f,
					dir2_ok = vec2f::cross( v2, v3 ) >= -0.001f;

			return	acc_angle + angle1 + angle2 <= 2*PI + 0.01f &&
					dir1_ok &&
					dir2_ok;
		}
		else return true;
	}

private:

	t2Body *poly_body;
	t2PolygonGeometry *poly_geom;
	vec2f p_move;
	float acc_angle, proximity_eps;
};
#endif

/*

*/
class t2UIAddSpring : public t2UIEvent
{

public:

	t2UIAddSpring(t2World* world, float K, float D, float coil_amplitude)
		: t2UIEvent(world), K(K), D(D), coil_amplitude(coil_amplitude)
	{
		spring_dummy = new t2CoilSpring(NULL, world->background, vec2f(), vec2f(), 0.0f, 0.0f, 0.0f, 0, coil_amplitude);
	}

	void mouseMove(vec2f p)
	{
		if(mouseLeftIsDown)
		{
			pB = p;
			spring_dummy->anchorB = p;
			spring_dummy->periods = (int)(floor(vec2f::getNorm(p - pA)) * 2.0f) + 3;
		}
	}

	~t2UIAddSpring()
	{
		delete spring_dummy;
	}

protected:

	void mouseLeftDown(vec2f p)
	{
		findBody(p, &bodyA, anchorA);
		pA = pB = p;

		spring_dummy->bodyA = bodyA;
		spring_dummy->anchorA = anchorA;
		spring_dummy->anchorB = p;
		world->forces.push_back(spring_dummy);
	}

	void mouseLeftUp(vec2f p)
	{
		if(mouseLeftIsDown)
		{
			findBody(p, &bodyB, anchorB);
			if(bodyA != bodyB)
			{
				pA = bodyA->X + mat2(bodyA->R) * anchorA;
				float len = vec2f::getNorm(p - pA);
				world->forces.push_back(
					new t2CoilSpring(bodyA, bodyB, anchorA, anchorB, K, D, len, spring_dummy->periods, coil_amplitude));
			}
			world->forces.remove(spring_dummy);
		}
	}

private:

	void findBody(vec2f p, t2Body **body_ret, vec2f &anchor_ret)
	{
		*body_ret = world->find(p);
		if(*body_ret != NULL)
		{
			anchor_ret = mat2(-(*body_ret)->R) * (p - (*body_ret)->X);
		}
		else
		{
			*body_ret = world->background;
			anchor_ret = p;
		}
	}

	t2Body *bodyA, *bodyB;
	vec2f pA, pB, anchorA, anchorB;
	float K, D, coil_amplitude;
	t2CoilSpring *spring_dummy;
};

/*

*/
class t2UIAddDistanceJoint : public t2UIEvent
{

public:

	t2UIAddDistanceJoint(t2World* world)
		: t2UIEvent(world)
	{ }

	void mouseMove(vec2f p)
	{
		if(mouseLeftIsDown)
		{
			pB = p;
		}
	}

	void render()
	{
		if(mouseLeftIsDown)
		{
			pA = bodyA->X + mat2(bodyA->R) * anchorA;

			glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
			glBegin(GL_LINES);
			glVertex2d(pA.x, pA.y);
			glVertex2d(pB.x, pB.y);
			glEnd();
		}
	}

protected:

	void mouseLeftDown(vec2f p)
	{
		findBody(p, &bodyA, anchorA);
		pA = pB = p;
	}

	void mouseLeftUp(vec2f p)
	{
		if(mouseLeftIsDown)
		{
			findBody(p, &bodyB, anchorB);
			if(bodyA != bodyB)
			{
				pA = bodyA->X + mat2(bodyA->R) * anchorA;
				float len = vec2f::getNorm(p - pA);
				world->joints.push_back(
					new t2DistanceJoint(bodyA, bodyB, anchorA, anchorB, len));
			}
		}
	}

private:

	void findBody(vec2f p, t2Body **body_ret, vec2f &anchor_ret)
	{
		*body_ret = world->find(p);
		if(*body_ret != NULL)
		{
			anchor_ret = mat2(-(*body_ret)->R) * (p - (*body_ret)->X);
		}
		else
		{
			*body_ret = world->background;
			anchor_ret = p;
		}
	}

	t2Body *bodyA, *bodyB;
	vec2f pA, pB, anchorA, anchorB;
};

/*

*/
class t2UIUnifyBodies : public t2UIEvent
{
public:

	t2UIUnifyBodies(t2World* world)
		: t2UIEvent(world), sel_body()
	{ }

	void reset()
	{
		sel_body = NULL;
	}

	void render()
	{
		if (sel_body != NULL)
		{
			glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
			glBegin(GL_POLYGON);
			glVertex2d(sel_body->X.x - 0.3f, sel_body->X.y - 0.3f);
			glVertex2d(sel_body->X.x + 0.3f, sel_body->X.y - 0.3f);
			glVertex2d(sel_body->X.x + 0.3f, sel_body->X.y + 0.3f);
			glVertex2d(sel_body->X.x - 0.3f, sel_body->X.y + 0.3f);
			glEnd();
		}
	}

protected:

	void mouseLeftDown(vec2f p)
	{
		t2Body *body = world->find(p);

		if (body == NULL || body == world->background) return;

		if (sel_body == NULL)
		{
			sel_body = body;
		}
		else
		{
			if (body == sel_body) return;

			for (int i = 0; i < body->nbrGeometries; i++ )
			{
				t2Geometry *geom = body->geometries[i];
				geom->transformLocal(mat2(body->R - sel_body->R));
				geom->offset = mat2(-sel_body->R) * (body->X + mat2(body->R) * geom->offset - sel_body->X);
				sel_body->addGeometry( geom );
			}
			vec2f COM_dir = sel_body->setMassFromGeometry( 8.0f );

			/* hack away... */
			/* remove body from world */
			int i = 0;
			while (world->bodies[i] != body) i++;
			for (; i < world->nbrBodies - 1; i++) world->bodies[i] = world->bodies[i+1];
			world->nbrBodies--;
			/* edit associated parent body in SAP geom lists */
			for (int j = 0; j < world->sweepPrune->nbrElements; j++)
			{
				if ( world->sweepPrune->list_x[j].body == body )	world->sweepPrune->list_x[j].body = sel_body;
				if ( world->sweepPrune->list_y[j].body == body )	world->sweepPrune->list_y[j].body = sel_body;
			}
			/* edit associated parent body among SAP candidates */
			for(	std::list<t2ElementPair>::iterator c_it = world->sweepPrune->candidates.begin();
					c_it != world->sweepPrune->candidates.end();
					c_it++)
			{
				if ( c_it->elemA.body == body )	c_it->elemA.body = sel_body;
				if ( c_it->elemB.body == body )	c_it->elemB.body = sel_body;
			}
			/* find and adjust constraint bodies & anchors */
			for(	std::list<t2Joint*>::iterator j_it = world->joints.begin();
					j_it != world->joints.end();
					j_it++)
			{
				if ( (*j_it)->bodyA == sel_body )
				{
					(*j_it)->anchorA -= COM_dir;
				}
				else if ( (*j_it)->bodyA == body )
				{
					(*j_it)->bodyA = sel_body;
					(*j_it)->anchorA = mat2(-sel_body->R) * (body->X + mat2(body->R) * (*j_it)->anchorA - sel_body->X);
				}

				if ( (*j_it)->bodyB == sel_body )
				{
					(*j_it)->anchorB -= COM_dir;
				}
				else if ( (*j_it)->bodyB == body )
				{
					(*j_it)->bodyB = sel_body;
					(*j_it)->anchorB = mat2(-sel_body->R) * (body->X + mat2(body->R) * (*j_it)->anchorB - sel_body->X);
				}
			}
			/* find and adjust force bodies & anchors */
			for(	std::list<t2Force*>::iterator f_it = world->forces.begin();
					f_it != world->forces.end();
					f_it++)
			{
				if ( (*f_it)->bodyA == sel_body )
				{
					(*f_it)->anchorA -= COM_dir;
				}
				else if ( (*f_it)->bodyA == body )
				{
					(*f_it)->bodyA = sel_body;
					(*f_it)->anchorA = mat2(-sel_body->R) * (body->X + mat2(body->R) * (*f_it)->anchorA - sel_body->X);
				}

				if ( (*f_it)->bodyB == sel_body )
				{
					(*f_it)->anchorB -= COM_dir;
				}
				else if ( (*f_it)->bodyB == body )
				{
					(*f_it)->bodyB = sel_body;
					(*f_it)->anchorB = mat2(-sel_body->R) * (body->X + mat2(body->R) * (*f_it)->anchorB - sel_body->X);
				}
			}
			// we want to do this...
			// delete body;

			t2Body b(vec2f(), false);
			t2Body c = b;
			//t2Body *c = new t2Body(vec2f(), false);;
			//*c = b;
		}
	}

	void mouseRightDown(vec2f p)
	{
		reset();
	}

private:

	t2Body *sel_body;
};

#endif /* T2UIEVENT_H */