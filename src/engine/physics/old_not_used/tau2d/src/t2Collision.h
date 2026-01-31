
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

#pragma once
#ifndef T2DCOLLISION_H
#define T2DCOLLISION_H

#include "tau2d.h"

class t2World;
class t2Body;
class t2Geometry;
class t2PolygonGeometry;
class t2CircleGeometry;
struct vec2f;

/*
	perform a crude O(N^2) pass of collision detection
*/
void t2dCDWorld(t2World* world);

/*
	test two geometries for collision
*/
int t2dCDPair(t2Body *bodyA, t2Body *bodyB, t2World* world);

/*
	distance between vertex and edge plane
*/
float DistVertexEdge(vec2f &v, vec2f &ev, vec2f &en);

/*
	min separating axis for a given edge and a polygon
*/
float MinSeparationEdgePoly(vec2f &ev, vec2f &en, t2PolygonGeometry *pgeom);

/*
	max separating axis for two polygons
*/
int MaxSeparatingEdge(t2PolygonGeometry *pgeomA, t2PolygonGeometry *pgeomB, float *ret_dist);

/*
	clip a line with respect to a plane
	note: it is assumed that v1 is outside and v2 is inside the given plane
*/
vec2f ClipLinePlane(vec2f &v1, vec2f &v2, vec2f &ev, vec2f &en);

/*
	test poly <-> poly for collision using SAT - Separating Axis Theorem
*/
void CDPolyPoly(int pgeomAi, int pgeomBi, t2Body *bodyA, t2Body *bodyB, t2World* world);

/*
	test circle <-> poly for collision using Voronoi Regions
*/
void CDCirclePoly(int pgeomAi, int pgeomBi, t2Body *bodyA, t2Body *bodyB, t2World* world);

/*
	test circle <-> circle for collision
*/
void CDCircleCircle(int pgeomAi, int pgeomBi, t2Body *bodyA, t2Body *bodyB, t2World* world);

/*
	test wether vertex is inside various types of geometry
*/
bool VertexInsideBody(vec2f &v, t2Body *body, t2Geometry *ret_geom = 0);

bool VertexInsidePoly(vec2f &v, t2PolygonGeometry *pgeom);

bool VertexInsideCircle(vec2f &v, t2CircleGeometry *cgeom);

bool VertexInsideEdge(vec2f &v, vec2f &edge_v, vec2f &edge_normal);

#endif /* T2DCOLLISION_H */