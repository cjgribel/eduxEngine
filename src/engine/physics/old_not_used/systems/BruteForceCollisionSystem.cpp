//
//  BruteForceCollisionSystem.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-12.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "BruteForceCollisionSystem.hpp"
#include "CollisionTests.hpp"
#include "RayTests.hpp"

std::shared_ptr<Collision3dSystem> gCollision3dSystem =
std::make_shared<BruteForceCollision3dSystem>();

std::shared_ptr<Collision2dSystem> gCollision2dSystem =
std::make_shared<BruteForceCollision2dSystem>();

void BruteForceCollision3dSystem::detect_collisions(float dt,
                                                  entt::registry& registry)
{
    // Temporary contacts
    static std::vector<ContactPoint3d> contacts;
    //    static std::vector<RigidBodyEntityPair> contact_rb_pairs;
    //    static std::vector<ColliderEntityPair> contact_collider_pairs;
    
    // TODO: Use tag<"ColliderEntity"> for view ?
    // If RigidBodyEntity & ParentEntity are optional, this view can be single component. May
    // make the view faster; although requires try_get
    auto view = registry.view<Base3dCollider, RigidBody3dEntity>();
    
    for(auto entityA = view.begin(); entityA != view.end(); ++entityA)
    {
        const auto& basecolliderA = view.get<Base3dCollider>(*entityA);
        const auto& rbA = view.get<RigidBody3dEntity>(*entityA); // can be entt::null
        
        auto next = entityA; ++next; // can't do entityA+1
        for(auto entityB = next; entityB != view.end(); ++entityB)
        {
            const auto& basecolliderB = view.get<Base3dCollider>(*entityB);
            const auto& rbB = view.get<RigidBody3dEntity>(*entityB);
            
            // MARK: Prune collisions ...
            
            // TODO: Layer test
            if (!CollisionLayerMask::check(basecolliderA.layer,
                                           basecolliderB.layer)) continue;
            
            // TODO: Both colliders are triggers (Unity-style) (ALLOW THIS)
            
            if (rbA.entity != entt::null && rbB.entity != entt::null)
            {
                // Colliders linked to the same RB
                if (rbA.entity == rbB.entity) continue;
                
                // Both RB's are static (requires indirection)
                if (registry.get<RigidBody3dComponent>(rbA.entity).is_static &&
                    registry.get<RigidBody3dComponent>(rbB.entity).is_static)
                    continue;
            }
            
            // - Not both colliders has RB's + none is a trigger ???
            // What does this case mean?
            
            // - AABB test
            if (!basecolliderA.aabb_w.intersect(basecolliderB.aabb_w)) continue;
            
            // Perform narrow phase test
            contacts.clear();
            
            // TODO: Temporary – flip bodies/colliders once in a while to test persistency
//            if (true) {
            if (counter % 60) {
                //            if (counter > 60*2) {
                Primitive3dIntersectionDispatcher::invoke(basecolliderA.type,
                                                          basecolliderB.type,
                                                          *entityA,
                                                          *entityB,
                                                          registry,
                                                          contacts);
                
                if (contacts.size())
                {
                    const RigidBodyEntityPair rb_pair {rbA.entity, rbB.entity};
                    const ColliderEntityPair collider_pair {{*entityA, *entityB}};
                    UnorderedEntityPairType entity_pair {};
                    // Obtain parent entities if any collider is a trigger
                    if (basecolliderA.is_trigger || basecolliderB.is_trigger)
                    {
                        entity_pair.first = registry.get<PrimaryEntity>(*entityA).entity;
                        entity_pair.second = registry.get<PrimaryEntity>(*entityB).entity;
                    }
                    register_contacts(contacts.begin(),
                                      contacts.end(),
                                      basecolliderA.is_trigger,
                                      basecolliderB.is_trigger,
                                      collider_pair,
                                      rb_pair,
                                      entity_pair);
                }
            } else {
                Primitive3dIntersectionDispatcher::invoke(basecolliderB.type,
                                                          basecolliderA.type,
                                                          *entityB,
                                                          *entityA,
                                                          registry,
                                                          contacts);
                
                if (contacts.size())
                {
                    const RigidBodyEntityPair rb_pair {rbB.entity, rbA.entity};
                    const ColliderEntityPair collider_pair {{*entityB, *entityA}};
                    UnorderedEntityPairType entity_pair {};
                    // Obtain parent entities if any collider is a trigger
                    if (basecolliderA.is_trigger || basecolliderB.is_trigger)
                    {
                        entity_pair.first = registry.get<PrimaryEntity>(*entityB).entity;
                        entity_pair.second = registry.get<PrimaryEntity>(*entityA).entity;
                    }
                    register_contacts(contacts.begin(),
                                      contacts.end(),
                                      basecolliderB.is_trigger,
                                      basecolliderA.is_trigger,
                                      collider_pair,
                                      rb_pair,
                                      entity_pair);
                }
            }
            
            nbr_narrow_phase_tests++;
        }
    }
    counter++; // DEV
//    std::cout << counter << std::endl;
}

