//
//  ColliderShadeSystem.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-10.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "ColliderShadeSystem.hpp"
#include "Scene.hpp"
#include "SceneAPI.hpp"

using namespace RigidBody;

namespace {
m4f
get_collider_world_transform(entt::entity collider_entity,
                             Scene& scene)
{
    auto& registry = scene.registry;
    auto& resources = scene.resources;
    
    // World transform via BoneSocket
    auto& socket = registry.get<BoneSocket>(collider_entity);
    if (socket.isActive && socket.index != -1)
    {
        assert(socket.primary_entity);
        
        auto& bone_pe_tfm = registry.get<Transform>(socket.primary_entity.entity).global_tfm;
  
        m4f Bone, Bib;
        bool res = SkeletonOp::Get_Bone_Transforms(socket.primary_entity, socket.index, scene, Bone, Bib);
        assert(res);
        
        return bone_pe_tfm * Bone * socket.D;
    }
    
    // World transform via RB Socket ???
    
    // World transform via parent (Primary) entity
    auto parent_entity = registry.get<PrimaryEntity>(collider_entity).entity;
    auto& world_tfm = registry.get<Transform>(parent_entity).global_tfm;
    
    return world_tfm;
    //return world_tfm * m4f::scaling(extract_scalinginv(world_tfm));
}

// Truncate 4D homogeneous to 3D homogeneous transform
// setting z-position to zero.
m3f
get_truncated(const m4f& m)
{
    v3f sv = extract_scaling(m);
    float theta = extract_Euler_angle_z(m);
    return m3f(sv.x * cos(theta), sv.y * -sin(theta), m.m14,
               sv.x * sin(theta), sv.y * cos(theta), m.m24,
               0.0f, 0.0f, 1.0f);
    
#if 0
    // R
    m3f m3d = m3f::rotation_z(extract_Euler_angle_z(m));
    
    // S
    v3f sv = extract_scaling(m);
    m3d.m11 *= sv.x;
    m3d.m22 *= sv.y;
    
    // T
    m3d.m13 = m.m14;
    m3d.m23 = m.m24;
    
    return m3d;
#endif
}

} // Anonymous namespace

// update(...)
// After tree has been traversed
// Tfm all colliders to world space based on Handle<Transform> & bones
// Also update AABB's ?
void Collider2dShadeSystem::update(float dt,
                                  Scene& scene)
{
    auto& registry = scene.registry;
    auto& resources = scene.resources;
    
#if 0
    // AABB2d
    //
    auto aabb2d_view = registry.view<Base2dCollider, AABB2dCollider>();
    for(auto entity : aabb2d_view)
    {
        auto& base_collider = aabb2d_view.get<Base2dCollider>(entity);
        const m4f& W = get_collider_world_transform(entity, registry);
        const m3f W2d = get_truncated(W); // TODO: Test - truncate transform to 2D
        
        base_collider.aabb_w = base_collider.aabb.post_transform(W2d);
    }
#endif
    
    // Poly2d
    //
    {
        auto view = registry.view<Base2dCollider, Polygon2dCollider>();
        for(auto entity : view)
        {
            auto& base_collider = view.get<Base2dCollider>(entity);
            auto& poly = view.get<Polygon2dCollider>(entity);
            
            const m4f& W = get_collider_world_transform(entity, scene);
            const m3f W2d = get_truncated(W); // TODO: Test - truncate transform to 2D
            
            for (int i = 0; i < poly.nbr_vertices; i++)
                poly.vertices_w[i] = xy(W2d * xy1(poly.vertices_loc[i]));
            for (int i = 0; i < poly.nbr_vertices; i++)
                poly.normals_w[i] = normalize(xy(W2d * xy0(poly.normals_loc[i])));
            
            // If a BoneSocket is present, W may have any 3D tfm
            base_collider.aabb_w = base_collider.aabb.post_transform(W2d);
        }
    }
    
    // Circle
    //
    {
        auto view = registry.view<Base2dCollider, CircleCollider>();
        for(auto entity : view)
        {
            auto& base_collider = view.get<Base2dCollider>(entity);
            auto& circle = view.get<CircleCollider>(entity);
            
            const m4f& W = get_collider_world_transform(entity, scene);
            const float W_s = extract_scaling(W).x;
            const m3f W2d = get_truncated(W); // TODO: Test - truncate transform to 2D
            
            circle.pos_w = xy(W2d * xy1(circle.pos));
            circle.r_w = W_s * circle.r;
            
            // If a BoneSocket is present, W may have any 3D tfm
            base_collider.aabb_w = base_collider.aabb.post_transform(W2d);
        }
    }
}

void Collider3dShadeSystem::update(float dt,
                                  Scene& scene)
{
    auto& registry = scene.registry;
//    auto& resources = scene.resources;
    
    // Sphere
    {
        auto view = registry.view<Base3dCollider, SphereCollider>();
        for(auto entity : view)
        {
            auto& base_collider = view.get<Base3dCollider>(entity);
            auto& sphere = view.get<SphereCollider>(entity);
            
            const m4f& W = get_collider_world_transform(entity, scene);
            const float W_s = extract_scaling(W).x;
            
            sphere.pos_w = xyz(W * xyz1(sphere.pos));
            sphere.r_w = W_s * sphere.r;
            
            // If a BoneSocket is present, W may have any 3D tfm
            base_collider.aabb_w = base_collider.aabb.post_transform(W);
        }
    }
    
    // Polyhedron
    //
    {
        auto view = registry.view<Base3dCollider, PolyhedronCollider>();
        for(auto entity : view)
        {
            auto& base_collider = view.get<Base3dCollider>(entity);
            auto& poly = view.get<PolyhedronCollider>(entity);
            
            const m4f& W = get_collider_world_transform(entity, scene);
//            const m3f W2d = get_truncated(W); // TODO: Test - truncate transform to 2D
            
            for (int i = 0; i < poly.vertices_loc.size(); i++)
                poly.vertices_w[i] = xyz(W * xyz1(poly.vertices_loc[i]));
            for (int i = 0; i < poly.normals_loc.size(); i++)
                poly.normals_w[i] = normalize(xyz(W * xyz0(poly.normals_loc[i])));
//            for (int i = 0; i < poly.nbr_vertices; i++)
//                poly.vertices_w[i] = xy(W2d * xy1(poly.vertices[i]));
//            for (int i = 0; i < poly.nbr_vertices; i++)
//                poly.normals_w[i] = xy(W2d * xy0(poly.normals[i]));
            
            // If a BoneSocket is present, W may have any 3D tfm
            base_collider.aabb_w = base_collider.aabb.post_transform(W);
        }
    }
    
    // MeshCollider
    //
    {
        auto view = registry.view<Base3dCollider, MeshCollider>();
        for(auto entity : view)
        {
            auto& base_collider = view.get<Base3dCollider>(entity);
            auto& mesh = view.get<MeshCollider>(entity);
            
            const m4f& W = get_collider_world_transform(entity, scene);
//            const m3f W2d = get_truncated(W); // TODO: Test - truncate transform to 2D
            
            for (int i = 0; i < mesh.vertices_loc.size(); i++)
                mesh.vertices_w[i] = xyz(W * xyz1(mesh.vertices_loc[i]));
            for (int i = 0; i < mesh.normals_loc.size(); i++)
                mesh.normals_w[i] = normalize(xyz(W * xyz0(mesh.normals_loc[i])));
            
            // If a BoneSocket is present, W may have any 3D tfm
            base_collider.aabb_w = base_collider.aabb.post_transform(W);
        }
    }
}
