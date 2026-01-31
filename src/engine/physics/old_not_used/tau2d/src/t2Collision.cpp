
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

#include "t2Collision.h"
#include "t2World.h"
#include "t2Geometry.h"

/*
	perform a crude O(N^2) pass of collision detection
*/
void t2dCDWorld(t2World* world)
{
	t2Body *bodyA, *bodyB;
	int nbrGeoms = 0;

	for(int i = 0; i < world->nbrBodies; i++)
	{
		bodyA = world->bodies[i];
		for(int j = i+1; j < world->nbrBodies; j++)
		{
			bodyB = world->bodies[j];

			if(!bodyA->isStatic || !bodyB->isStatic)
				nbrGeoms += t2dCDPair(bodyA, bodyB, world);
		}
	}
	//printf("geom-tests: %i\n", nbrGeoms);
}

/*
	test two geometries for collision
*/
int t2dCDPair(t2Body *bodyA, t2Body *bodyB, t2World* world)
{
	if( bodyA->nbrGeometries == 0 ||
		bodyB->nbrGeometries == 0 ||
		!(bodyA->collisionGroupId & bodyB->collisionFilter) ||
		!(bodyB->collisionGroupId & bodyA->collisionFilter) )
		return 0;

	int nbrGeoms = 0;
	t2Geometry *geomA, *geomB;
	
	for(int i = 0; i < bodyA->nbrGeometries; i++)
	{
		geomA = bodyA->geometries[i];
		for(int j = 0; j < bodyB->nbrGeometries; j++)
		{
			geomB = bodyB->geometries[j];

			if(geomA->aabb.overlap(geomB->aabb))
			{
				nbrGeoms++;
				if(geomA->type == GEOMTYPE_POLY && geomB->type == GEOMTYPE_POLY)
				{
					CDPolyPoly(i, j, bodyA, bodyB, world);
				}
				else if(geomA->type == GEOMTYPE_CIRCLE && geomB->type == GEOMTYPE_POLY)
				{
					CDCirclePoly(i, j, bodyA, bodyB, world);
				}
				else if(geomA->type == GEOMTYPE_POLY && geomB->type == GEOMTYPE_CIRCLE)
				{
					CDCirclePoly(j, i, bodyB, bodyA, world);
				}
				else // if(geomA->type == GeometryType::GEOMTYPE_CIRCLE && geomB->type == GeometryType::GEOMTYPE_CIRCLE)
				{
					CDCircleCircle(i, j, bodyA, bodyB, world);
				}
			}
		}
	}
	return nbrGeoms;
}

/*
	signed distance between vertex and closest point on edge plane
*/
float DistVertexEdge(vec2f &v, vec2f &ev, vec2f &en)
{
	return vec2f::dot(v - ev, en);
}

/*
	min separating axis for a given edge and a polygon
*/
float MinSeparationEdgePoly(vec2f &ev, vec2f &en, t2PolygonGeometry *pgeom)
{
	float dist, min_dist = INF;
	for(int i = 0; i < pgeom->nbrVertices; i++)
	{
		dist = DistVertexEdge(pgeom->vertices[i], ev, en); // vec2f::dot(pgeom->vertices[i] - ev, en);
		if(dist < min_dist)
			min_dist = dist;
	}
	return min_dist;
}

/*
	max separating axis for two polygons
*/
int MaxSeparatingEdge(t2PolygonGeometry *pgeomA, t2PolygonGeometry *pgeomB, float *ret_dist)
{
	int ei = -1;
	float dist, max_dist = -1e10;
	for(int i = 0; i < pgeomA->nbrVertices; i++)
	{
		dist = MinSeparationEdgePoly(pgeomA->vertices[i], pgeomA->normals[i], pgeomB);
		if(dist > 0.0f)
			return -1;
		if(dist > max_dist)
		{
			max_dist = dist;
			ei = i;
		}
	}
	*ret_dist = max_dist;
	return ei;
}

/*
	clip a line with respect to a plane
	note: it is assumed that v1 is outside and v2 is inside the given plane
*/
vec2f ClipLinePlane(vec2f &v1, vec2f &v2, vec2f &ev, vec2f &en)
{
	float
		frac_out = fabs(vec2f::dot(v1 - ev, en)),
		frac_in = fabs(vec2f::dot(v2 - ev, en));
	
	return v1 + (v2 - v1) * (frac_out / (frac_out + frac_in));
}

