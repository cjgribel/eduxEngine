
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

#include "t2Body.h"
#include "t2World.h"
#include "t2Geometry.h"
#include <stdlib.h>

t2Body::t2Body() :
    X(), V(), F(), R(), W(), T(), restitution(T2_DEFAULT_BODY_RESTITUTION), gravity(T2_G_x, T2_G_y), isStatic(false), framesAtRest(0),nbrGeometries(0), hasOutline(false)
{
    setMass(1.0f, 1.0f);
	setFriction(T2_DEFAULT_BODY_KIN_FRICTION, T2_DEFAULT_BODY_STAT_FRICTION);
    
	collisionGroupId = T2_COLLISION_GROUP_DEFAULT;	// set default collision group
	collisionFilter = T2_COLLISION_GROUP_ALL;		// collide with everything by default
    
    setColor((float)(rand() % 8)/10.0f, (float)(rand() % 8)/10.0f, (float)(rand() % 8)/10.0f);
}

t2Body::t2Body(vec2f pos, bool isStatic) :

	X(pos), V(), F(), R(), W(), T(), restitution(T2_DEFAULT_BODY_RESTITUTION), gravity(T2_G_x, T2_G_y), isStatic(isStatic), framesAtRest(0), nbrGeometries(0),
	hasOutline(false)
{
	setMass(1.0f, 1.0f);
	setFriction(T2_DEFAULT_BODY_KIN_FRICTION, T2_DEFAULT_BODY_STAT_FRICTION);

	collisionGroupId = T2_COLLISION_GROUP_DEFAULT;	// set default collision group
	collisionFilter = T2_COLLISION_GROUP_ALL;		// collide with everything by default

	setColor((float)(rand() % 8)/10.0f, (float)(rand() % 8)/10.0f, (float)(rand() % 8)/10.0f);
}

//t2Body::t2Body(const t2Body &body)
//{
//}

void t2Body::setMass(float mass, float I)
{
	this->mass = mass;
	this->imass = 1.0f / mass;
	
	this->I = I;
	this->iI = 1.0f / I;
}

vec2f t2Body::setMassFromGeometry(float density)
{
	setMass(	body_mass(geometries, nbrGeometries, density),
				body_I(geometries, nbrGeometries, density));

	/* compute true COM, relative the body pos (X) */
	vec2f body_COM_rel = body_COM(geometries, nbrGeometries);

	/* adjust body pos & geometry offsets to true COM */
	for( int i = 0; i < nbrGeometries; i++ )
		geometries[i]->offset -= body_COM_rel;

	/* adjust outline to true COM */
	if( hasOutline )
	{
		for( int i = 0; i < vc_outline; i++ )
			vec_outline[i] -= body_COM_rel;
	}

	/* take rotation into account when adjusting the pos */
	X += mat2(R) * body_COM_rel;

	return body_COM_rel;
}

void t2Body::setFriction(float kinetic_friction, float static_friction)
{
	this->kinetic_friction = kinetic_friction;
	this->static_friction = static_friction;
}

void t2Body::addGeometry(t2Geometry* geom)
{
	if(nbrGeometries < T2_MAX_GEOMETRIES)
		geometries[nbrGeometries++] = geom;
}

void t2Body::applyImpulse(vec2f P, vec2f r)
{
	if(isStatic) return;

	V += P * imass;
	W += (r % P) * iI;
}

void t2Body::applyForce(vec2f F, vec2f r)
{
	if(isStatic) return;

	this->F += F;
	T += r % F;		
}

void t2Body::setColor(float R, float G, float B)
{
	colorFillR = R;
	colorFillG = G;
	colorFillB = B;
	colorBorderR = R/2.0f;
	colorBorderG = G/2.0f;
	colorBorderB = B/2.0f;
}

/* note: vdiff is simply the offset computed by setMassFromGeometry, added here for convenience  */
void t2Body::setOutline( vec2f *vec, const int &vc, const vec2f &vdiff )
{
	vc_outline = vc;
	vec_outline = new vec2f[vc_outline];
	for( int i = 0; i < vc_outline; i++ )
		vec_outline[i] = vec[i] - vdiff;
	hasOutline = true;
}

t2Body::~t2Body(void)
{
	for (int i = 0; i < nbrGeometries; i++)
		delete geometries[i];

	if (hasOutline)
		delete vec_outline;
}

