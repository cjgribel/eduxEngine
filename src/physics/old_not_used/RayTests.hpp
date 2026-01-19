//
//  RayTests.hpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-07-28.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef RayTests_hpp
#define RayTests_hpp

#include <entt/entt.hpp>
#include <stdio.h>
#include "vec.h"
#include "ray.h"
#include "Colliders.hpp"

// --- Ray <-> Poly ------------------------------------------------------------

//bool RayPolyIntersection(Ray& ray,
//                         const Handle<ColliderBase> collider);

// --- Ray <-> Sphere ----------------------------------------------------------

//bool RaySphereIntersection(Ray& ray,
//                           const Handle<ColliderBase> collider);

// --- Dummy -------------------------------------------------------------------

//static bool DummyRayIntersection(Ray&,
//                                 const Handle<ColliderBase>)
//{
//    assert(0 && "Intersection test not implemented");
//    return false;
//}

// --- Function dispatch map ---------------------------------------------------

//using RayIntersectionPtr = bool(*)(Ray&,
//                                   const Handle<ColliderBase>);
//
//static std::unordered_map<ColliderType, RayIntersectionPtr>
//RayIntersectionDispatchMap
//{
//    { ColliderType::Sphere, &RaySphereIntersection },
//    { ColliderType::Polyhedron, &RayPolyIntersection },
//    { ColliderType::Plane, &DummyRayIntersection }
//};

//

// 3D Ray to 3D primitive dispatcher
struct Ray3dPrimitive3dIntersectionDispatcher
{
    using RayIntersectionPtr = bool (*)(Ray&,
                                        entt::entity,
                                        entt::registry&);
    
    static bool invoke(Collider3dType collider_type,
                       Ray& ray,
                       entt::entity collider_entity,
                       entt::registry& registry);
};

// 3D Ray to 2D primitive dispatcher
struct Ray3dPrimitive2dIntersectionDispatcher
{
    using RayIntersectionPtr = bool (*)(Ray&,
                                        entt::entity,
                                        entt::registry&);
    
    static bool invoke(Collider2dType collider_type,
                       Ray& ray,
                       entt::entity collider_entity,
                       entt::registry& registry);
};

// 2D Ray to 3D primitive dispatcher
struct Ray2dPrimitive2dIntersectionDispatcher
{
    using RayIntersectionPtr = bool (*)(Ray2d&,
                                        entt::entity,
                                        entt::registry&);
    
    static bool invoke(Collider2dType collider_type,
                       Ray2d& ray,
                       entt::entity collider_entity,
                       entt::registry& registry);
};

#endif /* RayTests_hpp */
