//
//  CollisionSystem.cpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-08-23.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "CollisionSystem.hpp"
#include <algorithm> /* std::max */

// Used when debug rendering contacts
#include "interp.h"

template<int D>
CollisionSystem<D>::CollisionSystem()
{
    contacts.reserve(1024);
    
    rb_collider_pairs.reserve(1024);
    //    rb_pairs.reserve(512);
    collider_pairs_triggered.reserve(256);
    //    rb_pairs_triggered.reserve(256);
}

template<int D>
void CollisionSystem<D>::CollisionSystem::next_frame()
{
    contacts.swap_buffers();
    // TODO: clear() is O(1) if elements are std::is_trivially_destructible
    contacts.clear_front();
    
    nbr_warm_contacts = 0;
    nbr_narrow_phase_tests = 0;
    
    rb_collider_pairs.swap_buffers();
    //    rb_pairs.swap_buffers();
    collider_pairs_triggered.swap_buffers();
    //    rb_pairs_triggered.swap_buffers();
    entity_pairs_triggered.swap_buffers();
    
    rb_collider_pairs.clear_front();
    //    rb_pairs.clear_front();
    collider_pairs_triggered.clear_front();
    //    rb_pairs_triggered.clear_front();
    entity_pairs_triggered.clear_front();
    
    ray_cache.clear();
    ray2d_cache.clear();
}

template<int D>
void CollisionSystem<D>::pre_solve(float dt,
                                                  entt::registry& registry)
{
    // Initialize & warm start
    //
    for (auto& elem : *rb_collider_pairs.front)
    {
        RigidBodyColliderEntityPair rbcollider_pair = elem.first;
        ContactIndexRange range_cur = elem.second;
        // For all current contacts
        for (int i = 0; i < range_cur.count; i++)
        {
            ContactPointType& cp_cur = contacts.front->at(range_cur.start + i);
            ContactPointType* cp_prev = nullptr;
#if 1
            // Look for previous RigidBodyColliderEntityPair
            auto range_prev_it = rb_collider_pairs.back->find(rbcollider_pair);
            if (range_prev_it != rb_collider_pairs.back->end())
            {
                // Look for previous contact within range of previous RigidBodyColliderEntityPair
                const ContactIndexRange& range_prev = range_prev_it->second;
                const auto cp_prev_start_it = contacts.back->begin() + range_prev.start;
                const auto cp_prev_end_it = cp_prev_start_it + range_prev.count;
                auto cp_prev_it = std::find(cp_prev_start_it,
                                            cp_prev_end_it,
                                            cp_cur);
                if (cp_prev_it != cp_prev_end_it)
                {
                    // Contact point found
                    cp_prev = &(*cp_prev_it);
                    nbr_warm_contacts++;
                }
            }
#endif
            
            // Initialze & warm start current contact
            //            auto& rbA = registry.get<RigidBody3dComponent>(elem.first.rb_pair.first);
            //            auto& rbB = registry.get<RigidBody3dComponent>(elem.first.rb_pair.second);
            //            cp_cur.pre_solve(rbA, rbB, cp_prev, dt);
            cp_cur.pre_solve(rbcollider_pair.rb_pair,
                             registry,
                             cp_prev,
                             dt);
        }
    }
}

template<int D>
void CollisionSystem<D>::post_solve(float dt,
                                                   entt::registry& registry)
{
    for (auto& elem : *rb_collider_pairs.front)
        for (int i = 0; i < elem.second.count; i++)
            contacts.front->at(elem.second.start + i).post_solve(dt);
}

