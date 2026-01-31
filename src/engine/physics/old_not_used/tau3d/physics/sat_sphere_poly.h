//
//  sat_sphere_poly.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2015-01-22.
//
//

#ifndef __tau3d__sat_sphere_poly__
#define __tau3d__sat_sphere_poly__

#include <stdio.h>
#include "vec.h"
#include "body.h"
#include "collider.h"
#include "contact_constraint.h"

/*
 closest-point on triangle to point
 
 credits: Ericson, Real-Time Collision Detection, page 141
 */
static vec3f closestpoint_point_triangle(const vec3f &p, const vec3f &a, const vec3f &b, const vec3f &c)
{
    // Check if P in vertex region outside A
    vec3f ab = b - a;
    vec3f ac = c - a;
    vec3f ap = p - a;
    float d1 = ab.dot(ap);
    float d2 = ac.dot(ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a; // barycentric coordinates (1,0,0)
    
    // Check if P in vertex region outside B
    vec3f bp = p - b;
    float d3 = ab.dot(bp);
    float d4 = ac.dot(bp);
    if (d3 >= 0.0f && d4 <= d3) return b; // barycentric coordinates (0,1,0)
    
    // Check if P in edge region of AB, if so return projection of P onto AB
    float vc = d1*d4 - d3*d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + ab*v; // barycentric coordinates (1-v,v,0)
    }
    
    // Check if P in vertex region outside C
    vec3f cp = p - c;
    float d5 = ab.dot(cp);
    float d6 = ac.dot(cp);
    if (d6 >= 0.0f && d5 <= d6) return c; // barycentric coordinates (0,0,1)
    
    // Check if P in edge region of AC, if so return projection of P onto AC
    float vb = d5*d2 - d1*d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + ac*w; // barycentric coordinates (1-w,0,w)
    }
    // Check if P in edge region of BC, if so return projection of P onto BC
    float va = d3*d6 - d5*d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b)*w; // barycentric coordinates (0,1-w,w)
    }
    // P inside face region. Compute Q through its barycentric coordinates (u,v,w)
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w; // = u*a + v*b + w*c, u = va * denom = 1.0f - v - w
}

static bool collide_poly_sphere(poly_collider_t *poly, sphere_collider_t *sphere, body_t *bodyA, body_t *bodyB, contact_manifold_t &cmanifold, bool nflip = false)
{
//    render_line(bodyA->X, bodyB->X, 1, {0,0,0,1});
    
    vec3f cp; // closest point
    float cp_norm = std::numeric_limits<float>::infinity(); // norm closest point -> sphere centre
    
    unsigned face_stride = poly->face_stride;
    for (int i=0; i<poly->nbr_faces; i++)
        for (int j=1; j<face_stride-1; j++)
        {
            vec3f v0 = poly->vertices_w[ poly->faces[i*face_stride + 0]    ];
            vec3f v1 = poly->vertices_w[ poly->faces[i*face_stride + j]    ];
            vec3f v2 = poly->vertices_w[ poly->faces[i*face_stride + j+1]  ];
            
            vec3f cp_tmp = closestpoint_point_triangle(sphere->p_w, v0, v1, v2);
            float cp_tmp_norm = (sphere->p_w-cp_tmp).norm2squared();
            // check if cp of triangle is closest cp
            if (cp_tmp_norm < cp_norm)
            {
                cp = cp_tmp;
                cp_norm = cp_tmp_norm;
            }
        }

    cp_norm = sqrt(cp_norm);
    
    // no collision
    if (cp_norm > sphere->r)
        return false;
    
    // collision
    vec3f v = sphere->p_w - cp; // collision vector A -> B
    vec3f vn = v; vn.normalize();
    contact_point_t c;
    c.dist = cp_norm - sphere->r;
    c.cp = sphere->p_w - vn*(sphere->r);
    c.cn = vn;
    c.cndir = nflip?-1:1;
    c.id.geomA = poly;
    c.id.geomB = sphere;
    
    cmanifold.contacts.push_back(c);
    return true;
}

//static bool collide_poly_sphere(sphere_collider_t *sphere, poly_geom_t *poly, body_t *bodyA, body_t *bodyB, arbiter_t &arbiter)
//{
//    if (collide_poly_sphere(poly, sphere, bodyB, bodyA, arbiter))
//    {
//        swap(arbiter.bodyA, arbiter.bodyB);
//        return true;
//    }
//        return false;
//}

#endif /* defined(__tau3d__sat_sphere_poly__) */
