//
//  t3collision.cpp
//  tau3d
//
//  Created by Carl Johan Gribel on 2012-05-01.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <iostream>
#include "tau3d.h"
#include "sat_collision.h"
#include "sat_sphere_poly.h"
#include "world.h"
#include "body.h"
#include "rendering.h"
#include "math.h"

/*
 contact ID generation notes
 
 -> minimal instrusion to current impl
    create ID's during clipping
    skip edges wholly inside opposite poly
    edges where both points are clipped: add both, correctly
    edges with one clipped points: ideally, add just the clipped one (the non-clipped one is added in an earlier pass for vertices)
        the non-clipped vertex may be added twice, but with different ID's (edge, potentially face too), so warm starting should be able to be tested
    PRINT CONTACTS TO DEBUG
 
 box2d-style clipping ('don't clip against reference edge')
    may apply when a face normal is sep axis. what about when an edge x edge axis is?
 
 PASS 1:
    check vertices against poly
    if inside: create contact with id {f,e,v} = {-1,-1,i,0} (bodyA->bodyB) and = = {-1,-1,i,1} (bodyA<-bodyB)
 PASS 2:
    clip polys against each other
    setup ID's for each edge = {-1,-1,-1}
    let clip_edge_face mark which edge points are clipped
    if point is clipped, set ID = {i,j,-1}
    edge has been clipped to all faces
        add clipped points to manifold
 
 clip poly A -> polyB
    for all edges of polyA
        setup contactID's ID0 and ID1 for edge points, set to face-/edge-ID = -1 and [vertex-ID] to vertex-ID
        setup clipped0 = clipped1 = false
        for all faces of polyB
            let clip_edge_face mark which edge points are clipped
            if !clip_edge_face
                skip this edge
            if clipped0_ret
                clipped0 = true;
                set edge and face indices to ID0, set vertex to -1
            else if clipped1
                clipped1 = true;
                set edge and face indices to ID1, set vertex to -1
            else
                ...
        edge has been clipped to all faces:
        if clipped0 = clipped1 = false
            edge is wholly inside poly -> don't add to manifold
        else
            add both contacts to manifold
 
 
 */

inline float dist_plane_vertex(const vec3f &plane_n, const vec3f &plane_p, const vec3f &p)
{
    return dot(p - plane_p, plane_n);
}

/*
 * min separation: plane -> vertices of poly
 *
 * todo: could use hill-climbing here
 */
static float min_separation_plane_poly(const vec3f &n, const vec3f &p, const poly_collider_t *poly)
{
    float min_sep = std::numeric_limits<float>::infinity();
    
    for (int i=0; i<poly->vertices_w.size(); i++)
    {
        float dist = dist_plane_vertex(n, p, poly->vertices_w[i]);
        if (dist < min_sep)
            min_sep = dist;
    }
    return min_sep;
}

/*
 * min & max separation: plane -> vertices of poly
 */
static void minmax_separation_plane_poly(vec3f &n, vec3f &p, poly_collider_t *poly, float &min, float &max)
{
    float min_sep = std::numeric_limits<float>::infinity();
    float max_sep = -std::numeric_limits<float>::infinity();
    
    for (int i=0; i<poly->vertices_w.size(); i++)
    {
        float dist = dist_plane_vertex(n, p, poly->vertices_w[i]);
        if (dist < min_sep)
            min_sep = dist;
        if (dist > max_sep)
            max_sep = dist;
    }
    min = min_sep;
    max = max_sep;
}

/*  
 clip edge against plane
 
 returns
    FALSE: edge is outside plane
    TRUE and clipped = FALSE: edge inside or on plane
    TRUE and clipped = TRUE:  edge straddles plane and is clipped (keep as contact point)
 
 principle illustration: Ericson p365, GDrive/misc13
 */