void BruteForceCollision3dSystem::raycast(RayContact& ray_contact,
                                          entt::registry& registry) // TODO: why registry here???
{
    // Conceptually: iterate over COLLIDER ENTITIES
    // It's up to the caller to dig out RB's and other stuff
    
    auto view = registry.view<Base3dCollider, RigidBody3dEntity>();
    
    // Brute force = iterate all colliders
    for(auto entity: view)
    {
        const auto& collider = view.get<Base3dCollider>(entity);
        uint32_t entityu = static_cast<std::underlying_type_t<entt::entity>>(entity);
        
        // Collision layer
        if (!CollisionLayerMask::check(ray_contact.layer,
                                       collider.layer)) continue;

        // AABB pruning
        float t_min {};
        if(!RayAABBIntersection(ray_contact.ray,
                                collider.aabb_w,
                                t_min)) continue;
        if (t_min > ray_contact.ray.z_near) continue;
        
        if (Ray3dPrimitive3dIntersectionDispatcher::invoke(collider.type,
                                                           ray_contact.ray,
                                                           entity,
                                                           registry))
        {
            // Set Collider entity
            ray_contact.collider_entityu = entityu;
            
            // Set RB entity and local hit point
            
            // TODO: Should computation of RB & r be here?
            // The collider entity might not be linked to a RB -
            // we can test this, but maybe it should be up to the caller
            // FOR NOW: check for an RB and calculate r
            
            // NOTE: The RigidBodyEntity is mandatory but might contain null
            auto& rb_entity = view.get<RigidBody3dEntity>(entity).entity;
            if (rb_entity != entt::null)
            {
                ray_contact.rb_entityu = static_cast<std::underlying_type_t<entt::entity>>(rb_entity);
                
                // hit point in world space
                v3f p_hit_w = ray_contact.ray.origin + ray_contact.ray.dir*ray_contact.ray.z_near;
                
                // Transform hit point from world to local body space using inverse of body transform
                //     body->world: T(X)*R =>
                //     body<-world: Ri*Ti * p_hit_w <=> Ri*T(-X) * p_hit_w
                auto& rb = registry.get<RigidBody3dComponent>(rb_entity);
                ray_contact.rb_r =  rb.Ri*(p_hit_w - rb.X);
            }
        }
    }
    ray_cache.push_back(ray_contact);
}

