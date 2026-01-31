
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

#pragma once
#ifndef T2WORLDCONTENT_H
#define T2WORLDCONTENT_H

#include "t2Body.h"
#include "t2Joint.h"
#include "t2Constraint.h"
#include "t2Geometry.h"

class t2World;
struct vec2f;
//enum GeometryType;
class t2CoilSpring;

/*
	add miscellaneous world content
*/
void addMisc(t2World *world);

/*
	add a pyramid of bodies
*/
void addPyramid(int levels, float dim, bool rand_dim, float density, float spacing,float x, float y, GeometryType type, t2World *world);

/*
	add a stack of bodies
*/
void addStack(int levels, float dim, float density, float spacing, float x, float y, GeometryType type, t2World *world);

/*
	add a link of connected circles
*/
void addCircleLink(vec2f leftAnchor, vec2f rightAnchor, int nbr, float radius, float density, t2World *world);

/*
	add some compund bodies
*/
void addCompound(vec2f pos, t2World *world);

/*
	add heart-shaped body
*/
void addHeart(vec2f pos, t2World *world);

/*
	add a course made of road segments
*/
void addCourse(vec2f pos, t2World *world);

void addShortCourse(vec2f pos, t2World *world);

#endif /* T2WORLDCONTENT_H */