static bool clip_edge_to_plane(const vec3f &plane_n, const vec3f &plane_p, vec3f &edge_p0, vec3f &edge_p1, bool &clipped, bool& clipped0, bool& clipped1)
{
    float d0 = (edge_p0-plane_p).dot(plane_n);
    float d1 = (edge_p1-plane_p).dot(plane_n);
    clipped = false;
    clipped0 = clipped1 = false;
    
    const float CLIP_EPS = 0.00001f;

    // edge on plane
    if (fabs(d0) < CLIP_EPS && fabs(d1) < CLIP_EPS) return true;
    // edge outside plane
    if (d0 > -CLIP_EPS && d1 > -CLIP_EPS) return false;
    // edge inside plane
    if (d0 <= CLIP_EPS && d1 <= CLIP_EPS) return true;
    
    vec3f pi = edge_p0 + (edge_p1 - edge_p0)*(fabs(d0)/(fabs(d0)+fabs(d1)));
    clipped = true;
    
    // edge straddles plane
    if (d0 > CLIP_EPS) { edge_p0 = pi; clipped0 = true; }
    if (d0 < -CLIP_EPS) { edge_p1 = pi; clipped1 = true; }
    return true;
}
#if 0
static bool clip_edge_to_plane(const vec3 &plane_n, const vec3 &plane_p, vec3 &edge_p0, vec3 &edge_p1, bool &clipped)
{
    float d0 = (edge_p0-plane_p).dot(plane_n);
    float d1 = (edge_p1-plane_p).dot(plane_n);
    
#define EPS 0.00001f
    
    if (d0 > -EPS && d1 > -EPS) return false;
    
    if (d0 <= -EPS && d1 <= -EPS) { clipped = false; return true; }
    
    vec3 pi = edge_p0 + (edge_p1 - edge_p0)*(fabs(d0)/(fabs(d0)+fabs(d1)));
    clipped = true;
    
    if (d0 < -EPS)
        edge_p1 = pi;
    
    if (d1 < -EPS)
        edge_p0 = pi;
    
    return true;
}
#endif

/*
 * clip edges of polygon A against faces of polygon B
 *
 * after an edge has been clipped towards all faces, its vertices are kept as contact points if:
 * - vertex was produced when the eddge was clipped by a face (FACE-EDGE feature)
 * - vertex was inside polygon B, and has not been added by another edge (VERTEX feature)
 *
 * the features are marked after each edge-face clipping step {face_id, edge_id, vertex_id}:
 * FACE-EDGE: { face index, edge index, -1 }
 * VERTEX: (if not previously indentified as clipped): { -1, -1, vertex index }
 *
 */
static void clip_polygons(poly_collider_t* polyA, poly_collider_t* polyB, std::vector<contact_point_t>& contacts, int flip, int n_exclude = -1)
{
    for (int i=0; i<polyA->edges.size(); i +=2)
    {
        // edge points
        unsigned vi0 = polyA->edges[i+0];
        unsigned vi1 = polyA->edges[i+1];
        vec3f p0 = polyA->vertices_w[ vi0 ];
        vec3f p1 = polyA->vertices_w[ vi1 ];
        // contacts id's
        contact_point_t::contact_id_t id0, id1;
        
        bool keep = true, clipped = false, clipped0 = false, clipped1 = false;
        
        for (int j=0; j<polyB->nbr_faces; j++)
        {
            if (j == n_exclude) { continue; }
            vec3f plane_n = polyB->normals_w[j];
            vec3f plane_p = polyB->vertices_w[ polyB->faces[j*polyB->face_stride+0] ];
            
            bool clipped_, clipped0_, clipped1_;
            if (!clip_edge_to_plane(plane_n, plane_p, p0, p1, clipped_, clipped0_, clipped1_)) { keep = false; break; }
            else {
                if (clipped0_)
                    id0 = {j, i, -1, flip};           // mark as clipped contact
                else if (!clipped0)
                    id0 = {-1, -1, (int)vi0, flip};   // mark as vertex contact
                if (clipped1_)
                    id1 = {j, i, -1, flip};           // mark as clipped contact
                else if (!clipped1)
                    id1 = {-1, -1, (int)vi1, flip};   // mark as vertex contact
                clipped |= clipped_; clipped0 |= clipped0_; clipped1 |= clipped1_;
            }
        }
        
        if (!keep) continue; // edge culled
        // edge clipping completed
//        if (clipped) {
//            manifold.push_back(p0);
//            manifold.push_back(p1);
//        }
        // use edge point as contacts if clipped or inside
        contact_point_t c0; c0.cp = p0; c0.id = id0;
        contact_point_t c1; c1.cp = p1; c1.id = id1;
        if (clipped0)
            contacts.push_back(c0);
        else if ( std::find(contacts.begin(), contacts.end(), c0) == contacts.end() )
            contacts.push_back(c0);
        if (clipped1)
            contacts.push_back(c1);
        else if ( std::find(contacts.begin(), contacts.end(), c1) == contacts.end() )
            contacts.push_back(c1);
    }
}

