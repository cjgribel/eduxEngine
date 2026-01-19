//
//  RayTests.cpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-07-28.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "RayTests.hpp"

namespace {

// MARK: --- 3D Ray -> 3D Poly -------------------------------------------------

#if 1

// Based on Ericson, page 199-200 & 54-55
//
bool Ray3dPoly3dIntersection(Ray& ray,
                             entt::entity collider_entity,
                             entt::registry& registry)
{
    auto& poly = registry.get<PolyhedronCollider>(collider_entity);
    
//    auto& collider = registry.get<Handle<ColliderBase>>(collider_entity);
//    auto poly = dynamic_handle_cast<PolyhedronCollider>(collider);
//    assert(poly);
    
    const v3f d = ray.dir;
    float tfirst = 0.0f;
    float tlast = XI_FINF;
    v3f nfirst;
    
    unsigned vindex = 0;
    for (int i = 0; i < poly.nbr_faces; i++)
    {
        const v3f plane_p = poly.vertices_w[ poly.faces[vindex + 0]];
        const v3f plane_n = poly.normals_w[i];
        const float plane_d = dot(plane_n, plane_p);
        const float denom = dot(plane_n, d);
        const float dist = plane_d - dot(plane_n, ray.origin);
        
        // Test if segment runs parallel to the plane
        if (denom == 0.0f) {
            // If so, return “no intersection” if segment lies outside plane
            if (dist < /*>*/ 0.0f) { /*std::cout << "1" << std::endl; */ return false; }
        } else {
            // Compute parameterized t value for intersection with current plane
            float t = dist / denom;
            if (denom < 0.0f) {
                // When entering halfspace, update tfirst if t is larger
                if (t > tfirst) { tfirst = t; nfirst = plane_n; }
            } else {
                // When exiting halfspace, update tlast if t is smaller
                if (t < tlast) tlast = t;
            }
            // Exit with “no intersection” if intersection becomes empty
            if (tfirst > tlast) return false;
        }
        
        vindex += poly.face_strides[i];
    }
    
    if (tfirst < ray.z_near)
    {
        ray.z_near = tfirst;
        ray.n_near = nfirst;
        return true;
    }
    return false;
}
#endif

#if 0
bool Ray3dPoly3dIntersection(Ray& ray,
                             entt::entity collider_entity,
                             entt::registry& registry)
{
    auto& collider = registry.get<Handle<ColliderBase>>(collider_entity);
    
    //auto poly = collider.dynamic_handle_cast<PolyhedronCollider>();
    auto poly = dynamic_handle_cast<PolyhedronCollider>(collider);
    assert(poly);
    
    bool hit = false;
    unsigned vindex = 0;
    for (int i = 0; i < poly->nbr_faces; i++)
    {
        for (int j = 1; j < poly->face_strides[i] - 1; j++)
        {
            v3f v0 = poly->vertices_w[ poly->faces[vindex + 0]];
            v3f v1 = poly->vertices_w[ poly->faces[vindex + j]];
            v3f v2 = poly->vertices_w[ poly->faces[vindex + j+1]];
            
            hit |= RayTriangleIntersection(ray, v0, v1, v2);
        }
        vindex += poly->face_strides[i];
    }
    return hit;
}
#endif

// MARK: --- 3D Ray -> 3D Sphere -----------------------------------------------

/*
 * line-sphere intersection
 * http://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
 */
bool Ray3dSphere3dIntersection(Ray& ray,
                               entt::entity collider_entity,
                               entt::registry& registry)
  {
    auto& sphere = registry.get<SphereCollider>(collider_entity);
    
//    auto& collider = registry.get<Handle<ColliderBase>>(collider_entity);
//    auto sphere = dynamic_handle_cast<SphereCollider>(collider);
//    assert(sphere);
    
    const v3f& sphere_p = sphere.pos_w;
    const float sphere_r = sphere.r_w;
    
    float loc = ray.dir.dot(ray.origin - sphere_p);
    float oc = (ray.origin - sphere_p).norm2();
    float sr = loc*loc - oc*oc + sphere_r*sphere_r;
    
    if (sr >= 0)
    {
        // hit; is it closer than previous hit?
        float d = -loc - sqrt(sr);
//        if (/*!ray ||*/ d < ray.znear) {
        if (d >= 0 && d < ray.z_near) {
            ray.z_near = d;
            ray.n_near = normalize(ray.origin + ray.dir * d - sphere_p);
            return true;
        }
    }
    return false;
}

// MARK: 3D Ray -> 2D Collider intersections

// MARK: --- 3D Ray -> 2D AABB -------------------------------------------------

bool Ray3dAABB2dIntersection(Ray& ray,
                             entt::entity collider_entity,
                             entt::registry& registry)
{
    auto& collider = registry.get<Base2dCollider>(collider_entity);
    
    float t_min {};
    if(!RayAABBIntersection(ray,
                            toAABB3d(collider.aabb_w),
                            t_min))
        return false;
    
    if (t_min > ray.z_near)
        return false;
        
    ray.z_near = t_min;
    return true;
}

// MARK: --- 3D Ray -> 2D Circle -----------------------------------------------

bool Ray3dCircle2dIntersection(Ray& ray,
                               entt::entity collider_entity,
                               entt::registry& registry)
{
//    assert(0);
    auto& circle = registry.get<CircleCollider>(collider_entity);
    
    // https://math.stackexchange.com/questions/395119/ray-disk-intersection
    
    // First intersect a copy of the ray with the z = 0 plane
    // (Don't modify the original ray yet)
    Ray ray_probe = ray;
    if (!Ray3dPlane3dIntersection(ray_probe, v3f_001, v3f_000))
    {
        // Ignore if parallel to the the z = 0 plane
        return false;
    }
    
    // Now intersect with circle within the z = 0 plane
    // Update the original ray if there is an intersection
    const v2f v = xy(ray_probe.point_of_contact()) - circle.pos_w;
    const float r_squared = circle.r_w * circle.r_w;
    
    if (length_squared(v) > r_squared) return false;
    if (ray_probe.z_near >= 0 && ray_probe.z_near < ray.z_near)
    {
        ray.z_near = ray_probe.z_near;
        ray.n_near = ray_probe.n_near;
        return true;
    }
    return false;
}

// MARK: --- 3D Ray -> 2D Poly -------------------------------------------------

bool Ray3dPoly2dIntersection(Ray& ray,
                             entt::entity collider_entity,
                             entt::registry& registry)
{
    auto& poly = registry.get<Polygon2dCollider>(collider_entity);
    
    const v3f v0 = xy0(poly.vertices_w[0]);
    for (int i = 1; i < poly.nbr_vertices - 1; i++)
    {
        const v3f v1 = xy0(poly.vertices_w[i]);
        const v3f v2 = xy0(poly.vertices_w[(i+1)%poly.nbr_vertices]);
        
        if (RayTriangleIntersection(ray, v0, v1, v2))
            return true;
    }
    return false;
}

#if 0
bool Ray3dPoly2dIntersection(Ray& ray,
                             entt::entity collider_entity,
                             entt::registry& registry)
{
    auto& poly = registry.get<Polygon2dCollider>(collider_entity);
    
    bool hit = false;
    const v3f v0 = xy0(poly.vertices_w[0]);
    for (int i = 1; i < poly.nbr_vertices - 1; i++)
    {
        const v3f v1 = xy0(poly.vertices_w[i]);
        const v3f v2 = xy0(poly.vertices_w[(i+1)%poly.nbr_vertices]);
        
        hit |= RayTriangleIntersection(ray, v0, v1, v2);
    }
    return hit;
}
#endif

// MARK: --- 2D Ray -> 2D AABB -------------------------------------------------

bool Ray2dAABB2dIntersection(Ray2d& ray,
                             entt::entity collider_entity,
                             entt::registry& registry)
{
    // 1) Mid-phase 2D raycast (all colliders)
    // 2) Narrow phase when collider = aabb2d
    
    // NOT USED for 1) - BruteForceCollision2dSystem::raycast2d converts
    // the ray & AABB to 3d and uses RayAABBIntersection
    
    // Not implemented for 2)
    
//    assert(0);
    return false;
}

// MARK: --- 2D Ray -> 2D Circle -----------------------------------------------

/// \link https://math.stackexchange.com/a/2536095
///
bool Ray2dCircle2dIntersection(Ray2d& ray,
                               entt::entity collider_entity,
                               entt::registry& registry)
{
//    assert(0);
    auto& circle = registry.get<CircleCollider>(collider_entity);
 
    // Move ray origin to circle's origin
    v2f ray_origin_rel = ray.origin - circle.pos_w;
    // Quadratic polynomial coefficients
    float a = dot(ray.dir, ray.dir);
    float b = 2.0f * dot(ray_origin_rel, ray.dir);
    float c = dot(ray_origin_rel, ray_origin_rel) - circle.r_w * circle.r_w;
    // Discriminant
    float d = b*b - 4.0f * a*c;
    
    // No intersection (imaginary roots)
    if (d < 0.0f) return false;
    
    // One or two intersections
    float t;
    float a2inv = 0.5f/a;
    if (d < XI_FEPSILON)
        t = -b * a2inv;
    else
        t = std::min((-b - sqrt(d)) * a2inv,
                     (-b + sqrt(d)) * a2inv);
    
    if (t >= 0.0f && t < ray.z_near)
    {
        ray.z_near = t;
        ray.n_near = normalize(ray.point_of_contact() - circle.pos_w);
        return true;
    }
    return false;
}

// MARK: --- 2D Ray -> 2D Poly -------------------------------------------------

#if 0
// Ericson, page 152
// Returns 2 times the signed triangle area. The result is positive if
// abc is ccw, negative if abc is cw, zero if abc is degenerate.
float Signed2DTriArea(const v2f& a, const v2f& b, const v2f& c)
{
    return (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
}

// Test if segments ab and cd overlap. If they do, compute and return
// intersection t value along ab and intersection position p
bool Test2DSegmentSegment(const v2f& a,
                                const v2f& b,
                                const v2f& c,
                                const v2f& d,
                                float &t,
                                v2f& p)
{
    // Sign of areas correspond to which side of ab points c and d are
    float a1 = Signed2DTriArea(a, b, d); // Compute winding of abd (+ or -)
    float a2 = Signed2DTriArea(a, b, c); // To intersect, must have sign opposite of a1
    // If c and d are on different sides of ab, areas have different signs
    if (a1 * a2 < 0.0f) {
        // Compute signs for a and b with respect to segment cd
        float a3 = Signed2DTriArea(c, d, a);
        // Compute winding of cda (+ or -)
        // Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
        float a4 = Signed2DTriArea(c, d, b);
        // Must have opposite sign of a3 float a4 = a3 + a2 - a1;
        // Points a and b on different sides of cd if areas have different signs if (a3 * a4 < 0.0f) {
        // Segments intersect. Find intersection point along L(t) = a + t * (b - a).
        // Given height h1 of an over cd and height h2 of b over cd,
        // t = h1 / (h1 - h2) = (b*h1/2) / (b*h1/2 - b*h2/2) = a3 / (a3 - a4),
        // where b (the base of the triangles cda and cdb, i.e., the length
        // of cd) cancels out.
        t = a3 / (a3 - a4);
        p = a + (b - a) * t;
        return true;
    }
    // Segments not intersecting (or collinear)
    return false;
}
#endif

bool Ray2dPoly2dIntersection(Ray2d& ray,
                             entt::entity collider_entity,
                             entt::registry& registry)
{
    auto& poly = registry.get<Polygon2dCollider>(collider_entity);
    
    // 2D ray → line segment ("edge")?
    
    // For all edges = line segments
        // Test2DSegmentSegment(...)
    
    // https://docs.google.com/document/d/12zwtHHG7sj5aCY47fW2IPkye1XDoUWcnIvansodr7u8/edit#bookmark=id.bx6shbbfy9zb
    
//    for (int i = 0; i < poly.nbr_vertices; ++i)
//    {
//        const v2f v0 = poly.vertices_w[i];
////        const v2f v1 = poly.vertices_w[(i+1)%poly.nbr_vertices];
//        const v2f edge_n = poly.normals_w[i];
//
//        float t = Ray2dPlane2dIntersection(ray, edge_n, v0);
//
//        if (t >= 0 && t < ray.z_near) {
//            ray.z_near = t;
//            ray.n_near = edge_n;
//            return true;
//        }
//    }
//    return false;
    
    const v2f d = ray.dir;
    float tfirst = 0.0f;
    float tlast = XI_FINF;
    v2f nfirst;
    
//    unsigned vindex = 0;
    for (int i = 0; i < poly.nbr_vertices; i++)
    {
        const v2f plane_p = poly.vertices_w[i];
        const v2f plane_n = poly.normals_w[i];
        const float plane_d = dot(plane_n, plane_p);
        const float denom = dot(plane_n, d);
        const float dist = plane_d - dot(plane_n, ray.origin);
        
        // Test if segment runs parallel to the plane
        if (denom == 0.0f) {
            // If so, return “no intersection” if segment lies outside plane
            if (dist < /*>*/ 0.0f) { /*std::cout << "1" << std::endl; */ return false; }
        } else {
            // Compute parameterized t value for intersection with current plane
            float t = dist / denom;
            if (denom < 0.0f) {
                // When entering halfspace, update tfirst if t is larger
                if (t > tfirst) { tfirst = t; nfirst = plane_n; }
            } else {
                // When exiting halfspace, update tlast if t is smaller
                if (t < tlast) tlast = t;
            }
            // Exit with “no intersection” if intersection becomes empty
            if (tfirst > tlast) return false;
        }
//        vindex += poly->face_strides[i];
    }
    
    if (tfirst < ray.z_near)
    {
        ray.z_near = tfirst;
        ray.n_near = nfirst;
        return true;
    }
    return false;
    
}

// MARK: --- Dummies -----------------------------------------------------------

bool DummyRay3dIntersection(Ray& ray,
                          entt::entity collider_entity,
                          entt::registry& registry)
{
    assert(0 && "Intersection test not implemented");
    return false;
}

bool DummyRay2dIntersection(Ray2d& ray,
                            entt::entity collider_entity,
                            entt::registry& registry)
{
    assert(0 && "Intersection test not implemented");
    return false;
}

} // Anonymous namespace

// MARK: --- Dispatch ----------------------------------------------------------

bool Ray3dPrimitive3dIntersectionDispatcher::invoke(Collider3dType collider_type,
                                                    Ray& ray,
                                                    entt::entity collider_entity,
                                                    entt::registry& registry)
{
    static
    std::unordered_map<Collider3dType, RayIntersectionPtr>
    DispatchMap
    {
        { Collider3dType::Sphere,     &Ray3dSphere3dIntersection },
        { Collider3dType::Polyhedron, &Ray3dPoly3dIntersection },
        { Collider3dType::Plane,      &DummyRay3dIntersection }
    };
    
    auto& IntersectionTest = DispatchMap.at(collider_type);
    return IntersectionTest(ray,
                            collider_entity,
                            registry);
}

bool Ray3dPrimitive2dIntersectionDispatcher::invoke(Collider2dType collider_type,
                                                    Ray& ray,
                                                    entt::entity collider_entity,
                                                    entt::registry& registry)
{
    static
    std::unordered_map<Collider2dType, RayIntersectionPtr>
    DispatchMap
    {
//        { Collider2dType::AABB2d,   &Ray3dAABB2dIntersection },
        { Collider2dType::Circle,   &Ray3dCircle2dIntersection },
        { Collider2dType::Polygon,  &Ray3dPoly2dIntersection }
    };
    
    auto& IntersectionTest = DispatchMap.at(collider_type);
    return IntersectionTest(ray,
                            collider_entity,
                            registry);
}

bool Ray2dPrimitive2dIntersectionDispatcher::invoke(Collider2dType collider_type,
                                                    Ray2d& ray,
                                                    entt::entity collider_entity,
                                                    entt::registry& registry)
{
    static
    std::unordered_map<Collider2dType, RayIntersectionPtr>
    DispatchMap
    {
//        { Collider2dType::AABB2d,   &Ray2dAABB2dIntersection },
        { Collider2dType::Circle,   &Ray2dCircle2dIntersection },
        { Collider2dType::Polygon,  &Ray2dPoly2dIntersection }
    };
    
    auto& IntersectionTest = DispatchMap.at(collider_type);
    return IntersectionTest(ray,
                            collider_entity,
                            registry);
}
