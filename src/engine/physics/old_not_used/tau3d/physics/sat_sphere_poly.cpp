//
//  sat_sphere_poly.cpp
//  tau3d
//
//  Created by Carl Johan Gribel on 2015-01-22.
//
//

#include "sat_sphere_poly.h"



//bool collide_poly_sphere(poly_geom_t *poly, sphere_collider_t *sphere, body_t *bodyA, body_t *bodyB, arbiter_t &arbiter)
//{
//    render_line(bodyA->X, bodyB->X, 1, {0,0,0,1});
//    /*
//     classify which Vornoi feature (face/edge/vertex) the sphere is in
//     which order is most effective?
//     see Ericsson
//     
//     collision normal
//     face: face normal
//     edge: direction from sphere center to its closest point on edge
//     vertex: direction from vertex to sphere center
//     
//     contact manifold
//     just the point of min/max separation? i.e. point along collision normal
//     
//     */
////    for (int i=0; i<nbr_faces; i++)
////        for (int j=1; j<face_stride-1; j++)
////        {
////            vec3 v0 = vertices_w[ faces[i*face_stride + 0]    ];
////            vec3 v1 = vertices_w[ faces[i*face_stride + j]    ];
////            vec3 v2 = vertices_w[ faces[i*face_stride + j+1]  ];
////            
////            if (intersect_triangle(v0, v1, v2, ray)) hit = true;
////        }
//
//    return false;
//}