// -> compute_Steiner_point
//
static vec3f poly_centre(poly_collider_t *p)
{
    vec3f vc = vec3f(0,0,0);
    for (vec3f &v : p->vertices_w) {
        vc += v;
    }
    vc *= (1.0f/p->vertices_w.size());
    
    return vc;
}

static bool collide_polys(poly_collider_t* polyA, poly_collider_t* polyB,
                          body_t* bodyA, body_t* bodyB, // NOT USED
                          contact_manifold_t& cmanifold)
{
//    printf("begin test '%s' - '%s'\n", bodyA->id.c_str(), bodyB->id.c_str());
    // 1. find sep axis (SA) with max separation
    
    vec3f SA_n;     // separating axis
    vec3f SA_p;     // separating point (from face or edge)
    int SA_ni = -1; // index of SA plane
    bool nflip = false;
    int feat_id; // dbg
    float max_sep = -std::numeric_limits<float>::infinity();
    
    // test faces of A -> vertices of B

    for (int i=0; i<polyA->nbr_faces; i++)
    {
        vec3f n = polyA->normals_w[i];
        vec3f p = polyA->vertices_w[ polyA->faces[i*polyA->face_stride+0] ];
        float sep = min_separation_plane_poly(n, p, polyB); //printf("i %d, sep %f\n", i, sep);
        if (sep > 0) return false;
        if (sep > max_sep)
        {
            max_sep = sep;
            SA_n = n;
            SA_p = p;
            nflip = false;
            // dbg
            SA_ni = i;
            feat_id = 0;
        }
    }
//    printf("max sep %f\n", max_sep);

    // test faces of B -> vertices of A
    
    for (int i=0; i<polyB->nbr_faces; i++)
    {
        vec3f n = polyB->normals_w[i];
        vec3f p = polyB->vertices_w[ polyB->faces[i*polyB->face_stride+0] ];
        float sep = min_separation_plane_poly(n, p, polyA);
        if (sep > 0) return false;
        if (sep > max_sep)
        {
            max_sep = sep;
            SA_n = n; // -n;
            SA_p = p;
            nflip = true;
            // dbg
            SA_ni = i;
            feat_id = 1;
        }
    }
    
    // test (unique) edges of A x edges of B

    // test
    vec3f polyAc = poly_centre(polyA), polyBc = poly_centre(polyB);
    //render_marker(polyA->p_w, 0.3, 1, vec4f(1,0,0,1));
    //render_marker(polyB->p_w, 0.3, 1, vec4f(1,0,0,1));
    
    for (int i=0; i<polyA->unique_edge_dirs.size(); i +=2)
    {
        vec3f pA = polyA->vertices_w[ polyA->unique_edge_dirs[i+0] ];
        vec3f eA = polyA->vertices_w[ polyA->unique_edge_dirs[i+1] ] - pA;
        
        for (int j=0; j<polyB->unique_edge_dirs.size(); j +=2)
        {
            vec3f pB = polyB->vertices_w[ polyB->unique_edge_dirs[j+0] ];
            vec3f eB = polyB->vertices_w[ polyB->unique_edge_dirs[j+1] ] - pB;
            
            if (1.0f-fabsf(dot(normalize(eA), normalize(eB))) < 0.0001f) continue;

            vec3f n = normalize(eA % eB);
            
            // check support overlap on n
            float Amin, Amax, Bmin, Bmax;
            minmax_separation_plane_poly(n, pA, polyA, Amin, Amax);
            minmax_separation_plane_poly(n, pA, polyB, Bmin, Bmax);
            
            float sep = fmaxf(Amin, Bmin) - fminf(Amax, Bmax);
            if (sep > 0) return false;
            if (sep > max_sep)
            {
                max_sep = sep;
                SA_n = n;
//                nflip = (polyB->p_w-polyA->p_w).dot(n) < 0.0f;
                nflip = (polyBc-polyAc).dot(n) < 0.0f;
                
                SA_p = nflip ? pB : pA;
                // dbg
                SA_ni = -1;
                feat_id = 2;
            }
        }
    }
#if 0
    printf("nflip %s, feat %s\n", nflip?"yes":"no", feat_id==0?"face A":(feat_id==1?"face B":"edge"));
    printf("SA_n "); SA_n.debugPrint();
    printf("SA_p "); SA_p.debugPrint();
#endif
    
    // 2. determine contact manifold by clipping polys against each other
    
//    std::vector<vec3f> manifold;
    std::vector<contact_point_t> contacts;

    // HALFCLIP: collision detected, but zero contacts from clipping
    
#define FULLCLIP
#ifdef FULLCLIP
    clip_polygons(polyA, polyB, contacts, 0);
    clip_polygons(polyB, polyA, contacts, 1);
#else
    if (nflip)  clip_polygons(polyA, polyB, contacts, 0, SA_ni);
    else        clip_polygons(polyB, polyA, contacts, 1, SA_ni);
#endif
    
//    if (manifold.size() == 0) return false;
    if (contacts.size() == 0) { /*printf("0 contacts, abort\n");*/ return false; }

//    if (nflip) printf("XXX\n"); // never (?) happens for halfclip
    
#if 0
    //render SA_n, SA_p
    render_marker(SA_p, 0.05, 2, {0,0,1,1});
    render_line(SA_p, SA_p+SA_n*(nflip?1:1), 0.05, {0,0,1,1});
#endif
#if 0
    // contact id
//    printf("contacts %d, manifold %d\n", contacts.size(), manifold.size());
    printf("contact id's (%d):\n", (int)contacts.size());
    for (contact_point_t &c : contacts) {
        printf("face %d, edge %d, vertex %d, flip %d\n", c.id.face_id, c.id.edge_id, c.id.vertex_id, c.id.flip);
    }
#endif
#if 0
    // check contact uniqueness
    bool unique = true;
    for (int i=0;i<contacts.size();i++)
        for (int j=i+1;j<contacts.size();j++)
            unique &= !(contacts[i] == contacts[j]);
    if (!unique) printf("%s\n", unique?"unique":"non-unique");
#endif
#if 0
    // DBG render manifold
//    for (int i=0; i<manifold.size(); i+=2)
//        render_line(manifold[i], manifold[i+1],  4, vec4f(0,1,0,1));
    for (vec3f &v : manifold)
        render_marker(v, 0.025, 2, {1,0,0,1});
#endif
    
    // 3. compute separations of manifold

    float manifold_max_sep = -std::numeric_limits<float>::infinity();
    float manifold_min_sep = std::numeric_limits<float>::infinity();
    std::vector<float> separations;
    for (auto &c : contacts) {
        c.cn = SA_n;
        c.cndir = nflip?-1:1;
        c.id.geomA = polyA;
        c.id.geomB = polyB;
        
        float sep = dist_plane_vertex(SA_n, SA_p, c.cp); //printf("%1.10f\n", c.dist);
        separations.push_back(sep);
        c.dist = sep;
        manifold_max_sep = fmaxf(manifold_max_sep, sep);
        manifold_min_sep = fminf(manifold_max_sep, sep);
    }
    
    // iterate contacts
    // already set: cp and id
    // set everything else
    for (auto &c : contacts){
        
#ifdef FULLCLIP
        c.dist -= manifold_max_sep;
        if (c.dist < -1.0e-6 || c.id.vertex_id != -1)
            cmanifold.contacts.push_back(c);
#else
        if (c.dist < 0.0f)
            cmanifold.contacts.push_back(c);
#endif
//        if (nflip) c.dist = manifold_min_sep - c.dist;
    }
#if 0
    // DBG render contacts
    printf("max sep %f\n", manifold_max_sep);
    for (auto &c : cmanifold.contacts) { c.cp.debugPrint(); }
    for (auto &c : cmanifold.contacts) { printf("%f, ", c.dist); } printf("\n");
    for (auto &c : cmanifold.contacts) { render_marker(c.cp, 0.025f, 2, {1,0,0,1}); }
    printf("nbr c %d, nflip %s, feat %s\n", cmanifold.contacts.size(), nflip?"yes":"no", feat_id==0?"face A":(feat_id==1?"face B":"edge"));
#endif
    
#if 0
    vec3f SP_p_ = SA_p;// vec3f(0,0,0);
    float manifold_max_sep = -std::numeric_limits<float>::infinity();
    float manifold_min_sep = std::numeric_limits<float>::infinity();
    std::vector<float> separations;
    for (int i=0; i<manifold.size(); i++)
    {
        float sep = dist_plane_vertex(SA_n, SP_p_, manifold[i]);
        manifold_max_sep = fmax(manifold_max_sep, sep);
        manifold_min_sep = fmin(manifold_min_sep, sep);
        separations.push_back(sep);
    }
#endif
    
#if 0
    // 4. create contacts
    for (int i=0; i<manifold.size(); i++)
    {
        contact_point_t c;
        c.cn = SA_n;
        c.cndir = nflip?-1:1;
        c.cp = manifold[i];
        c.dist = separations[i] - manifold_max_sep; // printf("%f\n", manifold_max_sep);
        c.id.geomA = polyA;
        c.id.geomB = polyB;
        cmanifold.contacts.push_back(c);
    }
#endif
    
    return true;
}

