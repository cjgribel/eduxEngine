//
//  MassProperties.hpp
//
//  Created by Carl Johan Gribel on 2014-11-29.
//  Updated July 2022
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

#ifndef MassProperties_hpp
#define MassProperties_hpp

#include <stdio.h>
#include <vector>
#include "vec.h"
#include "mat.h"
#include "Colliders.hpp"

using linalg::vec3f;
using linalg::mat3f;

struct MassProperties3d
{
    // Mass
    float m = 1.0f;
    
    // 3x3 moment of inertia tensor matrix
    // This matrix is always symmetric.
    // It is also diagonal, if the body frame is aligned
    // with with eigenvectors of the matrix.
    // Note that as the body is rotated (in world space),
    //  I has to be rebased: I_w =  R * I * R^T
    m3f I = m3f_identity;

    // Centre of mass
    v3f com = v3f_000;
};

struct MassProperties2d
{
    float m = 1.0f;     // Mass
    float I = 1.0f;     // Moment of inertia
    v2f com = v2f_00;   // Centre of mass
};

MassProperties3d
AggregateMassProperties3d(const std::vector<entt::entity>& collider_entities,
                          entt::registry& registry,
                          const m4f& tfm);

MassProperties2d
AggregateMassProperties2d(const std::vector<entt::entity>& collider_entities,
                          entt::registry& registry,
                          const m4f& tfm);

#endif
