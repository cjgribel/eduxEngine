
/*
 * Tau2D Rigid Body Dynamics 
 * CJ Gribel (c) 2008-2009, cjgribel@gmail.com
 *
 */

// inclusion guard
#pragma once
#ifndef T2DGEOMETRY_H
#define T2DGEOMETRY_H

#include "tau2d.h"

enum GeometryType {GEOMTYPE_POLY, GEOMTYPE_CIRCLE};

/*

*/
class t2Geometry
{
public:
	vec2f offset;			// the geometry's offset relative the body it is part of
	GeometryType type;		//
	AABB aabb;				// 

	t2Geometry(vec2f offset, GeometryType type)
		: offset(offset), type(type)
	{ }

	virtual void transform(mat2 rotate, vec2f translate) = 0;

	virtual void transformLocal(mat2 tfm) = 0;

	virtual ~t2Geometry(void)
	{ }
};

/*

*/
class t2PolygonGeometry : public t2Geometry
{
public:
	vec2f
		vertices[15],			// vertices (global frame)
		normals[15],			// normals (global frame)
		vertices_local[15];		// vertices (local frame)
	int	nbrVertices;			// 
	const int maxVertices;		// 

	// initiate empty
	t2PolygonGeometry()
		: t2Geometry(vec2f(0.0f, 0.0f), GEOMTYPE_POLY), nbrVertices(0), maxVertices(15)
	{ }

	// initiate empty, with offset
	t2PolygonGeometry(vec2f offset)
		: t2Geometry(offset, GEOMTYPE_POLY), nbrVertices(0), maxVertices(15)
	{ }

	// initiate as regular polygon
	t2PolygonGeometry(vec2f offset, int sides, float radius)
		: t2Geometry(offset, GEOMTYPE_POLY), nbrVertices(0), maxVertices(15)
	{
		vec2f vref = vec2f(radius, 0);
		float dphi = 2.0f*PI / sides;
		for(int i = 0; i < sides; i++)
			addVertex(mat2((float)i * dphi) * vref);
		nbrVertices = sides;
	}

	// initiate as rectangle
	t2PolygonGeometry(vec2f offset, float width, float height)
		: t2Geometry(offset, GEOMTYPE_POLY), nbrVertices(0), maxVertices(15)
	{
		float
			dx = width / 2.0f,
			dy = height / 2.0f;
		addVertex(vec2f(dx, -dy));
		addVertex(vec2f(dx, dy));
		addVertex(vec2f(-dx, dy));
		addVertex(vec2f(-dx, -dy));
	}

	// initiate from list of vertices
	t2PolygonGeometry(vec2f vertices[], int nbrVertices, vec2f offset)
		: t2Geometry(offset, GEOMTYPE_POLY), nbrVertices(0), maxVertices(15)
	{
		addVertices(vertices, nbrVertices);
	}

	void addVertices(vec2f vertices[], int nbrVertices)
	{
		for(int i = 0; i < nbrVertices; addVertex(vertices[i++]));
	}

	void addVertex(vec2f vertex)
	{
		if(nbrVertices < maxVertices)
			vertices_local[nbrVertices++] = vertex;
	}

	void transformLocal(mat2 tfm)
	{
		for(int i = 0; i < nbrVertices; i++)
			vertices_local[i] = tfm * vertices_local[i];
	}

	void transform(mat2 rotate, vec2f translate)
	{
		aabb.reset(translate + rotate * offset);

		// transform vertices
		for(int i = 0; i < nbrVertices; i++)
		{
			vertices[i] = translate + rotate * (vertices_local[i] + offset);
			aabb.append(vertices[i]);
		}

		// recalculate normals
		for(int i = 0; i < nbrVertices; i++)
			normals[i] = vec2f::getNormal(vertices[i], vertices[(i+1) % nbrVertices]).normalize();
	}

};

/*

*/
class t2CircleGeometry : public t2Geometry
{
public:
	vec2f
		X,						// centerpoint (global frame)
		vR1_local, vR2_local,	// radial points (to visualize rotation) (local frame)
		vR1, vR2;				// radial points (global frame)
	float radius;				// radius

	t2CircleGeometry(vec2f offset, float radius)
		: t2Geometry(offset, GEOMTYPE_CIRCLE),
		radius(radius), vR1_local(radius, 0.0f), vR2_local(0.0f, radius)
	{ }

	void transformLocal(mat2 tfm)
	{
		
	}

	void transform(mat2 rotate, vec2f translate)
	{
		X = translate + rotate * offset;
		vR1 = rotate * vR1_local;
		vR2 = rotate * vR2_local;

		aabb.reset(X);
		aabb.append(X.x + radius, X.y + radius);
		aabb.append(X.x - radius, X.y - radius);
	}

};

#endif /* T2DGEOMETRY_H */