/*
	test poly <-> poly for collision using SAT - Separating Axis Theorem
*/
void CDPolyPoly(int pgeomAi, int pgeomBi, t2Body *bodyA, t2Body *bodyB, t2World* world)
{
    t2PolygonGeometry   *pgeomA = static_cast<t2PolygonGeometry*> ( bodyA->geometries[pgeomAi] ),
                        *pgeomB = static_cast<t2PolygonGeometry*> ( bodyB->geometries[pgeomBi] );
    
	int sepiA, sepiB;				// index of separating axis for body A & B
	float sepA, sepB;				// seperation distance for body A & B

	sepiA = MaxSeparatingEdge(pgeomA, pgeomB, &sepA);
	if(sepiA == -1)
		return;

	sepiB = MaxSeparatingEdge(pgeomB, pgeomA, &sepB);
	if(sepiB == -1)
		return;

	vec2f
		ev, en, cp,					// edge vertex, edge normal, clip point
		vThis, vNext;				// current / next vertex along an edge
	bool
		this_leftIn, next_leftIn,	// state of current / next edge vertex relative left incident plane
		this_rightIn, next_rightIn;	// state of current / next edge vertex relative right incident plane
	float depth;					// penetration depth

	if(sepA < sepB)
	{
		ev = pgeomB->vertices[sepiB];
		en = pgeomB->normals[sepiB];

		for(int i = 0; i < pgeomA->nbrVertices; i++)
		{
			vThis = pgeomA->vertices[i];
			vNext = pgeomA->vertices[(i+1) % pgeomA->nbrVertices];

			if(VertexInsidePoly(vThis, pgeomB))
			{
				depth = DistVertexEdge(vThis, ev, en);
				world->contacts->push_back(t2ContactJoint(bodyA, bodyB, vThis, en, -depth, pgeomAi, pgeomBi, i, sepiB));
				this_leftIn = true;
				this_rightIn = true;
			}
			else
			{
				if(i == 0)
				{
					this_leftIn = VertexInsideEdge(vThis, pgeomB->vertices[(sepiB+1) % pgeomB->nbrVertices], pgeomB->normals[(sepiB+1) % pgeomB->nbrVertices]);
					this_rightIn = VertexInsideEdge(vThis, pgeomB->vertices[(sepiB-1) % pgeomB->nbrVertices], pgeomB->normals[(sepiB-1) % pgeomB->nbrVertices]);
				}
				else
				{
					this_leftIn = next_leftIn;
					this_rightIn = next_rightIn;
				}
			}
			next_leftIn = VertexInsideEdge(vNext, pgeomB->vertices[(sepiB+1) % pgeomB->nbrVertices], pgeomB->normals[(sepiB+1) % pgeomB->nbrVertices]);
			next_rightIn = VertexInsideEdge(vNext, pgeomB->vertices[(sepiB-1) % pgeomB->nbrVertices], pgeomB->normals[(sepiB-1) % pgeomB->nbrVertices]);

			if(!this_leftIn && next_leftIn)
			{
				cp = ClipLinePlane(vThis, vNext, pgeomB->vertices[(sepiB+1) % pgeomB->nbrVertices], pgeomB->normals[(sepiB+1) % pgeomB->nbrVertices]);
				depth = DistVertexEdge(cp, ev, en);
				if(depth <= 0.0f)
					world->contacts->push_back(t2ContactJoint(bodyA, bodyB, cp, en, -depth, pgeomAi, pgeomBi, i, sepiB));
			}
			if(this_rightIn && !next_rightIn)
			{
				cp = ClipLinePlane(vNext, vThis, pgeomB->vertices[(sepiB-1) % pgeomB->nbrVertices], pgeomB->normals[(sepiB-1) % pgeomB->nbrVertices]);
				depth = DistVertexEdge(cp, ev, en);
				if(depth <= 0.0f)
					world->contacts->push_back(t2ContactJoint(bodyA, bodyB, cp, en, -depth, pgeomAi, pgeomBi, i, sepiB));
			}
		} /* for */
	}
	else
	{
		ev = pgeomA->vertices[sepiA];
		en = pgeomA->normals[sepiA];

		for(int i = 0; i < pgeomB->nbrVertices; i++)
		{
			vThis = pgeomB->vertices[i];
			vNext = pgeomB->vertices[(i+1) % pgeomB->nbrVertices];

			if(VertexInsidePoly(vThis, pgeomA))
			{
				depth = DistVertexEdge(vThis, ev, en);
				world->contacts->push_back(t2ContactJoint(bodyB, bodyA, vThis, en, -depth, pgeomBi, pgeomAi, i, sepiA));
				this_leftIn = true;
				this_rightIn = true;
			}
			else
			{
				if(i == 0)
				{
					this_leftIn = VertexInsideEdge(vThis, pgeomA->vertices[(sepiA+1) % pgeomA->nbrVertices], pgeomA->normals[(sepiA+1) % pgeomA->nbrVertices]);
					this_rightIn = VertexInsideEdge(vThis, pgeomA->vertices[(sepiA-1) % pgeomA->nbrVertices], pgeomA->normals[(sepiA-1) % pgeomA->nbrVertices]);
				}
				else
				{
					this_leftIn = next_leftIn;
					this_rightIn = next_rightIn;
				}
			}
			next_leftIn = VertexInsideEdge(vNext, pgeomA->vertices[(sepiA+1) % pgeomA->nbrVertices], pgeomA->normals[(sepiA+1) % pgeomA->nbrVertices]);
			next_rightIn = VertexInsideEdge(vNext, pgeomA->vertices[(sepiA-1) % pgeomA->nbrVertices], pgeomA->normals[(sepiA-1) % pgeomA->nbrVertices]);

			if(!this_leftIn && next_leftIn)
			{
				cp = ClipLinePlane(vThis, vNext, pgeomA->vertices[(sepiA+1) % pgeomA->nbrVertices], pgeomA->normals[(sepiA+1) % pgeomA->nbrVertices]);
				depth = DistVertexEdge(cp, ev, en);
				if(depth <= 0.0f)
					world->contacts->push_back(t2ContactJoint(bodyB, bodyA, cp, en, -depth, pgeomBi, pgeomAi, i, sepiA));
			}
			if(this_rightIn && !next_rightIn)
			{
				cp = ClipLinePlane(vNext, vThis, pgeomA->vertices[(sepiA-1) % pgeomA->nbrVertices], pgeomA->normals[(sepiA-1) % pgeomA->nbrVertices]);
				depth = DistVertexEdge(cp, ev, en);
				if(depth <= 0.0f)
					world->contacts->push_back(t2ContactJoint(bodyB, bodyA, cp, en, -depth, pgeomBi, pgeomAi, i, sepiA));
			}
		} /* for */
	} /* if */
}

