//
//  ColliderShadeSystem.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-10.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef ColliderShadeSystem_hpp
#define ColliderShadeSystem_hpp

#include <entt/entt.hpp>
class Scene;

#if 0
class ColliderShadeSystem
{
public:
    static void update(float dt,
                       Scene
                       entt::registry& registry);
};
#endif

class Collider2dShadeSystem
{
public:
    static void update(float dt,
                       Scene& scene);
};

class Collider3dShadeSystem
{
public:
    static void update(float dt,
                       Scene& scene);
};

#endif /* ColliderShadeSystem_hpp */
