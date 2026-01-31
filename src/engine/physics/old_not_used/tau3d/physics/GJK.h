//
//  GJK.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-12-14.
//
//

#ifndef __tau3d__GJK__
#define __tau3d__GJK__

#include <stdio.h>
#include "vec.h"

#endif /* defined(__tau3d__GJK__) */

/*
 
 *** general CD ***
 
 – abstract collision detection for SAT / GJK
 class / templated function?
 abstract support function (returns supporting vertex) for all geometries
 
 *** GJK ***
 
 need: closest point tests for simplex – segment / face / tetrahedron
 need: function on geometries to obtain supporting vertex
 
 simplex = point / line / triangle / tetrahedron
 voronoi feature detection for simplex: Ericson p403-404
 
 finding extreme point wrt to vector: naïvely, or using hill-climping (applies to SAT as well, when sep is computed) (requires adjacency data)
 
 contact data:
    * manifold from simplex (simplices): Ericson (p405) ref's [Zhang95], [Nagle02],
        https://groups.google.com/forum/#!topic/comp.games.development.programming.algorithms/tjFsExG3JqA
    * Expanding Polytope Algorithm; iterative algo to find penetration depth using the GJK simplex
        Proximity Queries and Penetration Depth Computation on 3D Game Objects [theory folder]
    * clipping, the same way it's used in SAT? all that is given when clipping is applied is the SA. for GJK,
        SA can be obtained from the pair of closest points
 
 *** CHUNG-WANG ***
 
 Ericson p410
 finds separating axis iteratively for non-intersecting convex objects
 
 near constant time when the previous axis is used as initial guess
 
 */

#if 0

enum VORONOI { VERTEX, EDGE, FACE };

inline void closestpoint_vertex()
{
    // trivial
}

inline vec3 closestpoint_edge(/*seg_p0, seg_p1, p*/)
{
 
}

inline vec3 closestpoint_face()
{

}

VORONOI closestfeature_tetrahedron(/*simplex, point*/)
{
    
}

inline vec3 closestpoint_tetrahedron()
{
    // classify closets feature
    // compute closest point on feature
}

// void generate_contacts(std::vector<body_t*> &bodies, std::vector<arbiter_t> &arbiters);

static void generate_contacts_GJK()
{
    
}

#endif