/*
 collide convex polyhedra - sphere
 */
//static bool collide_poly_sphere(poly_geom_t *poly, sphere_collider_t *sphere, body_t *bodyA, body_t *bodyB, arbiter_t &arbiter)
//{
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
//    
//    return false;
//}

/*
 collide spheres A <-> B
 */
static bool collide_spheres(sphere_collider_t* sgeomA, sphere_collider_t* sgeomB, body_t* sbodyA, body_t* sbodyB, contact_manifold_t& cm)
{
    vec3f v = sgeomB->p_w - sgeomA->p_w;
    float d = v.norm2();
    if (d > sgeomA->r + sgeomB->r) return false;
    
    // collision -> generate collision data
    contact_point_t c;
    c.dist = d - (sgeomA->r + sgeomB->r);
    vec3f vn = v; vn.normalize();
    c.cn = vn;
    c.cp = sgeomA->p_w + vn*(sgeomA->r + /* changed from '-' */ c.dist);
    c.id.geomA = sgeomA;
    c.id.geomB = sgeomB;
    
    cm.contacts.push_back(c);
    return true;
}

/*
 collide spheres <-> plane
 */
static bool collide_sphere_plane(sphere_collider_t* sgeom, plane_collider_t* pgeom, body_t* sbody, body_t* pbody, contact_manifold_t& cm)
{
    vec3f v = sgeom->p_w - pgeom->p_w;
    float d = v.dot(pgeom->n_w);
    if (d > sgeom->r) return false;
    
    // collision -> generate collision data
    contact_point_t c;
    c.cn = pgeom->n_w;
    c.dist = d-sgeom->r;
    c.cp = sgeom->p_w - pgeom->n_w*d;
    c.id.geomA = sgeom;
    c.id.geomB = pgeom;
    
    cm.contacts.push_back(c);
    return true;
}

