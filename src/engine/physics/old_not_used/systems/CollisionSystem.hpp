//
//  CollisionSystem.hpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-08-23.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef CollisionSystem_hpp
#define CollisionSystem_hpp

#include <entt/entt.hpp>
#include <unordered_map>
#include <unordered_set>

#include "CollisionSystemTypes.h"
#include "ray.h"
#include "CoreComponents.hpp"
#include "Colliders.hpp"
#include "RigidBody.hpp"
#include "ContactConstraint.hpp"

// TODO: Where?
#include "ImPrimitiveRenderer.hpp"
using namespace ImPrimitiveRendererNS;

using namespace RigidBody;

// MARK: Global std::shared_ptr<CollisionSystem>
// TODO: Move to e.g. Scene
template<int ContactPointType>
class CollisionSystem;

extern std::shared_ptr<CollisionSystem<3>> gCollision3dSystem;
extern std::shared_ptr<CollisionSystem<2>> gCollision2dSystem;

// CONTACT SOLVER

struct RayContact
{
    Ray ray;
    uint32_t collider_entityu = 0;
    uint32_t rb_entityu = 0;
    v3f rb_r;
    uint layer;
    // TODO: Is there a better way to express whether this (3D) ray points to 3D or 2D collider/RB?
    bool is2d = false;
    
    RayContact() {}
    explicit RayContact(const Ray& ray,
                        uint layer = 0)
    : ray(ray), layer(layer) { }
  
    inline bool isColliderContact() const { return (bool)collider_entityu && !is2d; }
    inline bool isRigidBodyColliderContact() const { return (bool)rb_entityu && !is2d; }

    inline bool isCollider2dContact() const { return (bool)collider_entityu && is2d; }
    inline bool isRigidBody2dColliderContact() const { return (bool)rb_entityu && is2d; }
    
    inline v3f point_of_contact() const { return ray.point_of_contact(); }
};

struct RayContact2d
{
    Ray2d ray;
    uint32_t collider_entityu = 0;
    uint32_t rb_entityu = 0;
    v2f rb_r;
    uint layer;
    
    RayContact2d() {}
    explicit RayContact2d(const Ray2d& ray,
                        uint layer = 0)
    : ray(ray), layer(layer) { }
  
    inline bool isColliderContact() const { return (bool)collider_entityu; }
    inline bool isRigidBodyColliderContact() const { return (bool)rb_entityu; }
    
    inline v2f point_of_contact() const { return ray.point_of_contact(); }
};

template<class Map, class Key>
inline bool contains_key(const Map& map,
                         const Key& key)
{
    return map.count(key) > 0;
}

template<class ContainerType>
class DoubleBufferedContainer
{
public:
    std::unique_ptr<ContainerType> back;
    std::unique_ptr<ContainerType> front;
    
    DoubleBufferedContainer() :
    back(std::make_unique<ContainerType>()),
    front(std::make_unique<ContainerType>()) { }
    
    void reserve(size_t size)
    {
        back->reserve(size);
        front->reserve(size);
    }
    
    void swap_buffers() { back.swap(front); }
    
    void clear_front() { front->clear(); }
};

