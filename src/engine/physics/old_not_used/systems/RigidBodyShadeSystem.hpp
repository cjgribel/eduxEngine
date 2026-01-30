//
//  RigidBodyShadeSystem.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-10.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef RigidBodyShadeSystem_hpp
#define RigidBodyShadeSystem_hpp

#include <entt/entt.hpp>
class Scene;

#if 0
class RigidBodyShadeSystem
{
public:
    static void update(float dt,
                       entt::registry& registry);
};
#endif

// DRAFT

template<class RigidBodyType>
class RigidBodyShadeSystem_
{
public:
    static void update(float dt,
                       Scene& scene);
    
    static void late_update(float dt,
                            Scene& scene,
                            bool editor_mode);
};

#endif /* RigidBodyShadeSystem_hpp */
