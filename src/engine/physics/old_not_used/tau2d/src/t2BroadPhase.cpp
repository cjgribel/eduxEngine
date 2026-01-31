
/*
	Tau2D Rigid Body Dynamics 
	CJ Gribel (c) 2008-2009, cjgribel@gmail.com
*/

#include "t2BroadPhase.h"

/*	*/
t2SweepPrune::t2SweepPrune(t2World *world)
	: world(world), nbrElements(0), maxElements(1000)
{ }

/*	*/
bool t2SweepPrune::addBody(t2Body *body)
{
	if(nbrElements + 2 * body->nbrGeometries < maxElements)
	{
		t2Geometry *geom;
		for(int i = 0; i < body->nbrGeometries; i++)
		{
			geom = body->geometries[i];
			list_x[nbrElements] = t2ListElement(body, i, &geom->aabb.x_min, ELEMTYPE_MIN);
			list_x[nbrElements+1] = t2ListElement(body, i, &geom->aabb.x_max, ELEMTYPE_MAX);
			list_y[nbrElements] = t2ListElement(body, i, &geom->aabb.y_min, ELEMTYPE_MIN);
			list_y[nbrElements+1] = t2ListElement(body, i, &geom->aabb.y_max, ELEMTYPE_MAX);
			nbrElements += 2;
		}
		return true;
	}
	return false;
}

/*	*/
void t2SweepPrune::sweep()
{
	sort();

	t2Body *bodyA, *bodyB;
	t2Geometry *geomA, *geomB;
	for(std::list<t2ElementPair>::iterator it = candidates.begin(); it != candidates.end(); it++)
	{
		bodyA = it->elemA.body;
		bodyB = it->elemB.body;

		//if(!geomA->aabb.overlap(geomB->aabb))
		//	printf("err");

		if(	bodyA->collisionGroupId & bodyB->collisionFilter ||
			bodyB->collisionGroupId & bodyA->collisionFilter )
		{
            geomA = it->elemA.body->geometries[it->elemA.geom_i];
			geomB = it->elemB.body->geometries[it->elemB.geom_i];
			//geomA = it->elemA.geom;
			//geomB = it->elemB.geom;

			if(geomA->type == GEOMTYPE_POLY && geomB->type == GEOMTYPE_POLY)
			{
				CDPolyPoly(it->elemA.geom_i, it->elemB.geom_i, bodyA, bodyB, world);
			}
			else if(geomA->type == GEOMTYPE_CIRCLE && geomB->type == GEOMTYPE_POLY)
			{
				CDCirclePoly(it->elemA.geom_i, it->elemB.geom_i, bodyA, bodyB, world);
			}
			else if(geomA->type == GEOMTYPE_POLY && geomB->type == GEOMTYPE_CIRCLE)
			{
				CDCirclePoly(it->elemB.geom_i, it->elemA.geom_i, bodyB, bodyA, world);
			}
			else // if(geomA->type == GeometryType::GEOMTYPE_CIRCLE && geomB->type == GeometryType::GEOMTYPE_CIRCLE)
			{
				CDCircleCircle(it->elemA.geom_i, it->elemB.geom_i, bodyA, bodyB, world);
			}
		}
	}
}

/*	*/
void t2SweepPrune::sort()
{
	t2ListElement elemA, elemB;
	int j;

	for(int i = 1; i < nbrElements; i++)
	{
		// sort the x-dimension list

		j = i - 1;
		while(j >= 0 && *list_x[j].value > *list_x[j+1].value)
		{
			// elements j and j+1 are in wrong order - swap them

			elemA = list_x[j];
			elemB = list_x[j+1];

			// swap adjacency between element j and j+1 -> add or remove them as a collision candidate
			if(elemA.body != elemB.body && elemA.type != elemB.type)
			{
				if(elemA.type == ELEMTYPE_MIN && elemB.type == ELEMTYPE_MAX)
				{
					candidates.remove(t2ElementPair(elemA, elemB));
				}
				else if(elemA.body->geometries[elemA.geom_i]->aabb.y_min < elemB.body->geometries[elemB.geom_i]->aabb.y_max &&
                        elemA.body->geometries[elemA.geom_i]->aabb.y_max >= elemB.body->geometries[elemB.geom_i]->aabb.y_min)
				{
					candidates.push_back(t2ElementPair(elemA, elemB));
				}
			}

			// swap elements
			list_x[j+1] = elemA;
			list_x[j] = elemB;
			j--;
			//swaps++;
		}

		// sort the y-dimension list

		j = i - 1;
		while(j >= 0 && *list_y[j].value > *list_y[j+1].value)
		{
			// elements j and j+1 are in wrong order - swap them

			elemA = list_y[j];
			elemB = list_y[j+1];

			// swap adjacency between element j and j+1 -> add or remove them as a collision candidate
			if(elemA.body != elemB.body && elemA.type != elemB.type)
			{
				if(elemA.type == ELEMTYPE_MIN && elemB.type == ELEMTYPE_MAX)
				{
					candidates.remove(t2ElementPair(elemA, elemB));
				}
				else if(elemA.body->geometries[elemA.geom_i]->aabb.x_min < elemB.body->geometries[elemB.geom_i]->aabb.x_max &&
                        elemA.body->geometries[elemA.geom_i]->aabb.x_max >= elemB.body->geometries[elemB.geom_i]->aabb.x_min)
				{
					candidates.push_back(t2ElementPair(elemA, elemB));
				}
			}

			// swap elements
			list_y[j+1] = elemA;
			list_y[j] = elemB;
			j--;
			//swaps++;
		}
	}
	//printf("cand: %i, swaps: %i\n", candidates.size(), swaps);
}