template<int D>
void CollisionSystem<D>::dispatch_collision_events(entt::registry& registry,
                                                                  entt::dispatcher& dispatcher)
{
    // WE DON'T CARE ABOUT THESE W.R.T EVENTS
    //      rb_collider_pairs (access to IndexRange)
    //      rb_pairs
    
    //
    // MARK: (Keep) triggered_collider_pairs (access to IndexRange)
    //
    
    // EXIT event: IndexRange exists in BACK but not in FRONT (emit back-data)
    for (auto& elem_back : *collider_pairs_triggered.back)
    {
        const ColliderEntityPair& collider_pair = elem_back.first;
        const ContactIndexRange& range_back = elem_back.second;
        const bool pair_in_front = contains_key(*collider_pairs_triggered.front, collider_pair);
        
        if (!pair_in_front)
            OnColliderTriggerExit(collider_pair,
                                  contacts.back->data() + range_back.start,
                                  range_back.count,
                                  registry,
                                  dispatcher);
    }
    // STAY event: IndexRange exists in both BACK & FRONT (emit front-data)
    // ENTER event: IndexRange exists in FRONT but not in BACK (emit front-data)
    for (auto& elem : *collider_pairs_triggered.front)
    {
        const ColliderEntityPair& collider_pair = elem.first;
        const ContactIndexRange& range = elem.second;
        const bool pair_in_back = contains_key(*collider_pairs_triggered.back, collider_pair);
        
        if (pair_in_back)
            OnColliderTriggerStay(collider_pair,
                                  contacts.front->data() + range.start,
                                  range.count,
                                  registry,
                                  dispatcher);
        else
            OnColliderTriggerEnter(collider_pair,
                                   contacts.front->data() + range.start,
                                   range.count,
                                   registry,
                                   dispatcher);
    }
    
#if 0
    //
    // TODO: (Probably remove) triggered_rb_pairs (no direct access to IndexRange (may be multiple))
    //
    
    // EXIT event: pair exists in BACK but not in FRONT (emit back-data)
    for (auto& elem_back : *rb_pairs_triggered.back)
    {
        const RigidBodyEntityPair& rb_pair = elem_back;
        const bool pair_in_front = contains_key(*rb_pairs_triggered.front, rb_pair);
        
        if (!pair_in_front)
            OnRigidBodyTriggerExit(registry, rb_pair); // <- to GAME scope
    }
    // STAY event: IndexRange exists in both BACK & FRONT (emit front-data)
    // ENTER event: IndexRange exists in FRONT but not in BACK (emit front-data)
    for (auto& elem : *rb_pairs_triggered.front)
    {
        const RigidBodyEntityPair& rb_pair = elem;
        bool pair_in_back = contains_key(*rb_pairs_triggered.back, rb_pair);
        
        if (pair_in_back)
            OnRigidBodyTriggerStay(registry, rb_pair);  // <- to GAME scope
        else
            OnRigidBodyTriggerEnter(registry, rb_pair);  // <- to GAME scope
    }
#endif
    
    //
    // MARK: (Added) entity_pairs_triggered (no direct access to IndexRange (may be multiple))
    //
    
    // EXIT event: pair exists in BACK but not in FRONT (emit back-data)
    for (auto& elem_back : *entity_pairs_triggered.back)
    {
        const UnorderedEntityPairType& entity_pair = elem_back;
        const bool pair_in_front = contains_key(*entity_pairs_triggered.front, entity_pair);
        
        if (!pair_in_front)
            OnEntityPairTriggerExit(entity_pair,
                                    registry,
                                    dispatcher);
    }
    // STAY event: IndexRange exists in both BACK & FRONT (emit front-data)
    // ENTER event: IndexRange exists in FRONT but not in BACK (emit front-data)
    for (auto& elem : *entity_pairs_triggered.front)
    {
        const UnorderedEntityPairType& entity_pair = elem;
        const bool pair_in_back = contains_key(*entity_pairs_triggered.back, entity_pair);
        
        if (pair_in_back)
            OnEntityPairTriggerStay(entity_pair,
                                    registry,
                                    dispatcher);
        
        else
            OnEntityPairTriggerEnter(entity_pair,
                                     registry,
                                     dispatcher);
    }
    
    // TODO: remove notes
    // If {entityA, entityB} has callback function: call it with arguments (entityA, entityB)
    
    // Note that multiple collider may be involved here, and we can no longer
    // see which one is a trigger (at least one is)
    
    // A trigger collider may collide with anything in the scene,
    // so don't assume there will be a callback component available.
    // Only colliders / parent entities that are interested in *some* event
    // (not necessarily this one) are going to have callback components.
    //
    // Example:
    //      Coin entity + triggers collider entity
    //      Player with multiple non-trigger colliders - callback in Player entity
    //      Platform with non-trigger colliders - no callback
    
}

