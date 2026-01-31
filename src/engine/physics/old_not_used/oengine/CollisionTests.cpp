//
//  CollisionTests.cpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-07-26.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "CollisionTests.hpp"
#include "ImPrimitiveRenderer.hpp"

using namespace ImPrimitiveRendererNS;

namespace GlobalDebug {
struct ViewportText {v3f pos; uint color, bgcolor; std::string text, win_name; };
std::vector<ViewportText> viewport_text_cache;

void debug_text(const v3f& point,
                const std::string& text,
                const std::string& label,
                uint color = Color4u::White,
                uint bgcolor = Color4u::Black)
{
    viewport_text_cache.push_back(GlobalDebug::ViewportText { point, color, bgcolor, text, label });
}
}

// MARK: --- 3D Sphere <-> 3D Sphere -------------------------------------------

namespace {
void SphereSphereIntersection(//const Handle<ColliderBase>& colliderA,
                              //const Handle<ColliderBase>& colliderB,
                              entt::entity collider_entityA,
                              entt::entity collider_entityB,
                              entt::registry& registry,
                              std::vector<ContactPoint3d>& contacts,
                              int ndir)
{
    auto sphereA = registry.get<SphereCollider>(collider_entityA);
    auto sphereB = registry.get<SphereCollider>(collider_entityB);
    
//    auto& colliderA = registry.get<Handle<ColliderBase>>(collider_entityA);
//    auto& colliderB = registry.get<Handle<ColliderBase>>(collider_entityB);
//
//    auto sphereA = static_handle_cast<SphereCollider>(colliderA);
//    auto sphereB = static_handle_cast<SphereCollider>(colliderB);
//    assert(sphereA);
//    assert(sphereB);
    
    const v3f posA = sphereA.pos_w;
    const v3f posB = sphereB.pos_w;
    const float rA = sphereA.r_w;
    const float rB = sphereB.r_w;
    
    v3f v = posB - posA;
    float d = v.norm2();
    if (d > (rA + rB)) return;
    v3f vn = normalize(v);
    
    ContactPoint3d cp;
    cp.dist = d - (rA + rB);
    cp.cn = vn;
    cp.cndir = ndir;
    cp.cp = posA + vn*(rA + cp.dist);
    cp.id.colliderA = collider_entityA;
    cp.id.colliderB = collider_entityB;
    cp.id.flip = 0;
    
    // Use "contact flipping" to achieve persistency (better warm starting)
    // based on relative spatial location of the colliders.
    bool flip = false;
    const v3f distv = posA - posB;
    if (fabsf(distv.x) > XI_FEPSILON)
    {
        if (distv.x > 0.0f) flip = true;
    }
    else if (fabsf(distv.y) > XI_FEPSILON)
    {
        if (distv.y > 0.0f) flip = true;
    }
    else if (fabsf(distv.z) > XI_FEPSILON)
    {
        if (distv.z > 0.0f) flip = true;
    }
    if (flip)
    {
        cp.cn *= -1;
        cp.cndir *= -1;
        cp.id.flip = 1;
    }
    
    // Add to manifold
    contacts.push_back(cp);
}

// MARK: --- 3D Sphere <-> 3D Plane- -------------------------------------------

void SpherePlaneIntersection(//const Handle<ColliderBase>& colliderA,
                             //const Handle<ColliderBase>& colliderB,
                             entt::entity collider_entityA,
                             entt::entity collider_entityB,
                             entt::registry& registry,
                             std::vector<ContactPoint3d>& contacts,
                             int ndir)
{
    auto sphere = registry.get<SphereCollider>(collider_entityA);
    auto plane = registry.get<PlaneCollider>(collider_entityB);
    
//    auto& colliderA = registry.get<Handle<ColliderBase>>(collider_entityA);
//    auto& colliderB = registry.get<Handle<ColliderBase>>(collider_entityB);
//
//    auto sphere = static_handle_cast<SphereCollider>(colliderA);
//    auto plane = static_handle_cast<PlaneCollider>(colliderB);
//    assert(sphere);
//    assert(plane);
    
    v3f v = sphere.pos_w - plane.p_w;
    float d = v.dot(plane.n_w);
    if (d > sphere.r_w) return;
    
    // collision -> generate collision data
    ContactPoint3d cp;
    cp.cn = plane.n_w;
    cp.cndir = -ndir;
    cp.dist = d - sphere.r_w;
    cp.cp = sphere.pos_w - plane.n_w * d;
    cp.id.colliderA = collider_entityA;
    cp.id.colliderB = collider_entityB;
    
    contacts.push_back(cp);
}

void PlaneSphereIntersection(//const Handle<ColliderBase>& colliderA,
                             //const Handle<ColliderBase>& colliderB,
                             entt::entity collider_entityA,
                             entt::entity collider_entityB,
                             entt::registry& registry,
                             //body_t *bodyA, body_t *bodyB,
                             //contact_manifold_t &cmanifold,
                             std::vector<ContactPoint3d>& contacts,
                             int ndir)
{
    SpherePlaneIntersection(//colliderB,
                            //colliderA,
                            collider_entityB,
                            collider_entityA,
                            registry,
                            contacts,
                            -ndir);
}

// MARK: --- 3D Polyhedron <-> 3D Sphere ---------------------------------------

/*
 closest-point on triangle to point
 
 credits: Ericson, Real-Time Collision Detection, page 141
 */
inline v3f closestpoint_point_triangle(const v3f& p,
                                       const v3f& a,
                                       const v3f& b,
                                       const v3f& c)
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

void PolySphereIntersection(//const Handle<ColliderBase>& colliderA,
                            //const Handle<ColliderBase>& colliderB,
                            entt::entity collider_entityA,
                            entt::entity collider_entityB,
                            entt::registry& registry,
                            //body_t *bodyA, body_t *bodyB,
                            //contact_manifold_t &cmanifold,
                            std::vector<ContactPoint3d>& contacts,
                            int ndir)
{
    auto poly = registry.get<PolyhedronCollider>(collider_entityA);
    auto sphere = registry.get<SphereCollider>(collider_entityB);
    
//    auto& colliderA = registry.get<Handle<ColliderBase>>(collider_entityA);
//    auto& colliderB = registry.get<Handle<ColliderBase>>(collider_entityB);
//
//    auto poly = static_handle_cast<PolyhedronCollider>(colliderA);
//    auto sphere = static_handle_cast<SphereCollider>(colliderB);
//    assert(poly);
//    assert(sphere);
    
    vec3f cp; // closest point
    float cp_norm = std::numeric_limits<float>::infinity(); // norm closest point -> sphere centre
    const v3f& sphere_p = sphere.pos_w;
    const float& sphere_r = sphere.r_w;
    
    unsigned vindex = 0;
    for (int i = 0; i < poly.nbr_faces; i++)
    {
        for (int j = 1; j < poly.face_strides[i] - 1; j++)
        {
            vec3f v0 = poly.vertices_w[ poly.faces[vindex + 0]    ];
            vec3f v1 = poly.vertices_w[ poly.faces[vindex + j]    ];
            vec3f v2 = poly.vertices_w[ poly.faces[vindex + j+1]  ];
            
            vec3f cp_tmp = closestpoint_point_triangle(sphere_p, v0, v1, v2);
            float cp_tmp_norm = (sphere_p - cp_tmp).norm2squared();
            // check if cp of triangle is closest cp
            if (cp_tmp_norm < cp_norm)
            {
                cp = cp_tmp;
                cp_norm = cp_tmp_norm;
            }
        }
        vindex += poly.face_strides[i];
    }
    
    cp_norm = sqrt(cp_norm);
    
    // no collision
    if (cp_norm > sphere_r)
        return; // false;
    
    // collision
    vec3f v = sphere_p - cp; // collision vector A -> B
    vec3f vn = v; vn.normalize();
    ContactPoint3d c;
    c.dist = cp_norm - sphere_r;
    c.cp = sphere_p - vn*(sphere_r);
    c.cn = vn;
    c.cndir = ndir; // nflip?-1:1;
    c.id.colliderA = collider_entityA;
    c.id.colliderB = collider_entityB;
    //    c.id.geomA = poly;
    //    c.id.geomB = sphere;
    
    contacts.push_back(c);
    //    return true;
}

void SpherePolyIntersection(//const Handle<ColliderBase>& colliderA,
                            //const Handle<ColliderBase>& colliderB,
                            entt::entity collider_entityA,
                            entt::entity collider_entityB,
                            entt::registry& registry,
                            //body_t *bodyA, body_t *bodyB,
                            //contact_manifold_t &cmanifold,
                            std::vector<ContactPoint3d>& contacts,
                            int ndir)
{
    PolySphereIntersection(//colliderB,
                           //colliderA,
                           collider_entityB,
                           collider_entityA,
                           registry,
                           contacts,
                           -ndir);
}

// MARK: --- 3D Polyhedron <-> 3D Polyhedron -----------------------------------

inline float dist_plane_vertex(const v3f& plane_n,
                               const v3f& plane_p,
                               const v3f& p)
{
    return dot(p - plane_p, plane_n);
}

/*
 * min separation: plane -> vertices of poly
 *
 * todo: could use hill-climbing here
 */
inline float min_separation_plane_vertices(const v3f& n,
                                           const v3f& p,
                                           //const poly_collider_t *poly,
                                           const v3f* verts,
                                           size_t nbr_verts)
{
    float min_sep = std::numeric_limits<float>::infinity();
    
    for (size_t i = 0; i < nbr_verts; i++)
    {
        float dist = dist_plane_vertex(n, p, verts[i]);
        if (dist < min_sep) min_sep = dist;
    }
    return min_sep;
}
inline float min_separation_plane_poly(const v3f& n,
                                       const v3f& p,
                                       //const poly_collider_t *poly,
                                       const PolyhedronCollider& poly)
{
    return min_separation_plane_vertices(n, 
                                         p,
                                         poly.vertices_w.data(),
                                         poly.vertices_w.size());
    
//    float min_sep = std::numeric_limits<float>::infinity();
//    
//    for (int i = 0; i < poly.vertices_w.size(); i++)
//    {
//        float dist = dist_plane_vertex(n, p, poly.vertices_w[i]);
//        if (dist < min_sep) min_sep = dist;
//    }
//    return min_sep;
}

/*
 * min & max separation: plane -> vertices of poly
 */
inline void minmax_separation_plane_vertices(const v3f& n,
                                             const v3f& p,
                                             //poly_collider_t *poly,
                                             const v3f* verts,
                                             size_t nbr_verts,
                                             float &min,
                                             float &max)
{
    float min_sep = std::numeric_limits<float>::infinity();
    float max_sep = -std::numeric_limits<float>::infinity();
    
    for (int i = 0; i < nbr_verts; i++)
    {
        float dist = dist_plane_vertex(n, p, verts[i]);
        if (dist < min_sep) min_sep = dist;
        if (dist > max_sep) max_sep = dist;
    }
    min = min_sep;
    max = max_sep;
}

inline void minmax_separation_plane_poly(const v3f& n,
                                         const v3f& p,
                                         //poly_collider_t *poly,
                                         const PolyhedronCollider& poly,
                                         float &min,
                                         float &max)
{
    minmax_separation_plane_vertices(n,
                                     p,
                                     poly.vertices_w.data(),
                                     poly.vertices_w.size(),
                                     min,
                                     max);
    
//    float min_sep = std::numeric_limits<float>::infinity();
//    float max_sep = -std::numeric_limits<float>::infinity();
//    
//    for (int i = 0; i < poly.vertices_w.size(); i++)
//    {
//        float dist = dist_plane_vertex(n, p, poly.vertices_w[i]);
//        if (dist < min_sep) min_sep = dist;
//        if (dist > max_sep) max_sep = dist;
//    }
//    min = min_sep;
//    max = max_sep;
}



/*
 clip edge against plane
 
 returns
 FALSE: edge is outside plane
 TRUE and clipped=FALSE: edge inside or on plane
 TRUE and clipped=TRUE:  edge straddles plane and is clipped (keep as contact point)
 
 principle illustration: Ericson p365, GDrive/misc13
 */
inline bool clip_edge_to_plane(const v3f& plane_n,
                               const v3f& plane_p,
                               v3f& edge_p0,
                               v3f& edge_p1,
                               bool& clipped,
                               bool& clipped0,
                               bool& clipped1)
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
inline void clip_polygons(const PolyhedronCollider& polyA,
                          const PolyhedronCollider& polyB,
                          //poly_collider_t* polyA,
                          //poly_collider_t* polyB,
                          std::vector<ContactPoint3d>& contacts,
                          int flip,
                          int n_exclude = -1)
{
    for (int i = 0; i < polyA.edges.size(); i += 2)
    {
        // edge points
        unsigned vi0 = polyA.edges[i+0];
        unsigned vi1 = polyA.edges[i+1];
        vec3f p0 = polyA.vertices_w[ vi0 ];
        vec3f p1 = polyA.vertices_w[ vi1 ];
        // contacts id's
        ContactPoint3d::ContactID id0, id1;
        
        bool keep = true, clipped = false, clipped0 = false, clipped1 = false;
        
        unsigned vindex = 0;
        for (int j=0; j<polyB.nbr_faces; j++)
        {
            if (j == n_exclude) { continue; }
            vec3f plane_n = polyB.normals_w[j];
            vec3f plane_p = polyB.vertices_w[ polyB.faces[vindex + 0] ];
            
            bool clipped_, clipped0_, clipped1_;
            if (!clip_edge_to_plane(plane_n, plane_p, p0, p1, clipped_, clipped0_, clipped1_)) { keep = false; break; }
            else {
                if (clipped0_)
                    id0 = {j, i, -1, flip};           // FACE-EDGE feature
                else if (!clipped0)
                    id0 = {-1, -1, (int)vi0, flip};   // VERTEX feature
                if (clipped1_)
                    id1 = {j, i, -1, flip};           // FACE-EDGE feature
                else if (!clipped1)
                    id1 = {-1, -1, (int)vi1, flip};   // VERTEX feature
                clipped |= clipped_; clipped0 |= clipped0_; clipped1 |= clipped1_;
            }
            vindex += polyB.face_strides[j];
        }
        
        if (!keep) continue; // edge culled
        // edge clipping completed
        //        if (clipped) {
        //            manifold.push_back(p0);
        //            manifold.push_back(p1);
        //        }
        // use edge point as contacts if clipped or inside
        ContactPoint3d c0; c0.cp = p0; c0.id = id0;
        ContactPoint3d c1; c1.cp = p1; c1.id = id1;
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
inline v3f poly_centre(const PolyhedronCollider& poly)
{
    vec3f vc = vec3f(0,0,0);
    for (const vec3f& v : poly.vertices_w) {
        vc += v;
    }
    vc *= (1.0f/poly.vertices_w.size());
    
    return vc;
}

void PolyPolyIntersection(//const Handle<ColliderBase>& colliderA,
                          //const Handle<ColliderBase>& colliderB,
                          entt::entity collider_entityA,
                          entt::entity collider_entityB,
                          entt::registry& registry,
                          std::vector<ContactPoint3d>& contacts_,
                          int ndir)
{
    auto& polyA = registry.get<PolyhedronCollider>(collider_entityA);
    auto& polyB = registry.get<PolyhedronCollider>(collider_entityB);
 
    // TODO: Tie-breaker to have the polys be tested in the same order
    
//    auto& colliderA = registry.get<Handle<ColliderBase>>(collider_entityA);
//    auto& colliderB = registry.get<Handle<ColliderBase>>(collider_entityB);
//
//    //    Handle<PolyhedronCollider> polyA, polyB;
//    auto polyA = static_handle_cast<PolyhedronCollider>(colliderA);
//    auto polyB = static_handle_cast<PolyhedronCollider>(colliderB);
//    assert(polyA);
//    assert(polyB);
    
    // 1. find sep axis (SA) with max separation
    
    vec3f SA_n;     // separating axis
    vec3f SA_p;     // separating point (from face or edge)
    int SA_ni = -1; // index of SA plane
    bool nflip = false;
    int feat_id; // dbg
    float max_sep = XI_FNINF; // -std::numeric_limits<float>::infinity();
    
    // test faces of A -> vertices of B
    
    unsigned vindex = 0;
    for (int i=0; i<polyA.nbr_faces; i++)
    {
        vec3f n = polyA.normals_w[i];
        vec3f p = polyA.vertices_w[ polyA.faces[vindex + 0] ];
        float sep = min_separation_plane_poly(n, p, polyB); //printf("i %d, sep %f\n", i, sep);
        if (sep > 0) return;
        if (sep > max_sep) // <- TODO: tie breaker here
        {
            max_sep = sep;
            SA_n = n;
            SA_p = p;
            nflip = false;
            // dbg
            SA_ni = i;
            feat_id = 0;
        }
        vindex += polyA.face_strides[i];
    }
    //    printf("max sep %f\n", max_sep);
    
    // test faces of B -> vertices of A
    
    vindex = 0;
    for (int i = 0; i < polyB.nbr_faces; i++)
    {
        vec3f n = polyB.normals_w[i];
        vec3f p = polyB.vertices_w[ polyB.faces[vindex + 0] ];
        float sep = min_separation_plane_poly(n, p, polyA);
        if (sep > 0) return;
        if (sep > max_sep) // <- TODO: tie breaker here
        {
            max_sep = sep;
            SA_n = n; // -n;
            SA_p = p;
            nflip = true;
            // dbg
            SA_ni = i;
            feat_id = 1;
        }
        vindex += polyB.face_strides[i];
    }
    
    // test (unique) edges of A x edges of B
    
    // test
    vec3f polyAc = poly_centre(polyA), polyBc = poly_centre(polyB); // <- TODO: AABB mid point
    //render_marker(polyA->p_w, 0.3, 1, vec4f(1,0,0,1));
    //render_marker(polyB->p_w, 0.3, 1, vec4f(1,0,0,1));
    
    for (int i=0; i<polyA.unique_edge_dirs.size(); i +=2)
    {
        vec3f pA = polyA.vertices_w[ polyA.unique_edge_dirs[i+0] ];
        vec3f eA = polyA.vertices_w[ polyA.unique_edge_dirs[i+1] ] - pA;
        
        for (int j=0; j<polyB.unique_edge_dirs.size(); j +=2)
        {
            vec3f pB = polyB.vertices_w[ polyB.unique_edge_dirs[j+0] ];
            vec3f eB = polyB.vertices_w[ polyB.unique_edge_dirs[j+1] ] - pB;
            
            if (1.0f-fabsf(dot(normalize(eA), normalize(eB))) < 0.0001f) continue;
            
            vec3f n = normalize(eA % eB);
            
            // check support overlap on n
            float Amin, Amax, Bmin, Bmax;
            minmax_separation_plane_poly(n, pA, polyA, Amin, Amax);
            minmax_separation_plane_poly(n, pA, polyB, Bmin, Bmax);
            
            float sep = fmaxf(Amin, Bmin) - fminf(Amax, Bmax);
            if (sep > 0) return;
            if (sep > max_sep) // <- TODO: tie breaker here
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
    
    // Try to eliminate this temporary vector
    static std::vector<ContactPoint3d> contacts;
    contacts.clear();
    
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
    if (contacts.size() == 0) { /*printf("0 contacts, abort\n");*/ return; }
    
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
//    std::vector<float> separations;
    for (auto &c : contacts) {
        c.cn = SA_n;
        c.cndir = nflip?-1:1;
        c.id.colliderA = collider_entityA;
        c.id.colliderB = collider_entityB;
        
        float sep = dist_plane_vertex(SA_n, SA_p, c.cp); //printf("%1.10f\n", c.dist);
//        separations.push_back(sep);
        c.dist = sep;
        manifold_max_sep = fmaxf(manifold_max_sep, sep);
        manifold_min_sep = fminf(manifold_max_sep, sep);
    }
    
//    std::cout << manifold_max_sep << std::endl;
    
    // iterate contacts
    // already set: cp and id
    // set everything else
    for (auto &c : contacts){
        
#ifdef FULLCLIP
        c.dist -= manifold_max_sep;
        if (c.dist < -1.0e-6 || c.id.vertex_id != -1) // PENET_TOL 0.002f
            contacts_.push_back(c);
#else
        if (c.dist < 0.0f)
            contacts_.push_back(c);
#endif
        //        if (nflip) c.dist = manifold_min_sep - c.dist;
    }

//    std::cout << contacts.size() << " -> " << contacts_.size() << std::endl;
    
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
    
    //    return true;
}

// MARK: --- 3D Capsule <-> 3D Capsule (not implemented) -----------------------

// Ericson, page 149.
// Clamp n to lie within the range [min, max]
//float Clamp(float n, float min, float max) {
//    if (n < min) return min;
//    if (n > max) return max;
//    return n;
//}
// Computes closest points C1 and C2 of S1(s)=P1+s*(Q1-P1) and
// S2(t)=P2+t*(Q2-P2), returning s and t. Function result is squared // distance between between S1(s) and S2(t)
//float ClosestPtSegmentSegment(Point p1, Point q1, Point p2, Point q2,
//                              float &s, float &t, Point &c1, Point &c2)
//{
//    Vector d1 = q1 - p1; // Direction vector of segment S1
//    Vector d2 = q2 - p2; // Direction vector of segment S2
//    Vector r = p1 - p2;
//    float a = Dot(d1, d1); // Squared length of segment S1, always nonnegative float e = Dot(d2, d2); // Squared length of segment S2, always nonnegative float f = Dot(d2, r);
//    // Check if either or both segments degenerate into points
//    if (a <= EPSILON && e <= EPSILON) {
//        // Both segments degenerate into points
//        s = t = 0.0f;
//        c1 = p1;
//        c2 = p2;
//        return Dot(c1 - c2, c1 - c2);
//    }
//    if (a <= EPSILON) {
//        // First segment degenerates into a point
//        s = 0.0f;
//        t=f/e; //s=0=>t=(b*s+f)/e=f/e t = Clamp(t, 0.0f, 1.0f);
//    } else {
//        float c = Dot(d1, r);
//        if (e <= EPSILON) {
//            // Second segment degenerates into a point
//            t = 0.0f;
//            s=Clamp(-c/a,0.0f,1.0f); //t=0=>s=(b*t-c)/a=-c/a } else {
//            // The general nondegenerate case starts here
//            float b = Dot(d1, d2);
//            float denom = a*e-b*b; // Always nonnegative
//            // If segments not parallel, compute closest point on L1 to L2 and // clamp to segment S1. Else pick arbitrary s (here 0)
//            if (denom != 0.0f) {
//                s = Clamp((b*f - c*e) / denom, 0.0f, 1.0f);
//            } else s = 0.0f;
//            // Compute point on L2 closest to S1(s) using
//            // t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e t = (b*s + f) / e;
//            // If t in [0,1] done. Else clamp t, recompute s for the new value
//            // of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a // and clamp s to [0, 1]
//            if (t < 0.0f) {
//                t = 0.0f;
//                s = Clamp(-c / a, 0.0f, 1.0f);
//            } else if (t > 1.0f) {
//                t = 1.0f;
//                s = Clamp((b - c) / a, 0.0f, 1.0f);
//            }
//        } }
//    c1 = p1 + d1 * s;
//    c2 = p2 + d2 * t;
//    return Dot(c1 - c2, c1 - c2);
//}

// Ericson, page 114.
//inline void CapsuleCapsuleIntersection(Handle<PolyhedronCollider>& polyA,
//                                       Handle<PolyhedronCollider>& polyB,
//                                       std::vector<ContactPoint3d>& contacts,
//                                       int flip,
//                                       int n_exclude = -1)
//{
//    int TestCapsuleCapsule(Capsule capsule1, Capsule capsule2)
//    {
//        // Compute (squared) distance between the inner structures of the capsules
//        float s, t;
//        Point c1, c2;
//        float dist2 = ClosestPtSegmentSegment(capsule1.a, capsule1.b,
//                                              capsule2.a, capsule2.b, s, t, c1, c2);
//        // If (squared) distance smaller than (squared) sum of radii, they collide
//        float radius = capsule1.r + capsule2.r;
//        return dist2 <= radius * radius;
//    }
//}

// MARK: --- 3D Capsule <-> 3D Sphere (not implemented) ------------------------

// Ericson, page 130.
//// Returns the squared distance between point c and segment ab
//float SqDistPointSegment(Point a, Point b, Point c)
//{
//    Vector ab = b – a, ac = c – a, bc = c – b; float e = Dot(ac, ab);
//    // Handle cases where c projects outside ab if (e <= 0.0f) return Dot(ac, ac);
//    float f = Dot(ab, ab);
//    if (e >= f) return Dot(bc, bc);
//    // Handle cases where c projects onto ab return Dot(ac, ac) – e * e / f;
//}

// Ericson, page 114.
//inline void CapsuleSphereIntersection(Handle<PolyhedronCollider>& polyA,
//                                       Handle<PolyhedronCollider>& polyB,
//                                       std::vector<ContactPoint3d>& contacts,
//                                       int flip,
//                                       int n_exclude = -1)
//{
//    int TestSphereCapsule(Sphere s, Capsule capsule)
//     {
//         // Compute (squared) distance between sphere center and capsule line segment
//         float dist2 = SqDistPointSegment(capsule.a, capsule.b, s.c);
//         // If (squared) distance smaller than (squared) sum of radii, they collide
//         float radius = s.r + capsule.r;
//         return dist2 <= radius * radius;
//     }
//}

// MARK: --- Mesh <-> 3D Polyhedron -----------------------------------

void MeshPolyhedronIntersection(entt::entity collider_entityA,
                                entt::entity collider_entityB,
                                entt::registry& registry,
                                std::vector<ContactPoint3d>& contacts,
                                int ndir)
{
    auto& mesh_base = registry.get<Base3dCollider>(collider_entityA);
    auto& poly_base = registry.get<Base3dCollider>(collider_entityB);
    
    auto& mesh = registry.get<MeshCollider>(collider_entityA);
    auto& poly = registry.get<PolyhedronCollider>(collider_entityB);
    
    // < Stuff cached for mesh & poly >
    // Voided edge & vertex contacts
    
    enum class Type { None, Disjoint, TriFace, PolyFace, EdgeEdge } type;
    
    // For all mesh triangles
    for (int i = 0; i < mesh.faces.size(); i +=3)
    {
        v3f sep_axis, sep_point;
        //    vec3f SA_n;     // separating axis
        //    vec3f SA_p;     // separating point (from face or edge)
        //    int SA_ni = -1; // index of SA plane
        bool nflip = false;
        //    int feat_id; // dbg
        float max_sep = XI_FNINF; // -std::numeric_limits<float>::infinity();
        
        int face_index; // track face
        v3f edge; // track edge
        type = Type::None;
        std::string dbgtext;
        
        const v3f tri_p[3] {
            mesh.vertices_w[mesh.faces[i + 0]],
            mesh.vertices_w[mesh.faces[i + 1]],
            mesh.vertices_w[mesh.faces[i + 2]]
        };
        const v3f& tri_n = mesh.normals_w[i/3];
//        std::cout << tri_n << ", " << tri_p0 << ", " << tri_p1 << ", " << tri_p2 << std::endl;
        
        // --- Triangle face -> vertices of poly -------------------------------
        
        {
            float sep = min_separation_plane_poly(tri_n, tri_p[0], poly); //if (i == 9) std::cout << sep << ",";
            if (sep > 0) { max_sep = sep; } //continue; // TODO: EARLY OUT
            if (sep > max_sep) // <- TODO: tie breaker here
            {
                max_sep = sep;
                sep_axis = tri_n;
                sep_point = tri_p[0];
                nflip = false;
                //            // dbg
                //            SA_ni = i;
                //            feat_id = 0;
                type = Type::TriFace;
            }
        }
        
        // --- Poly faces -> triangle vertices ---------------------------------
        
        unsigned vindex = 0;
        if (max_sep <= 0.0f)
            for (int j = 0; j < poly.nbr_faces; j++)
            {
                vec3f poly_n = poly.normals_w[j];
                vec3f poly_p0 = poly.vertices_w[poly.faces[vindex + 0]];
                //            std::cout << poly_n << ", " << poly_p0 << std::endl;
                
                float sep = min_separation_plane_vertices(poly_n, poly_p0, tri_p, 3);
                //            float sep = XI_FINF;
                //            sep = std::fminf(sep, dist_plane_vertex(poly_n, poly_p0, tri_p[0]));
                //            sep = std::fminf(sep, dist_plane_vertex(poly_n, poly_p0, tri_p[1]));
                //            sep = std::fminf(sep, dist_plane_vertex(poly_n, poly_p0, tri_p[2]));
                
                //            float min_sep = std::numeric_limits<float>::infinity();
                //
                //            for (int i=0; i<poly.vertices_w.size(); i++)
                //            {
                //                float dist = dist_plane_vertex(n, p, poly.vertices_w[i]);
                //                if (dist < min_sep)
                //                    min_sep = dist;
                //            }
                //            return min_sep;
                
                //            float sep = -1; // = min_separation_plane_poly(n, p, polyB); //printf("i %d, sep %f\n", i, sep);
                //            std::cout << "Triangle sep " << sep << std::endl;
                if (sep > 0) { max_sep = sep; break; } // break;} // TODO: EARLY OUT TO NEXT TRIANGLE
                if (sep > max_sep) // <- TODO: tie breaker here
                {
                    max_sep = sep;
                    sep_axis = poly_n;
                    sep_point = poly_p0;
                    nflip = false;
                    //            // dbg
                    //            SA_ni = i;
                    //            feat_id = 0;
                    type = Type::PolyFace;
                    face_index = j;
                }
                vindex += poly.face_strides[j];
                
                //            std::cout << sep << ", ";
            }
//        std::cout << std::endl;
//        std::cout << "max_sep " << max_sep << ",";
//        if (max_sep > 0.0f) continue;
        
        // --- Edges x Edges ---------------------------------------------------
        
        // Triangle edges
        const v3f tri_edges[3] {
            normalize(tri_p[1] - tri_p[0]),
            normalize(tri_p[2] - tri_p[1]),
            normalize(tri_p[0] - tri_p[2]),
        };
        const v3f poly_center = poly_base.aabb_w.get_midpoint();
        const v3f tri_center = (tri_p[0] + tri_p[1] + tri_p[2]) * 1.0f/3;
#if 1
//        vec3f polyAc = poly_centre(polyA), polyBc = poly_centre(polyB); // <- TODO: AABB mid point
        
        if (max_sep <= 0.0f)
            for (int j = 0; j < poly.unique_edge_dirs.size(); j += 2)
            {
                const v3f& poly_p = poly.vertices_w[ poly.unique_edge_dirs[j+0] ];
                const v3f poly_edge = normalize(poly.vertices_w[ poly.unique_edge_dirs[j+1] ] - poly_p);
                
                for (int k = 0; k < 3; k++)
                {
                    //                vec3f pB = polyB.vertices_w[ polyB.unique_edge_dirs[j+0] ];
                    //                vec3f eB = polyB.vertices_w[ polyB.unique_edge_dirs[j+1] ] - pB;
                    
                    // Edges are parallel
                    if (1.0f - fabsf(dot(poly_edge, tri_edges[k])) < 1e-6) continue;
                    //                if (1.0f-fabsf(dot(normalize(eA), normalize(eB))) < 0.0001f) continue;
                    
                    const v3f n = normalize(cross(tri_edges[k], poly_edge)); // DOUBLE
                    //                vec3f n = normalize(eA % eB);
                    // An SA this is parallel to the triangle normal will always have zero separation
                    if (1.0f - fabsf(dot(n, tri_n)) < 1e-6) continue;
                    
                    // check support overlap on n
                    float Amin, Amax, Bmin, Bmax;
                    minmax_separation_plane_vertices(n, tri_p[k], tri_p, 3, Amin, Amax);
                    minmax_separation_plane_poly(n, tri_p[k], poly, Bmin, Bmax);
                    //                minmax_separation_plane_poly(n, pA, polyA, Amin, Amax);
                    //                minmax_separation_plane_poly(n, pA, polyB, Bmin, Bmax);
                    
                    float sep = fmaxf(Amin, Bmin) - fminf(Amax, Bmax);
                    if (sep > 0) { max_sep = sep; break; } //return; // TODO: EARLY OUT TO NEXT TRIANGLE
                    if (sep > max_sep) // <- TODO: tie breaker here
                    {
                        max_sep = sep;
                        sep_axis = n;
                        nflip = (poly_center - tri_center).dot(n) < 0.0f;
                        //                    //                nflip = (polyB->p_w-polyA->p_w).dot(n) < 0.0f;
                        //                    nflip = (polyBc-polyAc).dot(n) < 0.0f;
                        //
                        sep_point = nflip ? poly_p : tri_p[k];
                        //                    // dbg
                        //                    SA_ni = -1;
                        //                    feat_id = 2;
                        type = Type::EdgeEdge;
                        if (nflip) {
                            if (k == 0) edge = tri_p[1] - tri_p[0];
                            if (k == 1) edge = tri_p[2] - tri_p[1];
                            if (k == 2) edge = tri_p[0] - tri_p[2];
                        }
                        else edge = poly_edge;
                        std::stringstream str;
                        str << std::endl << "triedge" << tri_edges[k] << ", polyedge " << poly_edge << std::endl;
                        str << "tri_p[k] " << tri_p[k] << std::endl;
                        str << ", Aminmax (" << Amin << "," << Amax << ") Bminmax(" << Bmin << "," << Bmax << ")" << std::endl;
                        dbgtext += str.str();
                    }
                }
                if (max_sep > 0) break;
            }
#endif
        
        if (max_sep > 0.0f) type = Type::Disjoint;
        assert(type != Type::None);
        
        const ArrowDescriptor arrowdesc
        {
            .cone_fraction = 0.2,
            .cone_radius = 0.05f,
            .cylinder_radius = 0.025f
        };
        GlobalDebug::imrend_global->push_states(DepthTest::False);
        if (type == Type::TriFace)
        {
            GlobalDebug::imrend_global->push_states(Color4u::White);
            GlobalDebug::imrend_global->push_arrow(tri_center, tri_center + sep_axis*2, arrowdesc);
            GlobalDebug::imrend_global->pop_states<Color4u>();
        }
        if (type == Type::PolyFace)
        {
            GlobalDebug::imrend_global->push_states(Color4u::Blue);
            GlobalDebug::imrend_global->push_arrow(sep_point, sep_point + sep_axis*2, arrowdesc);
            GlobalDebug::imrend_global->pop_states<Color4u>();
        }
        if (type == Type::EdgeEdge)
        {
            GlobalDebug::imrend_global->push_states(Color4u::Yellow);
            GlobalDebug::imrend_global->push_arrow((sep_point + edge*0.5f), (sep_point + edge*0.5f) + sep_axis*2, arrowdesc);
            GlobalDebug::imrend_global->pop_states<Color4u>();
        }
        GlobalDebug::imrend_global->pop_states<DepthTest>();
        
        std::string text;
        uint color, bgcolor;
        //        std::cout << "tri " << i/3 << ": ";
        if (type == Type::Disjoint) { text = "Disjoint "; color = Color4u::White; bgcolor = Color4u::Maroon; }
        if (type == Type::TriFace) { text = "TriFace "; color = Color4u::Black; bgcolor = Color4u::Green; }
        if (type == Type::PolyFace) { text = "PolyFace "; color = Color4u::Black; bgcolor = Color4u::Lime; }
        if (type == Type::EdgeEdge) { text = "EdgeEdge "; color = Color4u::White; bgcolor = Color4u::Blue; }
        std::string sep_text = std::to_string(i/3) + std::string("\n") + std::to_string(max_sep);
        std::string win_id = std::string("window") + std::to_string(i);
        GlobalDebug::debug_text(tri_center, text + sep_text + dbgtext, win_id, color, bgcolor);
        
    } // for all triangles
//    std::cout << std::endl;
    
    // --- Clip manifold & pick contacts ---------------------------------------
    
    // ...
    
}

void PolyhedronMeshIntersection(entt::entity collider_entityA,
                                entt::entity collider_entityB,
                                entt::registry& registry,
                                std::vector<ContactPoint3d>& contacts,
                                int ndir)
{
    MeshPolyhedronIntersection(collider_entityB,
                               collider_entityA,
                               registry,
                               contacts,
                               -ndir);
}
    
// MARK: --- 2D AABB <-> 2D AABB -----------------------------------------------

void AABB2dAABB2dIntersection(entt::entity collider_entityA,
                              entt::entity collider_entityB,
                              entt::registry& registry,
                              std::vector<ContactPoint2d>& contacts,
                              int ndir)
{
    auto& aabbA = registry.get<Base2dCollider>(collider_entityA).aabb_w;
    auto& aabbB = registry.get<Base2dCollider>(collider_entityB).aabb_w;
    
    // Assuming AABB tests has already been performed,
    // we can assume these colliders are intersecting.
    
    ContactPoint2d cp0, cp1;
    AABB2d intersecton = aabbA.intersection(aabbB);
    assert(intersecton);
    
    float u = fabsf(aabbA.max.y - aabbB.min.y);
    float d = fabsf(aabbA.min.y - aabbB.max.y);
    float r = fabsf(aabbA.max.x - aabbB.min.x);
    float l = fabsf(aabbA.min.x - aabbB.max.x);
    
    // B separates from A: UP
    if (u < d && u < r && u < l)
    {
        // Contact normal
        cp0.cn = cp1.cn = { 0.0f, 1.0f };
        // Top two points
        cp0.cp = v2f { intersecton.max.x, intersecton.min.y };
        cp1.cp = v2f { intersecton.min.x, intersecton.min.y };
        // Penetration depth
        cp0.dist = cp1.dist = (intersecton.max.y - intersecton.min.y);
        // Vertex IDs
        cp0.id.vertex_id = 2;
        cp1.id.vertex_id = 3;
        cp0.id.flip = cp1.id.flip = 0;
    }
    // B separates from A: DOWN
    else if (d < r && d < l)
    {
        // Contact normal
        cp0.cn = cp1.cn = { 0.0f, -1.0f };
        // Bottom two points
        cp0.cp = v2f { intersecton.max.x, intersecton.max.y };
        cp1.cp = v2f { intersecton.min.x, intersecton.max.y };
        // Penetration depth
        cp0.dist = cp1.dist = (intersecton.max.y - intersecton.min.y);
        // Vertex IDs
        cp0.id.vertex_id = 2;
        cp1.id.vertex_id = 3;
        cp0.id.flip = cp1.id.flip = 1;
    }
    // B separates from A: RIGHT
    else if (r < l)
    {
        // Contact normal
        cp0.cn = cp1.cn = { 1.0f, 0.0f };
        // Left two points
        cp0.cp = v2f { intersecton.min.x, intersecton.min.y };
        cp1.cp = v2f { intersecton.min.x, intersecton.max.y };
        // Penetration depth
        cp0.dist = cp1.dist = (intersecton.max.x - intersecton.min.x);
        // Vertex IDs
        cp0.id.vertex_id = 0;
        cp1.id.vertex_id = 3;
        cp0.id.flip = cp1.id.flip = 0;
    }
    // B separates from A: LEFT
    else
    {
        // Contact normal
        cp0.cn = cp1.cn = { -1.0f, 0.0f };
        // Right two points
        cp0.cp = v2f { intersecton.max.x, intersecton.min.y };
        cp1.cp = v2f { intersecton.max.x, intersecton.max.y };
        // Penetration depth
        cp0.dist = cp1.dist = (intersecton.max.x - intersecton.min.x);
        // Vertex IDs
        cp0.id.vertex_id = 0;
        cp1.id.vertex_id = 3;
        cp0.id.flip = cp1.id.flip = 1;
    }
    
    // Direction
    cp0.cndir = cp1.cndir = 1;
    // Collider IDs
    cp0.id.colliderA = cp1.id.colliderA = collider_entityA;
    cp0.id.colliderB = cp1.id.colliderB = collider_entityB;
    
    contacts.push_back(cp0);
    contacts.push_back(cp1);
}

// MARK: --- 2D AABB <-> 2D Circle (not implemented) ---------------------------

//void CircleAABB2dIntersection(entt::entity collider_entityA,
//                              entt::entity collider_entityB,
//                              entt::registry& registry,
//                              std::vector<ContactPoint2d>& contacts,
//                              int ndir)
//{
//    auto& circle = registry.get<CircleCollider>(collider_entityA);
//    auto& aabb = registry.get<AABB2dCollider>(collider_entityB);
//
//    assert(0);
//}

//void AABB2dCircleIntersection(entt::entity collider_entityA,
//                              entt::entity collider_entityB,
//                              entt::registry& registry,
//                              std::vector<ContactPoint2d>& contacts,
//                              int ndir)
//{
//    assert(0);
//
//    CircleAABB2dIntersection(collider_entityB,
//                             collider_entityA,
//                             registry,
//                             contacts,
//                             -ndir);
//}

// MARK: --- 2D Circle <-> 2D Poly ---------------------------------------------

void Circle2dPoly2dIntersection(entt::entity collider_entityA,
                              entt::entity collider_entityB,
                              entt::registry& registry,
                              std::vector<ContactPoint2d>& contacts,
                              int ndir)
{
    auto& circle = registry.get<CircleCollider>(collider_entityA);
    auto& poly = registry.get<Polygon2dCollider>(collider_entityB);
    
//    assert(0);
    
//    t2CircleGeometry *cgeom = static_cast<t2CircleGeometry*>(bodyA->geometries[cgeomi]);
//    t2PolygonGeometry *pgeom = static_cast<t2PolygonGeometry*>(bodyB->geometries[pgeomi]);
    
    v2f cp, cn;
    float depth;
    int flip = 0; //(ndir == 1? 0 : 1);

    // Test edge features

    v2f n, ev1, ev2, e, c1, c2, c1n;
    float c1dot, c1n_len;

    for(int i = 0; i < poly.nbr_vertices; i++)
    {
        ev1 = poly.vertices_w[i];
        ev2 = poly.vertices_w[(i+1) % poly.nbr_vertices];
        e = ev2 - ev1;
        c1 = circle.pos_w - ev1;
        c2 = circle.pos_w - ev2;
        n = poly.normals_w[i];
        c1dot = dot(c1, n);

        // Test if circle center is within edge normal corridor
        if(dot(e, c1) > 0.0f && dot(-e, c2) > 0.0f)
        {
            if(c1dot > 0.0f)
            {
                // Circle center is inside edge outward corridor
                // Can only be true for one edge
                c1n = n * c1dot;
                c1n_len = length(c1n);
                if(c1n_len <= circle.r_w)
                {
                    cp = circle.pos_w - c1n;
                    cn = n;
                    depth = circle.r_w - c1n_len;
//                    world->contacts->push_back(t2ContactJoint(bodyA, bodyB, cp, cn, depth, cgeomi, pgeomi, 0, i));
                    ContactPoint2d::ContactID id { collider_entityA, collider_entityB, -1, i, flip };
                    if (flip) std::swap(id.colliderA, id.colliderB);
                    contacts.emplace_back(cp, cn, -ndir, depth, id);
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

    // Test vertex features

    // TODO: Abort if dist decrease then incease
    
    vec2f v;
    float v_len;
    
    for(int i = 0; i < poly.nbr_vertices; i++)
    {
        v = circle.pos_w - poly.vertices_w[i];
        v_len = length(v);
        
        // Test if vertex is inside circle
        // Note: Applies to max one vertex (if more, the collision will be
        //       detected by the edge test)
        // Note: The inner corridors are covered by the edge test
        if(v_len < circle.r_w)
        {
            cp = poly.vertices_w[i];
            cn = normalize(circle.pos_w - cp); // vec2f(cgeom->X - cp).normalize();
            depth = circle.r_w - v_len;
//            world->contacts->push_back(t2ContactJoint(bodyA, bodyB, cp, cn, depth, cgeomi, pgeomi, i, 0));
            ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, -1, flip };
            if (flip) std::swap(id.colliderA, id.colliderB);
            contacts.emplace_back(cp, cn, -ndir, depth, id);
            return;
        }
    }
}

void Poly2dCircle2dIntersection(entt::entity collider_entityA,
                              entt::entity collider_entityB,
                              entt::registry& registry,
                              std::vector<ContactPoint2d>& contacts,
                              int ndir)
{
//    assert(0);
    
    Circle2dPoly2dIntersection(collider_entityB,
                             collider_entityA,
                             registry,
                             contacts,
                             -ndir);
}

// MARK: --- 2D Circle <-> 2D Circle -------------------------------------------

void Circle2dCircle2dIntersection(entt::entity collider_entityA,
                                  entt::entity collider_entityB,
                                  entt::registry& registry,
                                  std::vector<ContactPoint2d>& contacts,
                                  int ndir)
{
    auto circleA = registry.get<CircleCollider>(collider_entityA);
    auto circleB = registry.get<CircleCollider>(collider_entityB);
    
    const v2f v = circleB.pos_w - circleA.pos_w;
    const float v_len = length(v);
    const float sum_radius = circleA.r_w + circleB.r_w;
    
    if(sum_radius < v_len || v_len < XI_FEPSILON) return;

    ContactPoint2d cp;
    cp.cn = v / v_len;  // collision normal A -> B
    cp.cndir = ndir;
    cp.dist = sum_radius - v_len; // > 0 if penetrating
    cp.cp = circleA.pos_w + cp.cn * (circleA.r_w - cp.dist);
    cp.id = { collider_entityA, collider_entityB, -1, -1, 0 };
     
     // Use "contact flipping" to achieve persistency (better warm starting)
     // based on relative spatial location of the colliders.
     bool flip = false;
     const v2f distv = circleA.pos_w - circleB.pos_w;
     if (fabsf(distv.x) > XI_FEPSILON)
     {
         if (distv.x > 0.0f) flip = true;
     }
     else if (fabsf(distv.y) > XI_FEPSILON)
     {
         if (distv.y > 0.0f) flip = true;
     }
     if (flip)
     {
         cp.cn *= -1;
         cp.cndir *= -1;
         cp.id.flip = 1;
     }
    
     contacts.push_back(cp);
}

// MARK: --- 2D Poly <-> 2D Poly -----------------------------------------------

bool VertexInsideEdge(const v2f& v,
                      const v2f& edge_v,
                      const v2f& edge_normal)
{
        return dot(v - edge_v, edge_normal) <= 0.0f;
}

inline bool VertexInsidePoly(const v2f& v,
                             const Polygon2dCollider& poly)
{
    for(int i = 0; i < poly.nbr_vertices; i++)
    {
        if(!VertexInsideEdge(v,
                             poly.vertices_w[i],
                             poly.normals_w[i]))
            return false;
    }
    return true;
}

/*
    signed distance between vertex and closest point on edge plane
*/
inline float DistVertexEdge(const v2f& v,
                            const v2f& ev,
                            const v2f& en)
{
    return dot(v - ev, en);
}

/*
    min separating axis for a given edge and a polygon
*/
inline float MinSeparationEdgePoly(const v2f &ev,
                            const v2f &en,
                            const Polygon2dCollider& poly)
{
    float dist, min_dist = XI_FINF;
    for(int i = 0; i < poly.nbr_vertices; i++)
    {
        dist = DistVertexEdge(poly.vertices_w[i], ev, en);
        if(dist < min_dist)
            min_dist = dist;
    }
    return min_dist;
}

/*
    max separating axis for two polygons
*/
inline int MaxSeparatingEdge(const Polygon2dCollider& polyA,
                             const Polygon2dCollider& polyB,
                             float* ret_dist)
{
    int ei = -1;
    float dist, max_dist = -1e10;
    for(int i = 0; i < polyA.nbr_vertices; i++)
    {
        dist = MinSeparationEdgePoly(polyA.vertices_w[i],
                                     polyA.normals_w[i], polyB);
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
inline v2f ClipLinePlane(const v2f& v1,
                         const v2f& v2,
                         const v2f& ev,
                         const v2f& en)
{
    float frac_out = fabs(dot(v1 - ev, en));
    float frac_in = fabs(dot(v2 - ev, en));
    
    return v1 + (v2 - v1) * (frac_out / (frac_out + frac_in));
}

void Poly2dPoly2dIntersection(entt::entity collider_entityA,
                              entt::entity collider_entityB,
                              entt::registry& registry,
                              std::vector<ContactPoint2d>& contacts,
                              int ndir)
{
    auto& polyA = registry.get<Polygon2dCollider>(collider_entityA);
    auto& polyB = registry.get<Polygon2dCollider>(collider_entityB);

    int sepiA, sepiB;   // Index of separating axis for collider A & B
    float sepA, sepB;   // Seperation distance for collider A & B

    sepiA = MaxSeparatingEdge(polyA,
                              polyB,
                              &sepA);
    if(sepiA == -1) return;

    sepiB = MaxSeparatingEdge(polyB,
                              polyA,
                              &sepB);
    if(sepiB == -1) return;

    // Determine which separating axis to use
    // If the difference (in separating distance) is small (common e.g. in
    // stacks) - use tie breaking rules to keep things coherent.
    
    bool use_edgeA;
    float sepdiff = fabsf(sepA) - fabsf(sepB);
//    std::cout << sepdiff << std::endl;
    if (fabsf(sepdiff) > 0.0001f)
    {
        use_edgeA = sepA < sepB;
    }
    else
    {
        auto aabbmidA = registry.get<Base2dCollider>(collider_entityA).aabb_w.get_midpoint();
        auto aabbmidB = registry.get<Base2dCollider>(collider_entityB).aabb_w.get_midpoint();
        
        use_edgeA = false;
        const v2f distv = aabbmidA - aabbmidB;
        if (fabsf(distv.x) > XI_FEPSILON)
        {
            if (distv.x > 0.0f) use_edgeA = true;
        }
        else if (fabsf(distv.y) > XI_FEPSILON)
        {
            if (distv.y > 0.0f) use_edgeA = true;
        }
    }
    
    // Clip one of the colliders against the other and generate contact points
    
    v2f ev, en, cp;             // edge vertex, edge normal, clip point
    v2f vThis, vNext;           // current & next vertex along an edge
    bool
        this_leftIn, next_leftIn,    // state of current / next edge vertex relative left incident plane
        this_rightIn, next_rightIn;    // state of current / next edge vertex relative right incident plane
    float depth;                    // penetration depth

//    if(sepA < sepB)
    if (use_edgeA)
    {
//        return;
        ev = polyB.vertices_w[sepiB];
        en = polyB.normals_w[sepiB];

        for(int i = 0; i < polyA.nbr_vertices; i++)
        {
            vThis = polyA.vertices_w[i];
            vNext = polyA.vertices_w[(i+1) % polyA.nbr_vertices];

            if(VertexInsidePoly(vThis, polyB))
            {
                depth = DistVertexEdge(vThis, ev, en);
//                contacts.push_back(t2ContactJoint(bodyA, bodyB, vThis, en, -depth, pgeomAi, pgeomBi, i, sepiB));
//                ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, sepiB };
//                contacts.emplace_back(vThis, en, -depth, id);
                ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, sepiB, 0 };
                contacts.emplace_back(vThis, en, -1, -depth, id);
                this_leftIn = true;
                this_rightIn = true;
            }
            else
            {
                if(i == 0)
                {
                    this_leftIn = VertexInsideEdge(vThis,
                                                   polyB.vertices_w[(sepiB+1) % polyB.nbr_vertices],
                                                   polyB.normals_w[(sepiB+1) % polyB.nbr_vertices]);
                    this_rightIn = VertexInsideEdge(vThis,
                                                    polyB.vertices_w[(sepiB-1) % polyB.nbr_vertices],
                                                    polyB.normals_w[(sepiB-1) % polyB.nbr_vertices]);
                }
                else
                {
                    this_leftIn = next_leftIn;
                    this_rightIn = next_rightIn;
                }
            }
            next_leftIn = VertexInsideEdge(vNext,
                                           polyB.vertices_w[(sepiB+1) % polyB.nbr_vertices],
                                           polyB.normals_w[(sepiB+1) % polyB.nbr_vertices]);
            next_rightIn = VertexInsideEdge(vNext,
                                            polyB.vertices_w[(sepiB-1) % polyB.nbr_vertices],
                                            polyB.normals_w[(sepiB-1) % polyB.nbr_vertices]);

            if(!this_leftIn && next_leftIn)
            {
                cp = ClipLinePlane(vThis,
                                   vNext,
                                   polyB.vertices_w[(sepiB+1) % polyB.nbr_vertices],
                                   polyB.normals_w[(sepiB+1) % polyB.nbr_vertices]);
                depth = DistVertexEdge(cp, ev, en);
                if(depth <= 0.0f)
                {
//                    world->contacts->push_back(t2ContactJoint(bodyA, bodyB, cp, en, -depth, pgeomAi, pgeomBi, i, sepiB));
//                    ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, sepiB };
//                    contacts.emplace_back(cp, en, -depth, id);
                    ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, sepiB, 0 };
                    contacts.emplace_back(cp, en, -1, -depth, id);
                }
            }
            if(this_rightIn && !next_rightIn)
            {
                cp = ClipLinePlane(vNext,
                                   vThis,
                                   polyB.vertices_w[(sepiB-1) % polyB.nbr_vertices],
                                   polyB.normals_w[(sepiB-1) % polyB.nbr_vertices]);
                depth = DistVertexEdge(cp, ev, en);
                if(depth <= 0.0f)
                {
//                    world->contacts->push_back(t2ContactJoint(bodyA, bodyB, cp, en, -depth, pgeomAi, pgeomBi, i, sepiB));
//                    ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, sepiB };
//                    contacts.emplace_back(cp, en, -depth, id);
                    ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, sepiB, 0 };
                    contacts.emplace_back(cp, en, -1, -depth, id);
                }
            }
        } /* for */
    }
    else
    {
        ev = polyA.vertices_w[sepiA];
        en = polyA.normals_w[sepiA];

        for(int i = 0; i < polyB.nbr_vertices; i++)
        {
            vThis = polyB.vertices_w[i];
            vNext = polyB.vertices_w[(i+1) % polyB.nbr_vertices];

            if(VertexInsidePoly(vThis, polyA))
            {
                depth = DistVertexEdge(vThis, ev, en);
//                world->contacts->push_back(t2ContactJoint(bodyB, bodyA, vThis, en, -depth, pgeomBi, pgeomAi, i, sepiA));
//                ContactPoint2d::ContactID id { collider_entityB, collider_entityA, i, sepiA };
//                contacts.emplace_back(vThis, en, -depth, id);
                ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, sepiA, 1 };
                contacts.emplace_back(vThis, en, 1, -depth, id);
                this_leftIn = true;
                this_rightIn = true;
            }
            else
            {
                if(i == 0)
                {
                    this_leftIn = VertexInsideEdge(vThis,
                                                   polyA.vertices_w[(sepiA+1) % polyA.nbr_vertices],
                                                   polyA.normals_w[(sepiA+1) % polyA.nbr_vertices]);
                    this_rightIn = VertexInsideEdge(vThis,
                                                    polyA.vertices_w[(sepiA-1) % polyA.nbr_vertices],
                                                    polyA.normals_w[(sepiA-1) % polyA.nbr_vertices]);
                }
                else
                {
                    this_leftIn = next_leftIn;
                    this_rightIn = next_rightIn;
                }
            }
            next_leftIn = VertexInsideEdge(vNext,
                                           polyA.vertices_w[(sepiA+1) % polyA.nbr_vertices],
                                           polyA.normals_w[(sepiA+1) % polyA.nbr_vertices]);
            next_rightIn = VertexInsideEdge(vNext,
                                            polyA.vertices_w[(sepiA-1) % polyA.nbr_vertices],
                                            polyA.normals_w[(sepiA-1) % polyA.nbr_vertices]);

            if(!this_leftIn && next_leftIn)
            {
                cp = ClipLinePlane(vThis,
                                   vNext,
                                   polyA.vertices_w[(sepiA+1) % polyA.nbr_vertices],
                                   polyA.normals_w[(sepiA+1) % polyA.nbr_vertices]);
                depth = DistVertexEdge(cp, ev, en);
                if(depth <= 0.0f)
                {
//                    world->contacts->push_back(t2ContactJoint(bodyB, bodyA, cp, en, -depth, pgeomBi, pgeomAi, i, sepiA));
//                    ContactPoint2d::ContactID id { collider_entityB, collider_entityA, i, sepiA };
//                    contacts.emplace_back(cp, en, -depth, id);
                    ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, sepiA, 1 };
                    contacts.emplace_back(cp, en, 1, -depth, id);
                }
            }
            if(this_rightIn && !next_rightIn)
            {
                cp = ClipLinePlane(vNext,
                                   vThis,
                                   polyA.vertices_w[(sepiA-1) % polyA.nbr_vertices],
                                   polyA.normals_w[(sepiA-1) % polyA.nbr_vertices]);
                depth = DistVertexEdge(cp, ev, en);
                if(depth <= 0.0f)
                {
//                    world->contacts->push_back(t2ContactJoint(bodyB, bodyA, cp, en, -depth, pgeomBi, pgeomAi, i, sepiA));
//                    ContactPoint2d::ContactID id { collider_entityB, collider_entityA, i, sepiA };
//                    contacts.emplace_back(cp, en, -depth, id);
                    ContactPoint2d::ContactID id { collider_entityA, collider_entityB, i, sepiA, 1 };
                    contacts.emplace_back(cp, en, 1, -depth, id);
                }
            }
        } /* for */
    } /* if */
}

// MARK: --- Dummy -------------------------------------------------------------

void DummyIntersection(//const Handle<ColliderBase>& sphere,
                       //const Handle<ColliderBase>& plane,
                       entt::entity collider_entityA,
                       entt::entity collider_entityB,
                       entt::registry& registry,
                       std::vector<ContactPoint3d>& contacts,
                       int ndir)
{
    assert(0 && "Intersection test not implemented");
}

} // anonymous namespace (internal linkage)

// MARK: --- Dispatch ----------------------------------------------------------

void Primitive3dIntersectionDispatcher::invoke(Collider3dType collider_typeA,
                                               Collider3dType collider_typeB,
                                               entt::entity collider_entityA,
                                               entt::entity collider_entityB,
                                               entt::registry& registry,
                                               std::vector<ContactPoint3d>& contacts)
{
    static
    std::unordered_map<Collider3dTypePair, PrimitiveIntersectionPtr>
    DispatchMap
    {
        // Sphere <-> Sphere
        { Collider3dTypePair {Collider3dType::Sphere,      Collider3dType::Sphere},     &SphereSphereIntersection },
        // Sphere <-> Plane
//        { Collider3dTypePair {Collider3dType::Sphere,      Collider3dType::Plane},      &SpherePlaneIntersection },
//        { Collider3dTypePair {Collider3dType::Plane,       Collider3dType::Sphere},     &PlaneSphereIntersection },
        // Poly <-> Poly
        { Collider3dTypePair {Collider3dType::Polyhedron,  Collider3dType::Polyhedron}, &PolyPolyIntersection },
        // Poly <-> Sphere
        { Collider3dTypePair {Collider3dType::Polyhedron,  Collider3dType::Sphere},     &PolySphereIntersection },
        { Collider3dTypePair {Collider3dType::Sphere,      Collider3dType::Polyhedron}, &SpherePolyIntersection },
        // Poly <-> Plane
//        { Collider3dTypePair {Collider3dType::Polyhedron,  Collider3dType::Plane},      &DummyIntersection },
//        { Collider3dTypePair {Collider3dType::Plane,       Collider3dType::Polyhedron}, &DummyIntersection },
        
        // Mesh <-> Mesh
        { Collider3dTypePair {Collider3dType::Mesh,         Collider3dType::Mesh},          &DummyIntersection },
        { Collider3dTypePair {Collider3dType::Mesh,         Collider3dType::Mesh},          &DummyIntersection },
        // Mesh <-> Polyhedron
        { Collider3dTypePair {Collider3dType::Mesh,         Collider3dType::Polyhedron},    &MeshPolyhedronIntersection },
        { Collider3dTypePair {Collider3dType::Polyhedron,   Collider3dType::Mesh},          &PolyhedronMeshIntersection },
        // Mesh <-> Sphere
        { Collider3dTypePair {Collider3dType::Mesh,         Collider3dType::Sphere},        &DummyIntersection },
        { Collider3dTypePair {Collider3dType::Sphere,       Collider3dType::Mesh},          &DummyIntersection }
    };
    
    const Collider3dTypePair collider_types {collider_typeA, collider_typeB};
    
    auto& IntersectionTest = DispatchMap.at(collider_types);
    IntersectionTest(collider_entityA,
                     collider_entityB,
                     registry,
                     contacts,
                     1);
}

void Primitive2dIntersectionDispatcher::invoke(Collider2dType collider_typeA,
                                               Collider2dType collider_typeB,
                                               entt::entity collider_entityA,
                                               entt::entity collider_entityB,
                                               entt::registry& registry,
                                               std::vector<ContactPoint2d>& contacts)
{
    static
    std::unordered_map<Collider2dTypePair, PrimitiveIntersectionPtr>
    DispatchMap
    {
        // AABB 2D <-> AABB 2D
//        { Collider2dTypePair {Collider2dType::AABB2d,       Collider2dType::AABB2d},      &AABB2dAABB2dIntersection },
        // Circle 2D <-> Circle 2D
        { Collider2dTypePair {Collider2dType::Circle,   Collider2dType::Circle},    &Circle2dCircle2dIntersection },
        // Poly 2D <-> Poly 2D
        { Collider2dTypePair {Collider2dType::Polygon,  Collider2dType::Polygon},   &Poly2dPoly2dIntersection },
        // Circle 2D <-> Poly 2D
        { Collider2dTypePair {Collider2dType::Circle,   Collider2dType::Polygon},   &Circle2dPoly2dIntersection },
        { Collider2dTypePair {Collider2dType::Polygon,  Collider2dType::Circle},    &Poly2dCircle2dIntersection },

        // Non-implemented/Unsupported tests
        
//        // Circle 2D <-> Poly 2D
//        { Collider2dTypePair {Collider2dType::AABB2d,       Collider2dType::Polygon},   nullptr },
//        { Collider2dTypePair {Collider2dType::Polygon2d,    Collider2dType::AABB2d},      nullptr },
//        // Circle 2D <-> Poly 2D
//        { Collider2dTypePair {Collider2dType::AABB2d,       Collider2dType::Circle},   nullptr },
//        { Collider2dTypePair {Collider2dType::Circle,    Collider2dType::AABB2d},      nullptr }
    };
    
    const Collider2dTypePair collider_types {collider_typeA, collider_typeB};
    
    auto& IntersectionTest = DispatchMap.at(collider_types);
    if (!IntersectionTest) return;
    IntersectionTest(collider_entityA,
                     collider_entityB,
                     registry,
                     contacts,
                     1);
}
