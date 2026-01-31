
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2DBODY_H
#define T2DBODY_H

#include "tau2d.h"
#include "config.h"

class t2Geometry;

/*
	generic, geometry-less body
*/
class t2Body
{
public:

	vec2f			X,									// center of mass (COM) (m)
					V,									// linear velocity (m/s)
					F,									// net force acting on body (N)
					gravity;							// gravitational force acting on body (N)
	float			R,									// rotation (rad)
					W,									// angular velocity (rad/s)
					T,									// net torque acting on body (Nm)
					mass, imass,						// mass (kg); inverted
					I, iI,								// moment of inertia (MOI) at COM (kg*m^2); inverted
					kinetic_friction, static_friction,	// kinetic & static coefficient of friction (unitless)
					restitution;						// coefficient of restitution (unitless)
	unsigned short	collisionGroupId,					// collision group ID
					collisionFilter;					// collision filter (toggle which groups to collide with)
	bool			isStatic;							// mobility of body
	t2Geometry*		geometries[T2_MAX_GEOMETRIES];		// sets of geometry
	int				nbrGeometries;						// 
	//const int		maxGeometries;						// 
	int				framesAtRest;						// 

	float			colorFillR, colorFillG, colorFillB,			// 
					colorBorderR, colorBorderG, colorBorderB;	// 
	vec2f			*vec_outline;
	int				vc_outline;
	bool			hasOutline;
    
    t2Body();

	t2Body(vec2f pos, bool isStatic);

	void setMass(float mass, float I);

	vec2f setMassFromGeometry(float density);

	void setFriction(float kinetic_friction, float static_friction);

	void addGeometry(t2Geometry* geom);

	void applyImpulse(vec2f P, vec2f r);

	void applyForce(vec2f F, vec2f r);

	void setColor(float R, float G, float B);

	/* note: vdiff is simply the offset computed by setMassFromGeometry, added here for convenience  */
	void setOutline( vec2f *vec, const int &vc, const vec2f &vdiff );

	~t2Body(void);
};

/*
	rectangle body
*/
class t2Box : public t2Body
{
public:

	t2Box(vec2f pos, float width, float height, float density);

	// deprecated
	t2Box(vec2f pos, float width, float height, float mass, bool isStatic);
};

/*
	regular polygon body
*/
class t2dRegularPoly : public t2Body
{
public:

	t2dRegularPoly(vec2f pos, int sides, float radius, float density);

	// deprecated
	t2dRegularPoly(vec2f pos, float mass, int sides, float radius, bool isStatic);
};

/*
	circle body
*/
class t2Circle : public t2Body
{
public:

	t2Circle(vec2f pos, float radius, float density);

	// deprecated
	t2Circle(vec2f pos, float mass, float radius, bool isStatic);
};

/*
	circle with studs
*/
class t2dStudWheel : public t2Body
{
public:

	t2dStudWheel(vec2f pos, float density, float radius, int nbrStuds, float studHeight);
};

#endif /* T2DBODY_H */