template<int D>
void CollisionSystem<D>::OnColliderTriggerEnter(const ColliderEntityPair& collider_pair,
                                                               ContactPointType* first_contact,
                                                               size_t nbr_contacts,
                                                               entt::registry& registry,
                                                               entt::dispatcher& dispatcher)
{
    const entt::entity collider_entA = collider_pair.pair.first;
    const entt::entity collider_entB = collider_pair.pair.second;
    
    // TODO: Add sticky note to COLLIDER ENTITIES in debug mode (if present)
    // Note that COLLIDER ENTITIES doesn't have to contain Transforms -
    // so modify the stick note system to no rely on Transforms.
    // Use the collider's inside_point as a location instead
    
#ifdef StickyNoteColliderCollisions
    // MARK: Sticky note to announce the event -> Colliders
    auto noteA = registry.try_get<StickyNoteComponent>(collider_entA);
    auto noteB = registry.try_get<StickyNoteComponent>(collider_entB);
    if (noteA) StickyNoteComponent_Append(*noteA, "[Collider][Enter]");
    if (noteB) StickyNoteComponent_Append(*noteB, "[Collider][Enter]");
#endif
    
    // Run callbacks on collider entities
    using Callback = TriggeredColliderPair_EnterCallback<D>;
    const auto callback_ptrA = registry.try_get<Callback>(collider_entA);
    const auto callback_ptrB = registry.try_get<Callback>(collider_entB);
    if (callback_ptrA)
        callback_ptrA->callback(collider_pair,
                                first_contact,
                                nbr_contacts,
                                registry,
                                dispatcher);
    if (callback_ptrB)
        callback_ptrB->callback({collider_pair.pair.flip_copy()},
                                first_contact,
                                nbr_contacts,
                                registry,
                                dispatcher);
    //            callback_ptrB->callback({{collider_entB, collider_entA}},
    //                                    first_contact,
    //                                    nbr_contacts,
    //                                    registry,
    //                                    dispatcher);
}

template<int D>
void CollisionSystem<D>::OnColliderTriggerStay(const ColliderEntityPair& collider_pair,
                                                              ContactPointType* first_contact,
                                                              size_t nbr_contacts,
                                                              entt::registry& registry,
                                                              entt::dispatcher& dispatcher)
{
    const entt::entity collider_entA = collider_pair.pair.first;
    const entt::entity collider_entB = collider_pair.pair.second;
    
#ifdef StickyNoteColliderCollisions
    // MARK: Sticky note to announce the event -> Colliders
    auto noteA = registry.try_get<StickyNoteComponent>(collider_entA);
    auto noteB = registry.try_get<StickyNoteComponent>(collider_entB);
    if (noteA) StickyNoteComponent_AppendStack(*noteA, "[Collider][Stay]");
    if (noteB) StickyNoteComponent_AppendStack(*noteB, "[Collider][Stay]");
#endif
    
    // Run callbacks on collider entities
    using Callback = TriggeredColliderPair_StayCallback<D>;
    const auto callback_ptrA = registry.try_get<Callback>(collider_entA);
    const auto callback_ptrB = registry.try_get<Callback>(collider_entB);
    if (callback_ptrA)
        callback_ptrA->callback(collider_pair,
                                first_contact,
                                nbr_contacts,
                                registry,
                                dispatcher);
    if (callback_ptrB)
        callback_ptrB->callback({collider_pair.pair.flip_copy()},
                                first_contact,
                                nbr_contacts,
                                registry,
                                dispatcher);
    //            callback_ptrB->callback({{collider_entB, collider_entA}},
    //                                    first_contact,
    //                                    nbr_contacts,
    //                                    registry,
    //                                    dispatcher);
}

template<int D>
void CollisionSystem<D>::OnColliderTriggerExit(const ColliderEntityPair& collider_pair,
                                                              ContactPointType* first_contact,
                                                              size_t nbr_contacts,
                                                              entt::registry& registry,
                                                              entt::dispatcher& dispatcher)
{
    const entt::entity collider_entA = collider_pair.pair.first;
    const entt::entity collider_entB = collider_pair.pair.second;
    
#ifdef StickyNoteColliderCollisions
    // MARK: Sticky note to announce the event -> Colliders
    auto noteA = registry.try_get<StickyNoteComponent>(collider_entA);
    auto noteB = registry.try_get<StickyNoteComponent>(collider_entB);
    if (noteA) StickyNoteComponent_Append(*noteA, "[Collider][Exit]");
    if (noteB) StickyNoteComponent_Append(*noteB, "[Collider][Exit]");
#endif
    
    // Run callbacks on collider entities
    using Callback = TriggeredColliderPair_ExitCallback<D>;
    const auto callback_ptrA = registry.try_get<Callback>(collider_entA);
    const auto callback_ptrB = registry.try_get<Callback>(collider_entB);
    if (callback_ptrA)
        callback_ptrA->callback(collider_pair,
                                first_contact,
                                nbr_contacts,
                                registry,
                                dispatcher);
    if (callback_ptrB)
        callback_ptrB->callback({collider_pair.pair.flip_copy()},
                                first_contact,
                                nbr_contacts,
                                registry,
                                dispatcher);
    //            callback_ptrB->callback({{collider_entB, collider_entA}},
    //                                    first_contact,
    //                                    nbr_contacts,
    //                                    registry,
    //                                    dispatcher);
}