/*
	test circle <-> poly for collision using Voronoi Regions
*/
void CDCirclePoly(int cgeomi, int pgeomi, t2Body *bodyA, t2Body *bodyB, t2World* world)
{
    t2CircleGeometry *cgeom = static_cast<t2CircleGeometry*>(bodyA->geometries[cgeomi]);
    t2PolygonGeometry *pgeom = static_cast<t2PolygonGeometry*>(bodyB->geometries[pgeomi]);
    
	vec2f cp, cn;
	float depth;

	// test edge features

	vec2f n, ev1, ev2, e, c1, c2, c1n;
	float c1dot, c1n_len;

	for(int i = 0; i < pgeom->nbrVertices; i++)
	{
		ev1 = pgeom->vertices[i];
		ev2 = pgeom->vertices[(i+1) % pgeom->nbrVertices];
		e = ev2 - ev1;
		c1 = cgeom->X - ev1;
		c2 = cgeom->X - ev2;
		n = pgeom->normals[i];
		c1dot = vec2f::dot(c1, n);

		// test if circle center is within edge normal corridor
		if(vec2f::dot(e, c1) > 0.0f && vec2f::dot(-e, c2) > 0.0f)
		{
			if(c1dot > 0.0f)
			{
				// circle center is inside edge outward corridor (can only be true for one edge)
				c1n = n * c1dot;
				c1n_len = c1n.norm();
				if(c1n_len <= cgeom->radius)
				{
					cp = cgeom->X - c1n;
					cn = n;
					depth = cgeom->radius - c1n_len;
					world->contacts->push_back(t2ContactJoint(bodyA, bodyB, cp, cn, depth, cgeomi, pgeomi, 0, i));
				}
				return;
			}
			else
			{
				// circle center is inside/behind body
				// todo: if this is true for ALL vertices the circle center is inside the poly
				//  trach this. if continously true, use cp, cn & depth for edge with smallest depth
			} 
		}
	}

	// test vertex features

	// todo: abort if dist decrease then incease
	
	vec2f v;
	float v_len;
	
	for(int i = 0; i < pgeom->nbrVertices; i++)
	{
		v = cgeom->X - pgeom->vertices[i];
		v_len = v.norm();
		// test if vertex is inside circle
		// note: applies to max one vertex (if more, the collision will be detected by the edge test)
		// note: the inner corridors are covered by the edge test
		if(v_len < cgeom->radius)
		{
			cp = pgeom->vertices[i];
			cn = vec2f(cgeom->X - cp).normalize();
			depth = cgeom->radius - v_len;
			world->contacts->push_back(t2ContactJoint(bodyA, bodyB, cp, cn, depth, cgeomi, pgeomi, i, 0));
			return;
		}
	}
}