//// These function pointers & callback components should be placed better
//
//// Can't template a function pointer... (ContactPoint[3d|2d])
//// Define and dispatch these ones in the derived class?
////
//using ColliderPair3dCallback = void (*)(const ColliderEntityPair&   collider_pair,
//                                        ContactPoint3d*             first_contact,
//                                        size_t                      nbr_contacts,
//                                        entt::registry&             registry,
//                                        entt::dispatcher&           dispatcher);
//
//using ColliderPair2dCallback = void (*)(const ColliderEntityPair&   collider_pair,
//                                        ContactPoint2d*             first_contact,
//                                        size_t                      nbr_contacts,
//                                        entt::registry&             registry,
//                                        entt::dispatcher&           dispatcher);
//
//using EntityPair_Callback = void (*)(const UnorderedEntityPairType& entity_pair,
//                                     entt::registry&                registry,
//                                     entt::dispatcher&              dispatcher);
//
//template<class> struct TriggeredColliderPair_EnterCallback { };
//template<>      struct TriggeredColliderPair_EnterCallback<ContactPoint3d> { ColliderPair3dCallback callback; uint callback_id; };
//template<>      struct TriggeredColliderPair_EnterCallback<ContactPoint2d> { ColliderPair2dCallback callback; uint callback_id;};
//using TriggeredCollider3dPair_EnterCallback = TriggeredColliderPair_EnterCallback<ContactPoint3d>;
//using TriggeredCollider2dPair_EnterCallback = TriggeredColliderPair_EnterCallback<ContactPoint2d>;
//
//template<class> struct TriggeredColliderPair_StayCallback { };
//template<>      struct TriggeredColliderPair_StayCallback<ContactPoint3d> { ColliderPair3dCallback callback; uint callback_id; };
//template<>      struct TriggeredColliderPair_StayCallback<ContactPoint2d> { ColliderPair2dCallback callback; uint callback_id; };
//using TriggeredCollider3dPair_StayCallback = TriggeredColliderPair_StayCallback<ContactPoint3d>;
//using TriggeredCollider2dPair_StayCallback = TriggeredColliderPair_StayCallback<ContactPoint2d>;
//
//template<class> struct TriggeredColliderPair_ExitCallback { };
//template<>      struct TriggeredColliderPair_ExitCallback<ContactPoint3d> { ColliderPair3dCallback callback; uint callback_id; };
//template<>      struct TriggeredColliderPair_ExitCallback<ContactPoint2d> { ColliderPair2dCallback callback; uint callback_id; };
//using TriggeredCollider3dPair_ExitCallback = TriggeredColliderPair_ExitCallback<ContactPoint3d>;
//using TriggeredCollider2dPair_ExitCallback = TriggeredColliderPair_ExitCallback<ContactPoint2d>;
//
////struct TriggeredColliderPair_EnterCallback { ColliderPairCallback callback; };
////struct TriggeredColliderPair_StayCallback { ColliderPairCallback callback; };
////struct TriggeredColliderPair_ExitCallback { ColliderPairCallback callback; };
//
//struct TriggeredEntityPair_EnterCallback { EntityPair_Callback callback; uint callback_id; };
//struct TriggeredEntityPair_StayCallback { EntityPair_Callback callback; uint callback_id; };
//struct TriggeredEntityPair_ExitCallback { EntityPair_Callback callback; uint callback_id; };

// ContactPoint
// RigidBodyComponent;
// RigidBodyEntityPair aaa;
// RigidBodyColliderEntityPair

template<int D>
class CollisionSystem
{
public:
    
    struct ContactIndexRange
    {
        size_t start, count;
    };
    
    using ContactPointType = ContactPoint<D>;
    
private:
    DoubleBufferedContainer<std::vector<ContactPointType>>
    contacts;

    // 0) CANDIDATE RB-COLLIDER PAIRS - (not here) used by broad phase CH algorithm
    
    // 1) RB-COLLIDER PAIRS - For contact solver
    DoubleBufferedContainer<std::unordered_map<RigidBodyColliderEntityPair, ContactIndexRange>>
    rb_collider_pairs;

#if 0
    // REMOVE?
    // 2) RB PAIRS - for what?
    DoubleBufferedContainer<std::unordered_set<RigidBodyEntityPair>>
    rb_pairs;
#endif
    
    // 3) COLLIDER PAIRS = one is a TRIGGER (RB's ignored)
    // For collider-collider events
    DoubleBufferedContainer<std::unordered_map<ColliderEntityPair, ContactIndexRange>>
    collider_pairs_triggered;
    
#if 0
    // REMOVE?
    // 3) For events
    // Same as 2) + one or both colliders is a Trigger
    DoubleBufferedContainer<std::unordered_set<RigidBodyEntityPair>>
    rb_pairs_triggered;
#endif
    