template<int D>
void CollisionSystem<D>::OnEntityPairTriggerEnter(const UnorderedEntityPairType& pair,
                                                                 entt::registry& registry,
                                                                 entt::dispatcher& dispatcher)
{
#ifdef StickyNoteEntityCollisions
    // MARK: Sticky note to announce the event
    auto noteA = registry.try_get<StickyNoteComponent>(pair.first);
    auto noteB = registry.try_get<StickyNoteComponent>(pair.second);
    if (noteA) StickyNoteComponent_Append(*noteA, "[Entity][Enter]");
    if (noteB) StickyNoteComponent_Append(*noteB, "[Entity][Enter]");
#endif
    
    const auto& callback_ptrA = registry.try_get<TriggeredEntityPair_EnterCallback>(pair.first);
    const auto& callback_ptrB = registry.try_get<TriggeredEntityPair_EnterCallback>(pair.second);
    if (callback_ptrA) callback_ptrA->callback(pair,
                                               registry,
                                               dispatcher);
    if (callback_ptrB) callback_ptrB->callback(UnorderedEntityPairType {pair.second, pair.first},
                                               registry,
                                               dispatcher);
    //        std::cout << "OnEntityPairTriggerEnter" << std::endl;
}

template<int D>
void CollisionSystem<D>::OnEntityPairTriggerStay(const UnorderedEntityPairType& pair,
                                                                entt::registry& registry,
                                                                entt::dispatcher& dispatcher)
{
#ifdef StickyNoteEntityCollisions
    // MARK: Sticky note to announce the event
    auto noteA = registry.try_get<StickyNoteComponent>(pair.first);
    auto noteB = registry.try_get<StickyNoteComponent>(pair.second);
    if (noteA) StickyNoteComponent_AppendStack(*noteA, "[Entity][Stay]");
    if (noteB) StickyNoteComponent_AppendStack(*noteB, "[Entity][Stay]");
#endif
    
    const auto& callback_ptrA = registry.try_get<TriggeredEntityPair_StayCallback>(pair.first);
    const auto& callback_ptrB = registry.try_get<TriggeredEntityPair_StayCallback>(pair.second);
    if (callback_ptrA) callback_ptrA->callback(pair,
                                               registry,
                                               dispatcher);
    if (callback_ptrB) callback_ptrB->callback(UnorderedEntityPairType {pair.second, pair.first},
                                               registry,
                                               dispatcher);
    
    // No sticky note here - will flood the note buffer
}

template<int D>
void CollisionSystem<D>::OnEntityPairTriggerExit(const UnorderedEntityPairType& pair,
                                                                entt::registry& registry,
                                                                entt::dispatcher& dispatcher)
{
#ifdef StickyNoteEntityCollisions
    // MARK: Sticky note to announce the event
    auto noteA = registry.try_get<StickyNoteComponent>(pair.first);
    auto noteB = registry.try_get<StickyNoteComponent>(pair.second);
    if (noteA) StickyNoteComponent_Append(*noteA, "[Entity][Exit]");
    if (noteB) StickyNoteComponent_Append(*noteB, "[Entity][Exit]");
#endif
    
    const auto& callback_ptrA = registry.try_get<TriggeredEntityPair_ExitCallback>(pair.first);
    const auto& callback_ptrB = registry.try_get<TriggeredEntityPair_ExitCallback>(pair.second);
    if (callback_ptrA) callback_ptrA->callback(pair,
                                               registry,
                                               dispatcher);
    if (callback_ptrB) callback_ptrB->callback(UnorderedEntityPairType {pair.second, pair.first},
                                               registry,
                                               dispatcher);
}

namespace {
inline v3f xyz(const v3f& v)
{
    return v;
}
inline v3f xyz(const v2f& v)
{
    return xy0(v);
}
}