static bool collide_poly_point(poly_collider_t* poly,
                               point_collider_t* point,
                               body_t* polyBody,
                               body_t* pointBody,
                               contact_manifold_t& cm,
                               bool nflip = false)
{
    vec3f SA_n;     // separating axis
    vec3f SA_p;     // separating point (from face)
    int SA_ni = -1; // index of SA plane
    float max_sep = -std::numeric_limits<float>::infinity();
    
    // test faces of A -> point
    for (int i=0; i<poly->nbr_faces; i++)
    {
        vec3f n = poly->normals_w[i];
        vec3f p = poly->vertices_w[ poly->faces[i*poly->face_stride+0] ];
        float sep = dist_plane_vertex(n, p, point->p_w);
        if (sep > 0) return false;
        if (sep > max_sep)
        {
            max_sep = sep;
            SA_n = n;
            SA_p = p;
            SA_ni = i;
        }
    }
    
    contact_point_t c;
    c.cp = point->p_w;
    c.cn = SA_n;
    c.cndir = nflip?-1:1;
    c.id = {SA_ni, -1, -1, -1}; // ?
    c.id.geomA = poly;
    c.id.geomB = point;
    c.dist = max_sep;
    cm.contacts.push_back(c);
    
    return true;
}

bool collide_geoms(collider_t* geomA, collider_t* geomB, body_t* bodyA, body_t* bodyB, contact_manifold_t &cm)
{
    if (geomA->gtype == POLYHEDRON)
    {
        poly_collider_t *polyA = static_cast<poly_collider_t*>(geomA);
        
        if (geomB->gtype == POLYHEDRON)
        {
            poly_collider_t *polyB = static_cast<poly_collider_t*>(geomB);
            collide_polys(polyA, polyB, bodyA, bodyB, cm);
        }
        else if (geomB->gtype == SPHERE)
        {
            sphere_collider_t *sgB = static_cast<sphere_collider_t*>(geomB);
            collide_poly_sphere(polyA, sgB, bodyA, bodyB, cm);
        }
        else if (geomB->gtype == PLANE)
        {
            plane_collider_t *pgB = static_cast<plane_collider_t*>(geomB);
            // TODO
        }
        else if (geomB->gtype == POINT)
        {
            point_collider_t *pgB = static_cast<point_collider_t*>(geomB);
            collide_poly_point(polyA, pgB, bodyA, bodyB, cm);
        }
    }
    else if (geomA->gtype == SPHERE)
    {
        sphere_collider_t *sgA = static_cast<sphere_collider_t*>(geomA);
        
        if (geomB->gtype == POLYHEDRON)
        {
            poly_collider_t *polyB = static_cast<poly_collider_t*>(geomB);
            collide_poly_sphere(polyB, sgA, bodyB, bodyA, cm, true);
        }
        else if (geomB->gtype == SPHERE)
        {
            sphere_collider_t *sgB = static_cast<sphere_collider_t*>(geomB);
            collide_spheres(sgA, sgB, bodyA, bodyB, cm);
        }
        else if (geomB->gtype == PLANE)
        {
            plane_collider_t *pg = static_cast<plane_collider_t*>(geomB);
            collide_sphere_plane(sgA, pg, bodyA, bodyB, cm);
        }
    }
    else if (geomA->gtype == PLANE)
    {
        plane_collider_t *pgA = static_cast<plane_collider_t*>(geomA);
        
        if (geomB->gtype == POLYHEDRON)
        {
            poly_collider_t *polyB = static_cast<poly_collider_t*>(geomB);
            //...
        }
        else if (geomB->gtype == SPHERE)
        {
            sphere_collider_t *sg = static_cast<sphere_collider_t*>(geomB);
            collide_sphere_plane(sg, pgA, bodyB, bodyA, cm);
        }
        else if (geomB->gtype == PLANE)
        {
            // should not happen
        }
    }
    else if (geomA->gtype == POINT)
    {
        point_collider_t *pgA = static_cast<point_collider_t*>(geomA);
        
        if (geomB->gtype == POLYHEDRON)
        {
            poly_collider_t *polyB = static_cast<poly_collider_t*>(geomB);
            collide_poly_point(polyB, pgA, bodyB, bodyA, cm, true);
        }
        else if (geomB->gtype == SPHERE)
        {
//            sphere_collider_t *sg = static_cast<sphere_collider_t*>(geomB);
//            collide_sphere_plane(sg, pgA, bodyB, bodyA, cm);
        }
        else if (geomB->gtype == PLANE)
        {
            //
        }
    }
    
    
    return cm.contacts.size() > 0;

    //if (!collide_polys_edges(polyB, polyA, bodyB, bodyA, arbiter)) return false;
    //return true;
}