/*
	test circle <-> circle for collision
*/
void CDCircleCircle(int cgeomAi, int cgeomBi, t2Body *bodyA, t2Body *bodyB, t2World* world)
{ 
    t2CircleGeometry    *cgeomA = static_cast<t2CircleGeometry*>(bodyA->geometries[cgeomAi]),
                        *cgeomB = static_cast<t2CircleGeometry*>(bodyB->geometries[cgeomBi]);
    
	vec2f	v = cgeomA->X - cgeomB->X,						// circle centers vector
			cp,												// collision point
			cn;												// collision normal
	float	v_len = v.norm(),								// 
			sum_radius = cgeomA->radius + cgeomB->radius,	// 
			depth;											// penetration depth

	if(v_len < sum_radius && v_len > 0.0f)
	{
		cn = v / v_len;
		depth = sum_radius - v_len;
		cp = cgeomB->X + v * (cgeomB->radius / sum_radius);
		world->contacts->push_back(t2ContactJoint(bodyA, bodyB, cp, cn, depth, cgeomAi, cgeomBi, 0, 0));
	}
}

/*
	test wether vertex is inside various types of geometry
*/
bool VertexInsideBody(vec2f &v, t2Body *body, t2Geometry *ret_geom)
{
		if(body->nbrGeometries == 0) return false;

		t2Geometry *geom;
		for(int i = 0; i < body->nbrGeometries; i++)
		{
			geom = body->geometries[i];
			if(geom->type == GEOMTYPE_POLY)
			{
				t2PolygonGeometry* p_geom = static_cast<t2PolygonGeometry*> (geom);
				if( VertexInsidePoly(v, p_geom) )
				{
					ret_geom = p_geom;
					return true;
				}
			}
			else if(geom->type == GEOMTYPE_CIRCLE)
			{
				t2CircleGeometry* c_geom = static_cast<t2CircleGeometry*> (geom);
				if( VertexInsideCircle(v, c_geom) )
				{
					ret_geom = c_geom;
					return true;
				}
			}
		}
		return false;
}

bool VertexInsidePoly(vec2f &v, t2PolygonGeometry *pgeom)
{
	for(int i = 0; i < pgeom->nbrVertices; i++)
	{
		if(!VertexInsideEdge(v, pgeom->vertices[i], pgeom->normals[i]))
			return false;
	}
	return true;
}

bool VertexInsideCircle(vec2f &v, t2CircleGeometry *cgeom)
{
	return vec2f(v - cgeom->X).norm() <= cgeom->radius;
}

bool VertexInsideEdge(vec2f &v, vec2f &edge_v, vec2f &edge_normal)
{
		return vec2f::dot(v - edge_v, edge_normal) <= 0.0f;
}