void BruteForceCollision2dSystem::detect_collisions(float dt,
                                                    entt::registry& registry)
{
//    if ((counter % 60) < 30) std::cout << "unflipped " << std::endl;
//    else std::cout << "flipped   " << std::endl;
        
    // Temporary contacts
    static std::vector<ContactPoint2d> contacts;
    //    static std::vector<RigidBodyEntityPair> contact_rb_pairs;
    //    static std::vector<ColliderEntityPair> contact_collider_pairs;
    
    // TODO: Use tag<"ColliderEntity"> for view ?
    // If RigidBodyEntity & ParentEntity are optional, this view can be single component. May
    // make the view faster; although requires try_get
    auto view = registry.view<Base2dCollider, RigidBody2dEntity>();
    
    for(auto entityA = view.begin(); entityA != view.end(); ++entityA)
    {
        const auto& basecolliderA = view.get<Base2dCollider>(*entityA);
        const auto& rbA = view.get<RigidBody2dEntity>(*entityA); // can be entt::null
        
        auto next = entityA; ++next; // can't do entityA+1
        for(auto entityB = next; entityB != view.end(); ++entityB)
        {
            const auto& basecolliderB = view.get<Base2dCollider>(*entityB);
            const auto& rbB = view.get<RigidBody2dEntity>(*entityB);
            
            // MARK: Prune collisions ...
            
            // TODO: Layer test
            if (!CollisionLayerMask::check(basecolliderA.layer,
                                           basecolliderB.layer)) continue;
            //if (!colliderA->layer_mask(colliderB->layer_bit)) continue;
            //            if (!colliderA->layer(colliderB->layer)) continue;
            
            // TODO: Both colliders are triggers (Unity-style) (ALLOW THIS)
            
            if (rbA.entity != entt::null && rbB.entity != entt::null)
            {
                // Colliders linked to the same RB
                if (rbA.entity == rbB.entity) continue;
                
                // Both RB's are static (requires indirection)
                if (registry.get<RigidBody2dComponent>(rbA.entity).is_static &&
                    registry.get<RigidBody2dComponent>(rbB.entity).is_static)
                    continue;
            }
            
            // - Not both colliders has RB's + none is a trigger ???
            // What does this case mean?
            
            // - AABB test
            if (!basecolliderA.aabb_w.intersect(basecolliderB.aabb_w)) continue;
            
            // Perform narrow phase test
            contacts.clear();
            
            // TODO: Temporary – flip bodies/colliders once in a while to test persistency
//            if (true)
            if ((counter % 60) < 30)
//            if (counter < 120)
            {
                Primitive2dIntersectionDispatcher::invoke(basecolliderA.type,
                                                          basecolliderB.type,
                                                          *entityA,
                                                          *entityB,
                                                          registry,
                                                          contacts);
                
                if (contacts.size())
                {
                    const RigidBodyEntityPair rb_pair {rbA.entity, rbB.entity};
                    const ColliderEntityPair collider_pair {{*entityA, *entityB}};
                    UnorderedEntityPairType entity_pair {};
                    // Obtain parent entities if any collider is a trigger
                    if (basecolliderA.is_trigger || basecolliderB.is_trigger)
                    {
                        entity_pair.first = registry.get<PrimaryEntity>(*entityA).entity;
                        entity_pair.second = registry.get<PrimaryEntity>(*entityB).entity;
                    }
                    register_contacts(contacts.begin(),
                                      contacts.end(),
                                      basecolliderA.is_trigger,
                                      basecolliderB.is_trigger,
                                      collider_pair,
                                      rb_pair,
                                      entity_pair);
                }
            }
            else
            {
                Primitive2dIntersectionDispatcher::invoke(basecolliderB.type,
                                                          basecolliderA.type,
                                                          *entityB,
                                                          *entityA,
                                                          registry,
                                                          contacts);
                
                if (contacts.size())
                {
                    const RigidBodyEntityPair rb_pair {rbB.entity, rbA.entity};
                    const ColliderEntityPair collider_pair {{*entityB, *entityA}};
                    UnorderedEntityPairType entity_pair {};
                    // Obtain parent entities if any collider is a trigger
                    if (basecolliderA.is_trigger || basecolliderB.is_trigger)
                    {
                        entity_pair.first = registry.get<PrimaryEntity>(*entityB).entity;
                        entity_pair.second = registry.get<PrimaryEntity>(*entityA).entity;
                    }
                    register_contacts(contacts.begin(),
                                      contacts.end(),
                                      basecolliderB.is_trigger,
                                      basecolliderA.is_trigger,
                                      collider_pair,
                                      rb_pair,
                                      entity_pair);
                }
            }
            nbr_narrow_phase_tests++;
        }
    }
    counter++; // DEV
}

