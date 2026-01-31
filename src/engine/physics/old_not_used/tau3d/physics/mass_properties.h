//
//  mass_properties.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-29.
//
//

/**
 * notes & todo
 *
 * inertia tensor (I) is closely related to the covariance matrix (C) of a set of points
 * this is excellently explained here [inertiatensor_from_covariance.pdf][http://number-none.com/blow/inertia/]
 *  the method to compute aggregate mass properties is to accumulate C, and then compute I from it
 *  it computes (for an arbitrary ref point) C for a canonical polyhedron, which is then transformed to the actual polyhedron
 * apparently, this method can handle concave bodies, as long as they are closed
 *  this way, tensors could potentially be computed from the original geometry, and not the simplified collider
 * the method and theory is nice and clean, and could be both fun and useful to implement
 *
 * the method, used here, to compute I for an arbitrary polyhedron, is based on a canonical form
 * as well as far as I know.
 *
 */

#ifndef __tau3d__mass_properties__
#define __tau3d__mass_properties__

#include <stdio.h>
#include <vector>
#include "vec.h"
#include "mat.h"

using linalg::vec3f;
using linalg::mat3f;

struct mass_properties_t
{
    float m;    // mass
    mat3f I;    // moment of inertia
    vec3f com;  // center of mass
};

class collider_t;
class poly_geom_t;
class sphere_collider_t;
class point_collider_t;

/*
 * Convex polyhedron mass properties
 *
 */
mass_properties_t polyhedron_mass_properties(poly_geom_t *pg, float rho);

/*
 * Sphere mass properties
 */
mass_properties_t sphere_mass_properties(sphere_collider_t *sg, float rho);

/*
 * Point mass properties
 */
mass_properties_t point_mass_properties(point_collider_t *sg, float rho);

/*
 * Collider mass properties
 */
mass_properties_t collider_mass_properties(collider_t *g, float rho);

/*
 * Rigid Body mass properties
 */
mass_properties_t body_mass_properties(std::vector<collider_t*> &geometries, float rho);

#endif /* defined(__tau3d__geometry_attribs__) */
