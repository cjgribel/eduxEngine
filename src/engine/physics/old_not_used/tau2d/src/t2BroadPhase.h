
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

// inclusion guard
//
#pragma once
#ifndef T2BROADPHASE_H
#define T2BROADPHASE_H

#include "t2World.h"

enum t2ElementType {	ELEMTYPE_MIN,
						ELEMTYPE_MAX };

/*
	single element of an interval list:
	the max- and min-point of the extent of a geometry in a certian dimension
*/
struct t2ListElement
{
	t2Body* body;
    int geom_i;
	//t2Geometry* geom;
	float *value;
	t2ElementType type;

	t2ListElement(void)
	{ }

    t2ListElement(t2Body *body, int geom_i, float *value, t2ElementType type)
    : body(body), geom_i(geom_i), value(value), type(type)
	{ }
//	t2ListElement(t2Body *body, t2Geometry *geom, float *value, t2ElementType type)
//		: body(body), geom(geom), value(value), type(type)
//	{ }

	bool operator==(const t2ListElement &rhs) const
	{
		return body->geometries[geom_i] == rhs.body->geometries[rhs.geom_i];
	}
};

/*
	collision candidate pair
*/
struct t2ElementPair
{
	t2ListElement elemA, elemB;

	t2ElementPair(t2ListElement elemA, t2ListElement elemB)
		: elemA(elemA), elemB(elemB)
	{ }

	bool operator==(const t2ElementPair &rhs) const
	{
        bool ret =
            (elemA.body->geometries[elemA.geom_i] == rhs.elemA.body->geometries[rhs.elemA.geom_i] &&
             elemB.body->geometries[elemB.geom_i] == rhs.elemB.body->geometries[rhs.elemB.geom_i]) ||
            (elemA.body->geometries[elemA.geom_i] == rhs.elemB.body->geometries[rhs.elemB.geom_i] &&
             elemB.body->geometries[elemB.geom_i] == rhs.elemA.body->geometries[rhs.elemA.geom_i]);
        return ret;
	//	return
	//		(elemA.geom_i == rhs.elemA.geom_i && elemB.geom_i == rhs.elemB.geom_i) ||
	//		(elemA.geom_i == rhs.elemB.geom_i && elemB.geom_i == rhs.elemA.geom_i);
	}
};

/*
	Sweep & Prune - maintain sorted lists of body AABB intervals
*/
class t2SweepPrune
{
public:

	// ctor: populate interval lists with geometry boundaries
	//
	t2SweepPrune(t2World *world);

	// add body to interval lists
	//
	bool addBody(t2Body *body);

	// sort interval lists and trigger narrow phase geometry <-> geometry
	// collision detection for all candidates
	//
	void sweep();

//private:

	t2ListElement list_x[T2_MAX_SAP_ELEMENTS];
	t2ListElement list_y[T2_MAX_SAP_ELEMENTS];
	int nbrElements, maxElements;
	std::list<t2ElementPair> candidates;
	t2World *world;

	// perform insertion-sort on lists, adjust candidate list accordingly
	// as pairs of geometry begin or seize to overlap
	//
	void sort();
};

//struct t2ElementPair
//{
//	t2ListElement *elemA, *elemB;
//
//	t2ElementPair(t2ListElement *elemA, t2ListElement *elemB)
//		: elemA(elemA), elemB(elemB)
//	{ }
//
//	bool operator==(const t2ElementPair &rhs) const
//	{
//		return (elemA->geom == rhs.elemA->geom && elemB->geom == rhs.elemB->geom) ||
//			(elemA->geom == rhs.elemB->geom && elemB->geom == rhs.elemA->geom);
//	}
//};

//class t2SweepPrune
//{
//public:
//
//	//
//	// ctor: populate interval lists with geometry boundaries
//	//
//	t2SweepPrune(t2World *world);
//
//	//
//	// sort interval lists and trigger geometry <-> geometry
//	// collision detection for all candidates
//	//
//	void sweep();
//
//	~t2SweepPrune(void)
//	{ }
//
//private:
//
//	t2ListElement *list_x[200];
//	t2ListElement *list_y[200];
//	int nbrElements;
//	std::list<t2ElementPair> candidates;
//	t2World *world;
//
//	//
//	// perform insertion-sort on lists, adjust candidate list accordingly
//	// as pairs of geometry begin or seize to overlap
//	//
//	void sort();
//};

#endif /* T2BROADPHASE_H */