void BruteForceCollision2dSystem::raycast(RayContact& ray_contact,
                                          entt::registry& registry) // TODO: why registry here???
{
    auto view = registry.view<Base2dCollider, RigidBody2dEntity>();
    
    // Brute force = iterate all colliders
    for(auto entity: view)
    {
        const auto& collider = view.get<Base2dCollider>(entity);
        uint32_t entityu = static_cast<std::underlying_type_t<entt::entity>>(entity);
        
        // Collision layer
        if (!CollisionLayerMask::check(ray_contact.layer,
                                       collider.layer)) continue;

        // AABB pruning
        float t_min {};
        if(!RayAABBIntersection(ray_contact.ray,
                                toAABB3d(collider.aabb_w),
                                t_min)) continue;
        if (t_min > ray_contact.ray.z_near) continue;
        
        if (Ray3dPrimitive2dIntersectionDispatcher::invoke(collider.type,
                                                           ray_contact.ray,
                                                           entity,
                                                           registry))
        {
            // Set Collider entity
            ray_contact.collider_entityu = entityu;
            ray_contact.is2d = true;
            
            // Set RB entity and local hit point
            
            // TODO: Should computation of RB & r be here?
            // The collider entity might not be linked to a RB -
            // we can test this, but maybe it should be up to the caller
            // FOR NOW: check for an RB and calculate r
            
            // NOTE: The RigidBodyEntity is mandatory but might contain null
            auto& rb_entity = view.get<RigidBody2dEntity>(entity).entity;
            if (rb_entity != entt::null)
            {
                ray_contact.rb_entityu = static_cast<std::underlying_type_t<entt::entity>>(rb_entity);
                
                // hit point in world space
                v2f p_hit_w = xy(ray_contact.ray.origin + ray_contact.ray.dir*ray_contact.ray.z_near);
                
                // Transform hit point from world to local body space using inverse of body transform
                //     body->world: T(X)*R =>
                //     body<-world: Ri*Ti * p_hit_w <=> Ri*T(-X) * p_hit_w
                auto& rb = registry.get<RigidBody2dComponent>(rb_entity);
                ray_contact.rb_r = xy0(transpose(m2f::rotation(rb.R)) * (p_hit_w - rb.X)); //  rb.Ri*(p_hit_w - rb.X);
            }
        }
    }
    ray_cache.push_back(ray_contact);
}

void BruteForceCollision2dSystem::raycast2d(RayContact2d& ray_contact,
                                            entt::registry& registry)
{
    auto view = registry.view<Base2dCollider, RigidBody2dEntity>();
    
    // Brute force = iterate all colliders
    for(auto entity: view)
    {
        const auto& collider = view.get<Base2dCollider>(entity);
        uint32_t entityu = static_cast<std::underlying_type_t<entt::entity>>(entity);
        
        // Collision layer
        if (!CollisionLayerMask::check(ray_contact.layer,
                                       collider.layer)) continue;

        // AABB pruning
        float t_min {};
        Ray ray3d { xy0(ray_contact.ray.origin), xy0(ray_contact.ray.dir) }; // TEMP
        if(!RayAABBIntersection(ray3d, //ray_contact.ray,
                                toAABB3d(collider.aabb_w),
                                t_min)) continue;
        if (t_min > ray_contact.ray.z_near) continue;
        
        if (Ray2dPrimitive2dIntersectionDispatcher::invoke(collider.type,
                                                           ray_contact.ray,
                                                           entity,
                                                           registry))
        {
            // Set Collider entity
            ray_contact.collider_entityu = entityu;
//            ray_contact.is2d = true;
            
            // Set RB entity and local hit point
            
            // TODO: Should computation of RB & r be here?
            // The collider entity might not be linked to a RB -
            // we can test this, but maybe it should be up to the caller
            // FOR NOW: check for an RB and calculate r
            
            // NOTE: The RigidBodyEntity is mandatory but might contain null
            auto& rb_entity = view.get<RigidBody2dEntity>(entity).entity;
            if (rb_entity != entt::null)
            {
                ray_contact.rb_entityu = static_cast<std::underlying_type_t<entt::entity>>(rb_entity);
                
                // hit point in world space
                v2f p_hit_w = ray_contact.ray.origin + ray_contact.ray.dir*ray_contact.ray.z_near;
                
                // Transform hit point from world to local body space using inverse of body transform
                //     body->world: T(X)*R =>
                //     body<-world: Ri*Ti * p_hit_w <=> Ri*T(-X) * p_hit_w
                auto& rb = registry.get<RigidBody2dComponent>(rb_entity);
                ray_contact.rb_r = transpose(m2f::rotation(rb.R)) * (p_hit_w - rb.X); //  rb.Ri*(p_hit_w - rb.X);
            }
        }
    }
    ray2d_cache.push_back(ray_contact);
}
