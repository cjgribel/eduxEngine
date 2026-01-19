//
//  BruteForceCollisionSystem.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-12.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef BruteForceCollisionSystem_hpp
#define BruteForceCollisionSystem_hpp

#include "CollisionSystem.hpp"

class BruteForceCollision3dSystem : public CollisionSystem<3>
{
    unsigned counter = 0;
public:
    
    // Dispatcher: 3D Collider <-> 3D Collider
    void detect_collisions(float dt,
                           entt::registry& registry) override; // TODO: why registry here???

    // Dispatcher: 3D Ray -> 3D collider
    // RayContact will hold entity of intersected RB3D
    // TODO:  dispatcher -> struct + move to cpp
    void raycast(RayContact& ray_contact,
                 entt::registry& registry) override; // TODO: why registry here???
};

class BruteForceCollision2dSystem : public CollisionSystem<2>
{
    unsigned counter = 0;
public:
    
    // Dispatcher: 2D Collider <-> 2D Collider
    void detect_collisions(float dt,
                           entt::registry& registry) override; // TODO: why registry here???
    
    // TODO: 2D Ray
    // TODO: 2D RayContact (2D Ray, 2D point-of-contact) ???
    
    // TODO: Dispatcher: 3D Ray -> 2D collider
    // RayContact will hold entity of intersected RB2D
    void raycast(RayContact& ray_contact,
                 entt::registry& registry) override;
    
    // TODO: Dispatcher: 2D Ray -> 2D collider
    // TODO: 2D Ray
    // RayContact will hold entity of intersected RB2D
    void raycast2d(RayContact2d& ray_contact,
                   entt::registry& registry);
};

#endif /* BruteForceCollisionSystem_hpp */