t2Box::t2Box(vec2f pos, float width, float height, float density)
	: t2Body(pos, false)
{
	// create geometry
	addGeometry(new t2PolygonGeometry(vec2f(), width, height));

	// rectangle mass: rho*w*h, moment of inertia: m/12*(w^2+h^2)
	float mass_tmp = density * width * height;
	float I_tmp = mass_tmp/12.0f * (width*width + height*height);
	setMass(mass_tmp, I_tmp);
}

// deprecated
t2Box::t2Box(vec2f pos, float width, float height, float mass, bool isStatic)
	: t2Body(pos, isStatic)
{
	// create geometry
	addGeometry(new t2PolygonGeometry(vec2f(), width, height));

	// moment of inertia for rectangle: I = m/12*(w^2 + h^2)
	float I_tmp = mass/12.0f * (width*width + height*height);
	setMass(mass, I_tmp);
}

t2dRegularPoly::t2dRegularPoly(vec2f pos, int sides, float radius, float density)
	: t2Body(pos, false)
{
	// create geometry
	t2PolygonGeometry *pgeom = new t2PolygonGeometry(vec2f(), sides, radius);
	addGeometry(pgeom);

	// mass & moment of inertia for polygon
	float mass_tmp = density * poly_area(pgeom->vertices_local, sides);
	float I_tmp = poly_I(pgeom->vertices_local, sides, density);
	setMass(mass_tmp, I_tmp);
}

// deprecated
t2dRegularPoly::t2dRegularPoly(vec2f pos, float mass, int sides, float radius, bool isStatic)
	: t2Body(pos, isStatic)
{
	// moment of inertia for polygon (approx as circle disc): m/2 * r^2
	I = mass * radius*radius/2.0f;
	setMass(mass, I);

	// create geometry
	addGeometry(new t2PolygonGeometry(vec2f(), sides, radius));
}

t2Circle::t2Circle(vec2f pos, float radius, float density)
	: t2Body(pos, false)
{
	// circle: mass: rho*PI*r^2, moment of inertia: m/2*r^2
	float mass_tmp = density * PI * radius*radius;
	float I_tmp = mass_tmp/2.0f * radius*radius;
	setMass(mass_tmp, I_tmp);

	// create geometry
	addGeometry(new t2CircleGeometry(vec2f(0.0f, 0.0f), radius));
}

// deprecated
t2Circle::t2Circle(vec2f pos, float mass, float radius, bool isStatic)
	: t2Body(pos, isStatic)
{
	// moment of inertia for circle disc: m/2 * r^2
	I = mass * radius*radius/2.0f;
	setMass(mass, I);

	// create geometry
	addGeometry(new t2CircleGeometry(vec2f(), radius));
}

t2dStudWheel::t2dStudWheel(vec2f pos, float density, float radius, int nbrStuds, float studHeight)
	: t2Body(pos, false)
{
	float
		stud_frac = 3.0f/4.0f,							// fraction of the stud + spacing arc dedicated to stud (<=1)
		phi = 0.0f,										// initial angle
		phi_inc = 2.0f*PI / nbrStuds,					// angle increment per stud
		phi_stud = stud_frac * phi_inc,					// arc angle per stud
		studWidth = 2.0f * radius * sin(phi_stud/2.0f);	// stud width
	vec2f
		r_init(radius * cos(phi_stud/2.0f) + studHeight/2.0f, 0.0f);
														// initial radial stud position

	float
		circle_mass = PI * radius*radius * density,		// inner circle mass
		circle_I = circle_mass * radius*radius / 2.0f,	// inner circle moment of inertia
		stud_mass = studWidth * studHeight * density,	// mass per stud
		stud_I = stud_mass/12.0f * (studWidth*studWidth + studHeight*studHeight) + stud_mass * r_init.normSquared();
														// moment of inertia per stud

	// set mass and moment of inertia for inner circle + all studs
	setMass(circle_mass + nbrStuds * stud_mass, circle_I + nbrStuds * stud_I);

	// add inner circle
	addGeometry(new t2CircleGeometry(vec2f(0.0f, 0.0f), radius));

	// add studs (rectangles translated to the exterior of the inner circle)
	for(int i = 0; i < nbrStuds; i++)
	{
		mat2 rot(phi);
		vec2f offset = rot * r_init;

		t2PolygonGeometry *pgeom = new t2PolygonGeometry(offset, studHeight, studWidth);
		pgeom->transformLocal(rot);
		addGeometry(pgeom);

		phi += phi_inc;
	}
}