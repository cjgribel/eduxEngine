//
//  CollisionTests.hpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-07-26.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef CollisionTests_hpp
#define CollisionTests_hpp

#include <stdio.h>
#include <entt/entt.hpp>
#include "Colliders.hpp"
#include "ContactConstraint.hpp"

namespace ImPrimitiveRendererNS { class ImPrimitiveRenderer; }

namespace GlobalDebug  {
extern std::shared_ptr<ImPrimitiveRendererNS::ImPrimitiveRenderer> imrend_global;
//extern std::function<void(const v3f&,
//                          const m4f,
//                          const int,
//                          const char*,
//                          const char*,
//                          const unsigned,
//                          const unsigned)> render_viewport_text;
//extern m4f CurrentProjViewMx;
//extern int CurrentWinHeight;
}

struct Primitive3dIntersectionDispatcher
{
    using PrimitiveIntersectionPtr = void (*)(entt::entity,
                                              entt::entity,
                                              entt::registry&,
                                              std::vector<ContactPoint3d>&,
                                              int);
    
    static void invoke(Collider3dType typeA,
                       Collider3dType typeB,
                       entt::entity collider_entityA,
                       entt::entity collider_entityB,
                       entt::registry& registry,
                       std::vector<ContactPoint3d>& contacts);
};

struct Primitive2dIntersectionDispatcher
{
    using PrimitiveIntersectionPtr = void (*)(entt::entity,
                                              entt::entity,
                                              entt::registry&,
                                              std::vector<ContactPoint2d>&,
                                              int);
    
    static void invoke(Collider2dType collider_typeA,
                       Collider2dType collider_typeB,
                       entt::entity collider_entityA,
                       entt::entity collider_entityB,
                       entt::registry& registry,
                       std::vector<ContactPoint2d>& contacts);
};

#endif /* CollisionTests_hpp */