    // ADDED
    // Entity-Entity events
    DoubleBufferedContainer<std::unordered_set<UnorderedEntityPairType>>
    entity_pairs_triggered;
    
protected:
    // For debugging
    unsigned nbr_warm_contacts;
    unsigned nbr_narrow_phase_tests;
    std::vector<RayContact> ray_cache;
    std::vector<RayContact2d> ray2d_cache;
    
public:
    CollisionSystem();
    
    // Expose for debugging purposes
    
    size_t nbr_front_contacts() { return contacts.front->size(); }
    size_t front_contacts_capacity() { return contacts.front->capacity(); }
    unsigned nbr_warm_contact() { return nbr_warm_contacts; }
    unsigned nbr_narrow_test() { return nbr_narrow_phase_tests; }
    
    size_t nbr_rb_collider_pairs() { return rb_collider_pairs.front->size(); }
    size_t nbr_collider_pairs_triggered() { return collider_pairs_triggered.front->size(); }
    size_t nbr_entity_pairs_triggered() { return entity_pairs_triggered.front->size(); }
    
    void next_frame();
    
    virtual void detect_collisions(float dt,
                                   entt::registry& registry) = 0;
    
    virtual void raycast(RayContact& ray_contact,
                         entt::registry& registry) = 0;
    
protected:
    
    /**
     Add contact points to the central buffer, mapped by collider pair.
     */
    // Note:
    // Note: Declaration & defintion for an inlined function can be separated,
    // but the definiton must then appear in every TU where the function is used.
    inline void register_contacts(typename std::vector<ContactPointType>::iterator first_contact,
                                  typename std::vector<ContactPointType>::iterator last_contact,
                                  bool colliderA_is_trigger,
                                  bool colliderB_is_trigger,
                                  const ColliderEntityPair& collider_pair,
                                  const RigidBodyEntityPair& rb_pair,
                                  const UnorderedEntityPairType& entity_pair)
    {
        size_t start = contacts.front->size();
        size_t count = static_cast<size_t>(std::distance(first_contact, last_contact));
        ContactIndexRange range {start, count };
        
        // Apend contacts to central storage
        contacts.front->insert(contacts.front->end(),
                               first_contact,
                               last_contact);
        // MOVE version. Should not matter if ContactPoint is simple enough (POD even)
        //        contacts_cur->insert(contacts_cur->end(),
        //                             std::make_move_iterator(first_contact),
        //                             std::make_move_iterator(last_contact));
        
        // Both have RB's => to be solved by contact solver
        const bool both_have_RB = rb_pair.pair.first != entt::null && rb_pair.pair.second != entt::null;
        
        // Any collider is a trigger (both may be triggers) => may issue events
        const bool any_is_trigger = colliderA_is_trigger || colliderB_is_trigger;
        
        if (both_have_RB)
        {
            // UNIQUE
            const RigidBodyColliderEntityPair rbcollider_pair {collider_pair.pair, rb_pair.pair};
//            const RigidBodyColliderEntityPair rbcollider_pair = MakeRigidBodyColliderEntityPair(collider_pair.pair, rb_pair.pair);
            // This pair should not have been registered previously
            assert( !contains_key(*rb_collider_pairs.front, rbcollider_pair) );
            rb_collider_pairs.front->insert( { rbcollider_pair, range} );
            
#if 0
            // NOT NEEDED?
            // NOT UNIQUE - the same key may occur multiple times if
            // multiple colliders attached to RB pair
            rb_pairs.front->insert(rb_pair);
#endif
        }
        
        // TODO: Remove?
        // Triggered collider pairs: if either collider is a trigger (RB's ignored)
        // UNIQUE
        if (any_is_trigger)
        {
            // CD system should submit contacts just once per pair
            assert( !contains_key(*collider_pairs_triggered.front, collider_pair) );
            collider_pairs_triggered.front->insert( {collider_pair, range } );
            //            triggered_collider_pairs[collider_pair] = range; // get or create
        }
        
#if 0
        // TODO: Remove?
        // Triggered RB pairs: if both collider have RB's & either is a trigger
        // NOT UNIQUE
        if (any_is_trigger && both_have_RB)
            rb_pairs_triggered.front->insert(rb_pair); // get or create
#endif
        
        // Hash parent entity pair of collider pair where at least one is a trigger
        // This may happen multiple times of parent entities has multiple colliders
        // if any collider is trigger AND both have Parent Entity
        //      triggered_entity_pairs (hash parent entities)
        if (any_is_trigger)
            entity_pairs_triggered.front->insert(entity_pair); // get or create
    }
    
public:

    void pre_solve(float dt,
                   entt::registry& registry);
    
    // Cannot be inlined (as long as the definition is in the cpp), since this
    // function is called from another TU.
    inline void solve(float dt,
                      entt::registry& registry)
    {
        for (auto& elem : *rb_collider_pairs.front)
        {
            auto rb_entity_pair = elem.first.rb_pair;
            for (int i = 0; i < elem.second.count; i++)
                contacts.front->at(elem.second.start + i).solve(rb_entity_pair,
                                                                registry,
                                                                dt);
        }
    }
    
    void post_solve(float dt,
                    entt::registry& registry);
    
    // (PUBLIC METHOD)
    // - This class should be part of ENGINE...but this method needs to know about GAME-specific events
    // - Allow events to hold temporary pointers to contacts (contacts.front + range.start)?
    //
    // Available data which can be included in events:
    //      Colliders
    //      RB's, parent entities (linked to by colliders)
    //      Contact points (in some cases)

    void dispatch_collision_events(entt::registry& registry,
                                   entt::dispatcher& dispatcher);
        
private:
    
#if defined(XI_DEBUG)
#define StickyNoteEntityCollisions
#endif

#if defined(XI_DEBUG)
#define StickyNoteColliderCollisions
#endif
    
    // TODO: Consider how to organize these Do[...] methods
    // These are merely staging calls where callbacks are fetched & called
    // from different types of entities
    // TODO: Why can dispatch_collision_events only submit const pairs???
    
    void OnColliderTriggerEnter(const ColliderEntityPair& collider_pair,
                                ContactPointType* first_contact,
                                size_t nbr_contacts,
                                entt::registry& registry,
                                entt::dispatcher& dispatcher);
    
    void OnColliderTriggerStay(const ColliderEntityPair& collider_pair,
                               ContactPointType* first_contact,
                               size_t nbr_contacts,
                               entt::registry& registry,
                               entt::dispatcher& dispatcher);
    
    void OnColliderTriggerExit(const ColliderEntityPair& collider_pair,
                               ContactPointType* first_contact,
                               size_t nbr_contacts,
                               entt::registry& registry,
                               entt::dispatcher& dispatcher);
    
    // MARK: Added, but the role of these methods have changed and they may be superfluous
    // These events may involve multiple colliders
    void OnEntityPairTriggerEnter(const UnorderedEntityPairType& pair,
                                  entt::registry& registry,
                                  entt::dispatcher& dispatcher);
    
    void OnEntityPairTriggerStay(const UnorderedEntityPairType& pair,
                                 entt::registry& registry,
                                 entt::dispatcher& dispatcher);
    
    void OnEntityPairTriggerExit(const UnorderedEntityPairType& pair,
                                 entt::registry& registry,
                                 entt::dispatcher& dispatcher);
    
public:
    void debug_render_contacts(ImPrimitiveRenderer* imrend,
                               bool render_rb_collider_pairs,
                               bool render_triggered_collider_pairs,
                               bool render_cached_rays);
};

using Collision3dSystem = CollisionSystem<3>;
using Collision2dSystem = CollisionSystem<2>;

#endif /* CollisionSystem_hpp */