void collide_bodies(body_t* bodyA, body_t* bodyB, contact_manifold_t& cm)
{
    if (bodyA->is_static && bodyB->is_static) return;
    
    collider_t *geomA, *geomB;
    
    for (int i=0; i<bodyA->colliders.size(); i++)
    {
        geomA = bodyA->colliders[i];
        
        for (int j=0; j<bodyB->colliders.size(); j++)
        {    
            geomB = bodyB->colliders[j];
            
            if (!geomA->AABB_w.intersect(geomB->AABB_w))
                continue;
            
            collide_geoms(geomA, geomB, bodyA, bodyB, cm);
        }
    }
}

void generate_contacts_SAT(std::vector<body_t*>& bodies, std::vector<contact_manifold_t>& cms)
{
    int nbr_bodies_tests = 0;
    
    for (int i=0; i<bodies.size(); i++)
    {
        for (int j=i+1; j<bodies.size(); j++)
        {
            body_t* bodyA = bodies[i];
            body_t* bodyB = bodies[j];
            
            if (!bodyA->AABB_w.intersect(bodyB->AABB_w))
                continue;
            
            contact_manifold_t cm;
            cm.bodyA = bodyA;
            cm.bodyB = bodyB;

            collide_bodies(bodyA, bodyB, cm);
            nbr_bodies_tests++;

            if (cm.contacts.size() > 0) cms.push_back(cm);
        }
    }
#if 0
    // DBG: render contacts
    int nbr_contacts = 0;
//    std::cout << "\nnbr arbiters: " << arbiters.size() << "\n";
    for (contact_manifold_t &arbiter : cms)
    {
        nbr_contacts += arbiter.contacts.size();
        //std::cout << arbiters[i] << "\n";
        //render_marker(arbiter.bodyB->X, 0.02f, 3, vec4f(1, 0, 0, 1));
        render_line(arbiter.bodyA->X, arbiter.bodyB->X, 3, vec4f(0,1,0,1));
#if 0
        for (contact_point_t &contact : arbiter.contacts)
        {
            //if (contact.cndir < 0) std::cout << "XYZ" << std::endl;
            
            vec3f c = contact.cp, cn = c + contact.cn*-contact.dist;
            render_marker(c, 0.02f, 3, vec4f(1, 0, 0, 1));
            render_line(c, cn, 3, vec4f(0,1,0,1));
            render_line(c, c+((cn-c)%vec3f(0,0,1)*0.2), 3, vec4f(0,1,0,1));
            render_line(c, c-((cn-c)%vec3f(0,0,1)*0.2), 3, vec4f(0,1,0,1));
        }
#endif
    }
    printf("nbr bodies %d, nbr collision tests %d, nbr contacts %d\n", (int)bodies.size(), nbr_bodies_tests, nbr_contacts);
#endif
#if 0
    // DBG: find max sep
    float max_sep = 0;
    for (arbiter_t &arbiter : arbiters)
    {
        for (contact_t &c : arbiter.contacts) {
            max_sep = max(max_sep, -c.dist);
        }
    }
    printf("max sep %f\n", max_sep);
    if (max_sep > 0.2) printf("X\n");
#endif
}