template<int D>
void CollisionSystem<D>::debug_render_contacts(ImPrimitiveRenderer* imrend,
                                               bool render_rb_collider_pairs,
                                               bool render_triggered_collider_pairs,
                                               bool render_cached_rays)
{
#if 0
    // Check max bucket size = max number of key collisions
    // Test: out of ~150 keys, max bucket size was 3
    size_t max_bucket_size = 0;
    for (int i = 0; i < rb_collider_pairs.front->bucket_count(); i++)
        max_bucket_size = std::max(max_bucket_size,
                                   rb_collider_pairs.front->bucket_size(i));
    
    std::cout << "rb_collider_pairs.front->size() " << rb_collider_pairs.front->size() << std::endl;
    std::cout << "rb_collider_pairs.front->bucket_count() " << rb_collider_pairs.front->bucket_count() << std::endl;
    std::cout << "max_bucket_count " << max_bucket_size << std::endl;
#endif
    
    // RB Colliders
    if (render_rb_collider_pairs)
    {
        imrend->push_states(Color4u::Lime, DepthTest::False);
        for (auto& elem : *rb_collider_pairs.front)
        {
            for (int i = 0; i < elem.second.count; i++)
            {
                const ContactPointType& cp = contacts.front->at(elem.second.start + i);
                float age_frac = clamp(cp.age/3, 0.0f, 1.0f);
                const v3f color {lerp(1.0f, 0.0f, age_frac), 0.0f, lerp(0.0f, 1.0f, age_frac)};
                
                imrend->push_states(Color4u {color}, DepthTest::False);
                imrend->push_point(xyz(cp.cp), 10);
                imrend->pop_states<Color4u, DepthTest>();
                
                //                // Tangents (friction)
                //                v3f tng0 = cp.ct0 * cp.lambda_t0_acc * 1.0f;
                //                v3f tng1 = cp.ct1 * cp.lambda_t1_acc * 1.0f;
                //                imrend->push_line(cp.cp, cp.cp+tng0, {1,0,0});
                //                imrend->push_line(cp.cp, cp.cp+tng1, {0,0,1});
                // Normal
                imrend->push_line(xyz(cp.cp), xyz(cp.cp+cp.cn*0.2f));
                //                //
                //                imrend->push_line(cp.cp, cp.cp+cp.rABdot*1.0f, {0,1,1});
            }
        }
        imrend->pop_states<Color4u, DepthTest>();
    }
    
    // Triggered colliders (RB's ignored)
    if (render_triggered_collider_pairs)
        for (auto& elem : *collider_pairs_triggered.front)
        {
            for (int i = 0; i < elem.second.count; i++)
            {
                const v3f cp = xyz(contacts.front->at(elem.second.start + i).cp);
                const v3f color {1.0f, 1.0f, 0.0f};
                
                imrend->push_states(Color4u {color}, DepthTest::False);
                imrend->push_point(cp, 10);
                imrend->pop_states<Color4u, DepthTest>();
                
            }
        }
    
    // Cached rays (3d)
    if (render_cached_rays)
    {
        imrend->push_states(DepthTest::True);
        for (auto& rayc : ray_cache)
        {
            unsigned ray_color = 0xffffffff;
            unsigned ray_normal_color = 0xff80ff80;
            v3f poc = rayc.ray.origin + rayc.ray.dir * fminf(rayc.ray.z_near, 1e5);
            
            imrend->push_states(Color4u {ray_color});
            imrend->push_line(rayc.ray.origin, poc);
            imrend->pop_states<Color4u>();
            
            imrend->push_states(Color4u {ray_normal_color});
            imrend->push_line(poc, poc + rayc.ray.n_near);
            imrend->pop_states<Color4u>();
        }
        imrend->pop_states<DepthTest>();
    }
    
    // Cached rays (2d)
    if (render_cached_rays)
    {
        imrend->push_states(DepthTest::True);
        for (auto& rayc : ray2d_cache)
        {
            const v3f ray_color {1,1,1};
            const v3f ray_normal_color {0,1,0};
            v3f poc = xy0(rayc.ray.origin + rayc.ray.dir * fminf(rayc.ray.z_near, 1e5));
            
            imrend->push_states(Color4u {ray_color});
            imrend->push_line(xy0(rayc.ray.origin), poc);
            imrend->pop_states<Color4u>();
            
            imrend->push_states(Color4u {ray_normal_color});
            imrend->push_line(poc, poc + xy0(rayc.ray.n_near));
            imrend->pop_states<Color4u>();
        }
        imrend->pop_states<DepthTest>();
    }
}

template
class CollisionSystem<3>;

template
class CollisionSystem<2>;
