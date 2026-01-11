//
//  UISystems.cpp
//
//  Created by Carl Johan Gribel on 2023-11-04.
//  Copyright © 2023 Carl Johan Gribel. All rights reserved.
//

#include "EditorUISystems.hpp"
//#include "ImPrimitiveRenderer.hpp"
#include "InputManager.h"

// MARK: --- Viewport widgets --------------------------------------------------

#include <cmath>
#include <limits>
#include <iostream>

namespace EditorUI {

namespace {

#define EPSILON 0.0001f

static bool IntersectRayPlane(const Ray& ray,
                              const Plane& plane,
                              double& t_intersection)
{
    double d = dot(ray.dir, plane.n);
    if (std::abs(d) < 1e-6) return false; // Ray parallel to plane
    
    double t = dot(plane.p - ray.origin, plane.n) / d;
    if (t < 0.0) return false; // Intersection behind ray
    
    t_intersection = t;
    //    ip = ray.origin + ray.dir * t; // Intersection
    return true;
}

// Adapted from ClosestPtSegmentSegment by Ericson 2005, page 148.
//
// Computes closest points C1 and C2 of S1(s)=P1+s*(Q1-P1) and
// S2(t)=P2+t*(Q2-P2), returning s and t. Function result is squared // distance between between S1(s) and S2(t)
static float ClosestPtSegmentSegment(v3f seg0_p0,
                                     v3f seg0_p1,
                                     v3f seg1_p0,
                                     v3f seg1_p1,
                                     float &s,
                                     float &t,
                                     v3f &c1,
                                     v3f &c2)
{
    v3f d0 = seg0_p1 - seg0_p0; // Direction vector of segment S1
    v3f d1 = seg1_p1 - seg1_p0; // Direction vector of segment S2
    v3f r = seg0_p0 - seg1_p0;
    float a = dot(d0, d0); // Squared length of segment S1, always nonnegative
    float e = dot(d1, d1); // Squared length of segment S2, always nonnegative
    float f = dot(d1, r);
    
    // Check if either or both segments degenerate into points
    if (a <= EPSILON && e <= EPSILON) {
        // Both segments degenerate into points
        s = t = 0.0f;
        c1 = seg0_p0;
        c2 = seg1_p0;
        return dot(c1 - c2, c1 - c2);
    }
    if (a <= EPSILON) {
        // First segment degenerates into a point
        s = 0.0f;
        t=f/e; //s=0=>t=(b*s+f)/e=f/e
        t = clamp(t, 0.0f, 1.0f);
    }
    else
    {
        float c = dot(d0, r);
        if (e <= EPSILON) {
            // Second segment degenerates into a point
            t = 0.0f;
            s= clamp(-c/a,0.0f,1.0f); //t=0=>s=(b*t-c)/a=-c/a
        } else {
            // The general nondegenerate case starts here
            float b = dot(d0, d1);
            float denom = a*e-b*b; // Always nonnegative
            
            // If segments not parallel, compute closest point on L1 to L2 and
            // clamp to segment S1. Else pick arbitrary s (here 0)
            if (denom != 0.0f)
                s = clamp((b*f - c*e) / denom, 0.0f, 1.0f);
            else
                s = 0.0f;
            // Compute point on L2 closest to S1(s) using
            // t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
            t = (b*s + f) / e;
            
            // If t in [0,1] done. Else clamp t, recompute s for the new value
            // of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a
            // and clamp s to [0, 1]
            if (t < 0.0f)
            {
                t = 0.0f;
                s = clamp(-c / a, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    c1 = seg0_p0 + d0 * s;
    c2 = seg1_p0 + d1 * t;
    return dot(c1 - c2, c1 - c2);
}

static void ClosestPtRaySegment(v3f ray_origin,
                                v3f ray_dir,
                                v3f seg_p0,
                                v3f seg_p1,
                                LineIntersectionResult& res)
{
    v3f seg_dir = seg_p1 - seg_p0; // Direction vector of segment S2
    v3f r = ray_origin - seg_p0;
    float a = dot(ray_dir, ray_dir); // Squared length of segment S1, always nonnegative
    float e = dot(seg_dir, seg_dir); // Squared length of segment S2, always nonnegative
    float f = dot(seg_dir, r);
    
    // Check if either or both segments degenerate into points
    if (a <= EPSILON && e <= EPSILON) {
        // Both segments degenerate into points
        res.s = res.t = 0.0f;
        res.c0 = ray_origin;
        res.c1 = seg_p0;
        res.dist = dot(res.c0 - res.c1, res.c0 - res.c1);
        return;
    }
    if (a <= EPSILON) {
        // First segment degenerates into a point
        res.s = 0.0f;
        res.t = f/e; //s=0=>t=(b*s+f)/e=f/e
        res.t = clamp(res.t, 0.0f, 1.0f);
    }
    else
    {
        float c = dot(ray_dir, r);
        if (e <= EPSILON) {
            // Second segment degenerates into a point
            res.t = 0.0f;
            res.s = std::max(0.0f, -c/a); // clamp(-c/a,0.0f,1.0f); //t=0=>s=(b*t-c)/a=-c/a
        } else {
            // The general nondegenerate case starts here
            float b = dot(ray_dir, seg_dir);
            float denom = a*e-b*b; // Always nonnegative
            
            // If segments not parallel, compute closest point on L1 to L2 and
            // clamp to segment S1. Else pick arbitrary s (here 0)
            if (denom != 0.0f)
                res.s = std::max(0.0f, (b*f - c*e) / denom); //clamp((b*f - c*e) / denom, 0.0f, 1.0f);
            else
                res.s = 0.0f;
            // Compute point on L2 closest to S1(s) using
            // t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
            res.t = (b * res.s + f) / e;
            
            // If t in [0,1] done. Else clamp t, recompute s for the new value
            // of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a
            // and clamp s to [0, 1]
            if (res.t < 0.0f)
            {
                res.t = 0.0f;
                res.s = std::max(0.0f, -c/a); // clamp(-c / a, 0.0f, 1.0f);
            }
            else if (res.t > 1.0f)
            {
                res.t = 1.0f;
                res.s = std::max(0.0f, (b - c) / a); // clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    res.c0 = ray_origin + ray_dir * res.s;
    res.c1 = seg_p0 + seg_dir * res.t;
    res.dist = dot(res.c0 - res.c1, res.c0 - res.c1);
    return;
}

//
static void ClosestPtRayLine(v3f ray_origin,
                             v3f ray_dir,
                             v3f line_origin,
                             v3f line_dir,
                             LineIntersectionResult& res)
{
    //    v3f seg_dir = line_dir - line_origin; // Direction vector of segment S2
    v3f r = ray_origin - line_origin;
    float a = dot(ray_dir, ray_dir); // Squared length of segment S1, always nonnegative
    float e = dot(line_dir, line_dir); // Squared length of segment S2, always nonnegative
    float f = dot(line_dir, r);
    
    // Check if either or both segments degenerate into points
    if (a <= EPSILON && e <= EPSILON) {
        // Both segments degenerate into points
        res.s = res.t = 0.0f;
        res.c0 = ray_origin;
        res.c1 = line_origin;
        res.dist = dot(res.c0 - res.c1, res.c0 - res.c1);
        return;
    }
    if (a <= EPSILON) {
        // First segment degenerates into a point
        res.s = 0.0f;
        res.t = f/e; //s=0=>t=(b*s+f)/e=f/e
        //        t = clamp(t, 0.0f, 1.0f);
    }
    else
    {
        float c = dot(ray_dir, r);
        if (e <= EPSILON) {
            // Second segment degenerates into a point
            res.t = 0.0f; // ???
            res.s = std::max(0.0f, -c/a); // clamp(-c/a,0.0f,1.0f); //t=0=>s=(b*t-c)/a=-c/a
        } else {
            // The general nondegenerate case starts here
            float b = dot(ray_dir, line_dir);
            float denom = a*e-b*b; // Always nonnegative
            
            // If segments not parallel, compute closest point on L1 to L2 and
            // clamp to segment S1. Else pick arbitrary s (here 0)
            if (denom != 0.0f)
                res.s = std::max(0.0f, (b*f - c*e) / denom); //clamp((b*f - c*e) / denom, 0.0f, 1.0f);
            else
                res.s = 0.0f;
            // Compute point on L2 closest to S1(s) using
            // t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
            res.t = (b * res.s + f) / e;
            
            // If t in [0,1] done. Else clamp t, recompute s for the new value
            // of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a
            // and clamp s to [0, 1]
            //            if (t < 0.0f)
            //            {
            ////                t = 0.0f;
            //                s = std::max(0.0f, -c/a); // clamp(-c / a, 0.0f, 1.0f);
            //            }
            //            else if (t > 1.0f)
            //            {
            ////                t = 1.0f;
            //                s = std::max(0.0f, (b - c) / a); // clamp((b - c) / a, 0.0f, 1.0f);
            //            }
        }
    }
    res.c0 = ray_origin + ray_dir * res.s;
    res.c1 = line_origin + line_dir * res.t;
    res.dist = dot(res.c0 - res.c1, res.c0 - res.c1);
}

inline quatf rotq(const v3f& r)
{
    const float rx = r.x * fTO_RAD;
    const float ry = r.y * fTO_RAD;
    const float rz = r.z * fTO_RAD;
    return quatf::rotation(rx, ry, rz);
}

inline m3f rotm(const v3f& r)
{
    return m3f {rotq(r)};
}

inline m3f rotm(const Transform& tfm)
{
    return rotm(tfm.rotation);
}

// Computes a factor that scales an object to a fixed screen-space size
// For example: if the screen-space size of an object is 100 and it's
// fixed size is 50, the factor is going to be 0.5
//
float compute_screenspace_scale(const v3f& world_pos,
                                float screenspace_size,
                                const Camera& camera)
{
    const m4f M = camera.VPProjView;
    v4f p0 = M * xyz1(world_pos);
    v4f p1 = p0 + M * camera.WorldViewInverse.column(0);
    p0 = p0 * (1.0f/p0.w);
    p1 = p1 * (1.0f/p1.w);
    
    return screenspace_size / length(xyz(p0) - xyz(p1));
}

v3f compute_view_consistent_position(const v3f& world_pos,
                                     float screenspace_size,
                                     const Camera& camera)
{
//    return world_pos;
    
    const m4f& P = camera.Proj;
    const m4f& VP = camera.Viewport;
    const auto& vp = camera.viewport;
    const v3f camera_pos = camera.position;
    
    float w = screenspace_size; // 0 -> (r-l)
    //        float vpr = scene.get_main_camera().viewport.r, vpl = scene.get_main_camera().viewport.l;
    //        float vpt = scene.get_main_camera().viewport.t, vpb = scene.get_main_camera().viewport.b;
    //    std::cout << vpr << ", " << vpl << std::endl; // 1280, 0

    float z_view_fromx = VP.m11 * P.m11 * 1.0f /*x_view*/ / -((vp.r-vp.l)/2.0f+w - VP.m14 + VP.m11 * P.m13); //std::cout << z_view_fromx << std::endl;
    // Equivalent
//    float z_view_fromy = VP.m22 * P.m22 * 1.0f /*y_view*/ / -((vp.t-vp.b)/2.0f+w - VP.m24 + VP.m22 * P.m23); //std::cout << z_view_fromy << std::endl;
    
    //
    //    v4f VPPinvp = (VP * P).inverse() * v4f {vpr/2+w, vpl/2+w, 0.0f, 1.0f};
    //    std::cout << VPPinvp * (1.0f/VPPinvp.w) << std::endl;
    //    std::cout << std::endl;
    
    //        v3f cam_pos = scene.get_main_camera().getWorldPosition();
    //        v3f pos = tfm.position;
    //    v3f vv = /*pos - last_ray.origin*/;
    //    pos = (cam_pos + normalize(pos - cam_pos) * (-2.0f/m33)); // z = 0 in SS
    return camera_pos + normalize(camera_pos - world_pos) * z_view_fromx; // z = 0 in SS
}

} // anon namespace

// MARK: --- AxisTranslateSubWidget --------------------------------------------------------

bool AxisTranslateSubWidget::hover(Ray& ray,
                                   const Transform& tfm,
                                   float scale) const
{
    v3f p0 = tfm.position;
    v3f p1 = p0 + (rotm(tfm) * dir) * length * scale;
    LineIntersectionResult arrow_isc;
    ClosestPtRaySegment(ray.origin, ray.dir, p0, p1, arrow_isc);
    
    if (arrow_isc.dist > radius * radius) return false;
    if (arrow_isc.s > ray.z_near) return false;
    ray.z_near = arrow_isc.s;
    return true;
}

void AxisTranslateSubWidget::engage(Ray& ray,
                                    const Transform& tfm,
                                    float scale)
{
    const v3f rdir = (rotm(tfm) * dir);
    float t_pos = dot(tfm.position, rdir);
    v3f p = tfm.position - rdir * t_pos;
    LineIntersectionResult axis_isc;
    ClosestPtRayLine(ray.origin, ray.dir, p, rdir, axis_isc);
    
    t_ofs = axis_isc.t - t_pos;
}

void AxisTranslateSubWidget::update(Ray& ray,
                                    Transform& tfm,
                                    float scale)
{
    const v3f rdir = (rotm(tfm) * dir);
    float t_pos = dot(tfm.position, rdir);
    v3f p = tfm.position - rdir * t_pos;
    LineIntersectionResult axis_isc;
    ClosestPtRayLine(ray.origin, ray.dir, p, rdir, axis_isc);
    
    t_current = axis_isc.t;
    tfm.position = p + rdir * (t_current - t_ofs);
}

void AxisTranslateSubWidget::render(Scene& scene,
                                    std::shared_ptr<ImPrimitiveRenderer> renderer,
                                    const Transform& tfm,
                                    float scale,
                                    WidgetState state) const
{
    const ArrowDescriptor arrdesc
    {
        .cone_fraction = 0.2f,
        .cone_radius = radius * scale * 1.5f,
        .cylinder_radius = radius * scale
    };
    v3f p0 = tfm.position;
    v3f p1 = p0 + (rotm(tfm) * dir) * length * scale;
    
    switch (state) {
        case WidgetState::Engaged:
        case WidgetState::Hovered:
            renderer->push_states(Color4u::Yellow);
            break;
        case WidgetState::Passive:
            renderer->push_states(Color4u::Gray);
            break;
        default:
            renderer->push_states(Color4u {color});
            break;
    }
    
    Color4u used_color = color;
    if (state == WidgetState::Engaged) used_color = Color4u::Yellow;
    if (state == WidgetState::Hovered) used_color = Color4u::Yellow;
    if (state == WidgetState::Passive) used_color = Color4u::Gray;
    renderer->push_states(DepthTest::False, BackfaceCull::True, used_color);
    renderer->push_arrow(p0, p1, arrdesc);
    renderer->pop_states<DepthTest, BackfaceCull, Color4u>();
}

// MARK: --- ScaleSubWidget -------------------------------------------------------

bool ScaleSubWidget::hover(Ray& ray,
                           const Transform& tfm,
                           float scale) const
{
    float t;
    const v3f p = tfm.position + rotm(tfm) * (dir * scale * (1.0f + radius)); //
    const v3f dp = v3f_111 * scale * radius;
    const auto aabb = AABB3d {p - dp, p + dp};
    
    if (!RayAABBIntersection(ray, aabb, t)) return false;
    ray.z_near = t;
    return true;
}

void ScaleSubWidget::engage(Ray& ray,
                            const Transform& tfm,
                            float scale)
{
    const v3f rdir = (rotm(tfm) * dir);
    float t_pos = dot(tfm.position, rdir);
    v3f p = tfm.position - rdir * t_pos;
    LineIntersectionResult axis_isc;
    ClosestPtRayLine(ray.origin, ray.dir, p, rdir, axis_isc);
 
    t_ofs = axis_isc.t;
    scaling_engaged = tfm.scaling;
}

void ScaleSubWidget::update(Ray& ray,
                            Transform& tfm,
                            float scale)
{
    const v3f rdir = (rotm(tfm) * dir);
    float t_pos = dot(tfm.position, rdir);
    v3f p = tfm.position - rdir * t_pos;
    LineIntersectionResult axis_isc;
    ClosestPtRayLine(ray.origin, ray.dir, p, rdir, axis_isc);

    t_current = axis_isc.t;
    tfm.scaling = scaling_engaged * (v3f_111 - dir * (t_ofs - t_current));
}

void ScaleSubWidget::render(Scene& scene,
                            std::shared_ptr<ImPrimitiveRenderer> renderer,
                            const Transform& tfm,
                            float scale,
                            WidgetState state) const
{
    const v3f pos = tfm.position;                       // Main widget position
    const v3f v_pos = dir * ((1.0f + radius) * scale);  // Subwidget relative position
    const v3f v_drag = (state == WidgetState::Engaged? dir * (t_current - t_ofs) : v3f_000); // Dragged distance
    const m4f M = m4f::translation(pos) * m4f{rotm(tfm)} * m4f::translation(v_pos + v_drag) * m4f::scaling(radius * 2 * scale);
  
    Color4u used_color = color;
    if (state == WidgetState::Engaged) used_color = Color4u::Yellow;
    if (state == WidgetState::Hovered) used_color = Color4u::Yellow;
    if (state == WidgetState::Passive) used_color = Color4u::Gray;
    renderer->push_states(DepthTest::False, BackfaceCull::True, used_color, M);
    renderer->push_cube();
    renderer->pop_states<DepthTest, BackfaceCull, Color4u, m4f>();
}

// MARK: --- PlaneTranslateSubWidget -------------------------------------------------------

void PlaneTranslateSubWidget::get_transformed_quad(v3f points[4],
                                                   const Transform& tfm,
                                                   float scale) const
{
    v3f pos = tfm.position;
    m3f RS = rotm(tfm) * (m3f_identity * scale);
    points[0] = pos + RS * puv[0];
    points[1] = points[0] + RS * puv[1];
    points[2] = points[1] + RS * puv[2];
    points[3] = points[0] + RS * puv[2];
}

bool PlaneTranslateSubWidget::hover(Ray& ray,
                                    const Transform& tfm,
                                    float scale) const
{
    v3f points[4];
    get_transformed_quad(points, tfm, scale);
    
    bool tri0 = RayTriangleIntersection(ray, points[0], points[1], points[2]);
    bool tri1 = RayTriangleIntersection(ray, points[0], points[2], points[3]);
    return tri0 || tri1;
}

void PlaneTranslateSubWidget::engage(Ray& ray,
                                     const Transform& tfm,
                                     float scale)
{
    v3f points[4];
    get_transformed_quad(points, tfm, scale);
    
    // Fixed plane throughout engage
    plane_engaged.p = tfm.position;
    plane_engaged.n = normalize(cross(points[1] - points[0], points[2] - points[0]));
    // Fixed engaged point
    double t;
    bool res = IntersectRayPlane(ray, plane_engaged, t);
    assert(res); // Should not fail if previously hovered in a correct way ... but what if it does?
    p_engaged = ray.origin + ray.dir * t;
}

void PlaneTranslateSubWidget::update(Ray& ray,
                                     Transform& tfm,
                                     float scale)
{
    double t;
    bool res = IntersectRayPlane(ray, plane_engaged, t); // plane_engaged.intersect_ray(ray, p);
    if (!res) return; // Ray points away from plane
    v3f p = ray.origin + ray.dir * t;
    
    tfm.position = plane_engaged.p + p - p_engaged;
}

void PlaneTranslateSubWidget::render(Scene& scene,
                                     std::shared_ptr<ImPrimitiveRenderer> renderer,
                                     const Transform& tfm,
                                     float scale,
                                     WidgetState state) const
{
    v3f points[4];
    get_transformed_quad(points, tfm, scale);
    const v3f n = v3f {0.577f,0.577f,0.577f};
    
    Color4u used_color = color;
    if (state == WidgetState::Engaged) used_color = Color4u::Yellow;
    if (state == WidgetState::Hovered) used_color = Color4u::Yellow;
    if (state == WidgetState::Passive) used_color = Color4u::Gray;
    renderer->push_states(DepthTest::False, BackfaceCull::False, used_color);
    renderer->push_quad(points, n);
    renderer->pop_states<Color4u, DepthTest, BackfaceCull>();
}

// MARK: --- RotationSubWidget ----------------------------------------------------

bool RotationSubWidget::hover(Ray& ray, 
                           const Transform& tfm,
                           float scale) const
{
    const v3f plane_n = rotm(tfm) * cross(u, v);
//    if (dot(plane_n, ray.dir) < 0.1f) return false;
    
    Plane plane { tfm.position, plane_n }; // Plane of rotation
    double t;
    bool res = IntersectRayPlane(ray, plane, t); // Ray -> plane
    if (!res) return false;
    
    // Check intersection distance
    const v3f p = ray.origin + ray.dir * t; // Intersection point
    float dist = length(tfm.position - p); // Distance from center to intersection
    if (fabs(dist - radius_outer * scale) > radius_inner) return false;
    
    ray.z_near = t;
    return true;
}

void RotationSubWidget::engage(Ray& ray,
                               const Transform& tfm,
                               float scale)
{
    plane_engaged = Plane { tfm.position, rotm(tfm) * cross(u, v) }; // Plane of rotation
    
    double t;
    bool res = IntersectRayPlane(ray, plane_engaged, t); // Ray -> plane
    assert(res);
    
    // Engaged position and rotation
    p_engaged = ray.origin + ray.dir * t;
    rot_engaged = tfm.rotation;
}

void RotationSubWidget::update(Ray& ray,
                               Transform& tfm,
                               float scale)
{
    // Obtain an intersection point for the current ray w.r.t. engaged plane
    double t;
    bool res = IntersectRayPlane(ray, plane_engaged, t);
    if (!res) return; // Ray points away from plane
    v3f p = ray.origin + ray.dir * t;
    
    // Engaged and current vectors in engaged plane
    const v3f v0 = normalize(p_engaged - plane_engaged.p);
    const v3f v1 = normalize(p - plane_engaged.p);
    // For visualization
    axis0_enagaged = v0;
    axis1_enagaged = v1;
    
    // Angle between engaged & current vectors
    const double det = dot(plane_engaged.n, cross(v0, v1));
    const double diff = atan2(det, dot(v0, v1));
        
    // Apply rotation and set Transform angles
#if 1 
    // Quaternion version
    const quatf drq = quatf::rotation(diff, plane_engaged.n);
    quatf q = drq * rotq(rot_engaged);
    q.normalize();

    // extract_Euler_angles fails when y-rotation is +/-90 degrees
    tfm.rotation = extract_Euler_angles(q) * fTO_DEG;
    if (tfm.rotation.y != tfm.rotation.y) tfm.rotation.y = rot_engaged.y;
#else 
    // Matrix version
    const m3f dr = m3f::rotation(diff, plane_engaged.n);
    const m3f r = dr * rotm(rot_engaged);

    tfm.rotation = extract_Euler_angles(r) * fTO_DEG;
//    if (tfm.rotation.y != tfm.rotation.y) tfm.rotation.y = 90.0f;
#endif
}

void RotationSubWidget::render(Scene& scene,
                               std::shared_ptr<ImPrimitiveRenderer> renderer,
                               const Transform& tfm,
                               float scale,
                               WidgetState state) const
{
//    v3f plane_n = rotm(tfm) * cross(u, v);
//    if (std::fabs(dot(plane_n, scene.get_main_camera().get_forward_vector())) < 0.1f) return;
    
    const m3f r = rotm(tfm);
    const v3f x = r * (u * scale);
    const v3f y = r * (v * scale);
    const v3f z = cross(x, y);
    const m4f m = m4f {
        xyz0(x), xyz0(y), xyz0(z), xyz1(tfm.position)
    };
    
    Color4u c = color;
    if (state == WidgetState::Engaged) c = Color4u::Yellow;
    if (state == WidgetState::Hovered) c = Color4u::Yellow;
    if (state == WidgetState::Passive) c = Color4u::Gray;
    renderer->push_states(DepthTest::False, c, m);
    renderer->push_circle_ring<32>();
    renderer->pop_states<Color4u, m4f>();
    
    if (state == WidgetState::Engaged)
    {
        renderer->push_states(Color4u::Yellow);
        renderer->push_line(plane_engaged.p, plane_engaged.p + axis0_enagaged * scale);
        renderer->push_line(plane_engaged.p, plane_engaged.p + axis1_enagaged * scale);
        renderer->pop_states<Color4u>();
    }
    renderer->pop_states<DepthTest>();
}

// MARK: --- TransformWidgetComponent ------------------------------------------

TransformWidgetComponent::TransformWidgetComponent() :
widgets
{
    // Translation, constrained to cardinal axes
    std::make_shared<AxisTranslateSubWidget>(v3f_100, 0.025f, 1.0f, Color4u::Red, 100.0f),
    std::make_shared<AxisTranslateSubWidget>(v3f_010, 0.025f, 1.0f, Color4u::Lime, 100.0f),
    std::make_shared<AxisTranslateSubWidget>(v3f_001, 0.025f, 1.0f, Color4u::Blue, 100.0f),
    // Translation, constrained to cardinal planes
    std::make_shared<PlaneTranslateSubWidget>(v3f_011 * 0.1f, v3f_010 * 0.2f, v3f_001 * 0.2f, Color4u::Red, 100.0f),
    std::make_shared<PlaneTranslateSubWidget>(v3f_101 * 0.1f, v3f_001 * 0.2f, v3f_100 * 0.2f, Color4u::Lime, 100.0f),
    std::make_shared<PlaneTranslateSubWidget>(v3f_110 * 0.1f, v3f_100 * 0.2f, v3f_010 * 0.2f, Color4u::Blue, 100.0f),
    // Rotation, constrained to cardinal planes
    std::make_shared<RotationSubWidget>(v3f_010, v3f_001, 1.0f, 0.05f, Color4u::Red, 100.0f),
    std::make_shared<RotationSubWidget>(v3f_001, v3f_100, 1.0f, 0.05f, Color4u::Lime, 100.0f),
    std::make_shared<RotationSubWidget>(v3f_100, v3f_010, 1.0f, 0.05f, Color4u::Blue, 100.0f),
    // Scaling, constrained to cardinal planes
    std::make_shared<ScaleSubWidget>(v3f_100, 0.075f, Color4u::Red, 100.0f),
    std::make_shared<ScaleSubWidget>(v3f_010, 0.075f, Color4u::Lime, 100.0f),
    std::make_shared<ScaleSubWidget>(v3f_001, 0.075f, Color4u::Blue, 100.0f)
    
    // + multi-purpose widgets, e.g. scale along all axis simultaneously
}
{
    
}

// MARK: --- TransformWidgetSystem ---------------------------------------------

void TransformWidgetSystem::init(Scene& scene,
                                 entt::dispatcher& dispatcher) { }

void TransformWidgetSystem::update(Scene& scene,
                                   entt::dispatcher& dispatcher,
                                   float dt)
{
    const bool mouse_left = Input::Mouse.left_pressed();
    const bool ctrl = Input::Keyboard->control_pressed();
    
    // World ray from mouse
    const double mouse_x = Input::Mouse.x;
    const double mouse_y = Input::Mouse.y;
    auto& camera = scene.get_main_camera();
    auto ray = camera.world_ray_from_window_coords(mouse_x, mouse_y);
    
    auto view = scene.registry.view<TransformWidgetComponent>();
    
    // NEW
    for(auto entity: view)
    {
        auto& tfmw = view.get<TransformWidgetComponent>(entity);
        
        // No target
        if (!tfmw.target_entity) continue;
        // Do nothing mouse "drags" over widget
        if (!tfmw.any_hovered() && !tfmw.any_engaged() && mouse_left) continue;
        
        auto& tfm = scene.registry.get<Transform>(tfmw.target_entity.entity);
        
        // Hover
        tfmw.hovered_widget = nullptr;
        if (!tfmw.any_engaged())
            for (auto& w : tfmw.widgets)
            {
                float widget_scale = compute_screenspace_scale(tfm.position,  w->screenspace_size, scene.get_main_camera());
                if (w->hover(ray, tfm, widget_scale)) tfmw.hovered_widget = w;
            }
        
        // Engage
        for (auto& w : tfmw.widgets)
        {
            float widget_scale = compute_screenspace_scale(tfm.position,  w->screenspace_size, scene.get_main_camera());
            
            // Another widget is engaged
            if (tfmw.any_engaged() && !tfmw.is_engaged(w))
            {
                continue;
            }
            
            // New engage
            if (!tfmw.is_engaged(w) && tfmw.is_hovered(w) && mouse_left)
            {
                w->engage(ray, tfm, widget_scale);
                tfmw.engage(w);
            }
            
            // Continued engage
            if (tfmw.is_engaged(w) && mouse_left)
            {
                w->update(ray, tfm, widget_scale);
            }
            
            // Engage ends
            if (tfmw.is_engaged(w) && !mouse_left)
            {
                tfmw.disengage();
            }
        }
        
        if (ctrl && tfmw.any_engaged())
        {
            tfm.position.x = std::round(tfm.position.x / tfmw.linear_snap) * tfmw.linear_snap;
            tfm.position.y = std::round(tfm.position.y / tfmw.linear_snap) * tfmw.linear_snap;
            tfm.position.z = std::round(tfm.position.z / tfmw.linear_snap) * tfmw.linear_snap;
            
            tfm.rotation.x = std::round(tfm.rotation.x / tfmw.angular_snap) * tfmw.angular_snap;
            tfm.rotation.y = std::round(tfm.rotation.y / tfmw.angular_snap) * tfmw.angular_snap;
            tfm.rotation.z = std::round(tfm.rotation.z / tfmw.angular_snap) * tfmw.angular_snap;
        }
        
        // Run EVERY FRAME for target entity - will reset ragdoll joints etc to bind config
        Transform::update(tfm);
        EntityOp::UpdateAllSockets(tfmw.target_entity, scene);
    }
}

void TransformWidgetSystem::primitive_render(Scene& scene,
                                             std::shared_ptr<ImPrimitiveRenderer> renderer)
{
//    const auto& camera = scene.get_main_camera();
    auto view = scene.registry.view<TransformWidgetComponent>();
    
    for(auto entity: view)
    {
        auto& tfmw = view.get<TransformWidgetComponent>(entity);
        for (auto& w : tfmw.widgets)
        {
            if (!tfmw.target_entity) continue;
            auto& tfm = scene.registry.get<Transform>(tfmw.target_entity.entity);
            
            // Adjusts widget distance so it renders with a consistent size
//            auto widget_pos = compute_view_consistent_position(tfm.position, w->screenspace_size, camera);
            // Adjusts widget scale so it renders with a consistent size
            float widget_scale = compute_screenspace_scale(tfm.position,  w->screenspace_size, scene.get_main_camera());
            
            WidgetState state;
            if (tfmw.any_engaged())
                state = tfmw.is_engaged(w) ? WidgetState::Engaged : WidgetState::Passive;
            else
                state = tfmw.is_hovered(w) ? WidgetState::Hovered : WidgetState::Default;
            
            w->render(scene,
                      renderer,
                      tfm,
                      widget_scale,
                      state);
        }
    }
}

} // namespace EditorUI