//t2SweepPrune::t2SweepPrune(t2World *world)
//	: world(world), nbrElements(0)
//{
//	t2Body *body;
//	t2Geometry *geom;
//
//	for(int i = 0; i < world->nbrBodies; i++)
//	{
//		body = world->bodies[i];
//		for(int j = 0; j < body->nbrGeometries; j++)
//		{
//			geom = body->geometries[j];
//			list_x[nbrElements] = new t2ListElement(body, geom, &geom->aabb.x_min, t2ElementType::MIN);
//			list_x[nbrElements+1] = new t2ListElement(body, geom, &geom->aabb.x_max, t2ElementType::MAX);
//			list_y[nbrElements] = new t2ListElement(body, geom, &geom->aabb.y_min, t2ElementType::MIN);
//			list_y[nbrElements+1] = new t2ListElement(body, geom, &geom->aabb.y_max, t2ElementType::MAX);
//			nbrElements += 2;
//		}
//	}
//}
//
//void t2SweepPrune::sweep()
//{
//	sort();
//
//	t2Body *bodyA, *bodyB;
//	t2Geometry *geomA, *geomB;
//	t2PolygonGeometry *pgeomA, *pgeomB;
//	for(std::list<t2ElementPair>::iterator it = candidates.begin(); it != candidates.end(); it++)
//	{
//		bodyA = it->elemA->body;
//		bodyB = it->elemB->body;
//		geomA = it->elemA->geom;
//		geomB = it->elemB->geom;
//
//		//if(!geomA->aabb.overlap(geomB->aabb))
//		//	printf("err");
//
//		if(geomA->type == GeometryType::GEOMTYPE_POLY && geomB->type == GeometryType::GEOMTYPE_POLY)
//		{
//			pgeomA = static_cast<t2PolygonGeometry*> (geomA);
//			pgeomB = static_cast<t2PolygonGeometry*> (geomB);
//			CDPolyPoly(pgeomA, pgeomB, bodyA, bodyB, world);
//			CDPolyPoly(pgeomB, pgeomA, bodyB, bodyA, world);
//		}
//		else if(geomA->type == GeometryType::GEOMTYPE_CIRCLE && geomB->type == GeometryType::GEOMTYPE_POLY)
//		{
//			CDCirclePoly(static_cast<t2CircleGeometry*> (geomA), static_cast<t2PolygonGeometry*> (geomB), bodyA, bodyB, world);
//		}
//		else if(geomA->type == GeometryType::GEOMTYPE_POLY && geomB->type == GeometryType::GEOMTYPE_CIRCLE)
//		{
//			CDCirclePoly(static_cast<t2CircleGeometry*> (geomB), static_cast<t2PolygonGeometry*> (geomA), bodyB, bodyA, world);
//		}
//		else // if(geomA->type == GeometryType::GEOMTYPE_CIRCLE && geomB->type == GeometryType::GEOMTYPE_CIRCLE)
//		{
//			CDCircleCircle(static_cast<t2CircleGeometry*> (geomA), static_cast<t2CircleGeometry*> (geomB), bodyA, bodyB, world);
//		}
//	}
//}
//
//void t2SweepPrune::sort()
//{
//	t2ListElement *elemA, *elemB;
//	int j, swaps = 0;
//
//	for(int i = 1; i < nbrElements; i++)
//	{
//		j = i - 1;
//		while(j >= 0 && *list_x[j]->value > *list_x[j+1]->value)
//		{
//			// swap elements j and j+1
//
//			elemA = list_x[j];
//			elemB = list_x[j+1];
//
//			// swap adjacency between element j and j+1
//			if(elemA->body != elemB->body && elemA->type != elemB->type)
//			{
//				if(elemA->type == t2ElementType::MIN && elemB->type == t2ElementType::MAX)
//					candidates.remove(t2ElementPair(elemA, elemB));
//				else if(elemA->geom->aabb.y_min < elemB->geom->aabb.y_max && elemA->geom->aabb.y_max >= elemB->geom->aabb.y_min)
//					candidates.push_back(t2ElementPair(elemA, elemB));
//			}
//
//			// swap elements
//			list_x[j+1] = elemA;
//			list_x[j] = elemB;
//			j--;
//			swaps++;
//		}
//		j = i - 1;
//
//		while(j >= 0 && *list_y[j]->value > *list_y[j+1]->value)
//		{
//			// swap elements j and j+1
//
//			elemA = list_y[j];
//			elemB = list_y[j+1];
//
//			// swap adjacency between element j and j+1			
//			if(elemA->body != elemB->body && elemA->type != elemB->type)
//			{
//				if(elemA->type == t2ElementType::MIN && elemB->type == t2ElementType::MAX)
//					candidates.remove(t2ElementPair(elemA, elemB));
//				else if(elemA->geom->aabb.x_min < elemB->geom->aabb.x_max && elemA->geom->aabb.x_max >= elemB->geom->aabb.x_min)
//					candidates.push_back(t2ElementPair(elemA, elemB));
//			}
//
//			// swap elements
//			list_y[j+1] = elemA;
//			list_y[j] = elemB;
//			j--;
//			swaps++;
//		}
//	}
//	//printf("cand: %i, swaps: %i\n", candidates.size(), swaps);
//}