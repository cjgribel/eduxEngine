//
//  CollisionSystemTypes.h
//  oengine
//
//  Created by Carl Johan Gribel on 2023-08-20.
//  Copyright © 2023 Carl Johan Gribel. All rights reserved.
//

#ifndef CollisionSystemTypes_h
#define CollisionSystemTypes_h

//#include "Colliders.hpp"
#include "CoreComponents.hpp"

template<int D>
struct ContactPoint;

using ContactPoint3d = ContactPoint<3>;
using ContactPoint2d = ContactPoint<2>;
//class ContactPoint2d;
//class ContactPoint3d;

// These function pointers & callback components should be placed better

// Can't template a function pointer... (ContactPoint[3d|2d])
// Define and dispatch these ones in the derived class?
//
using ColliderPair3dCallback = void (*)(const ColliderEntityPair&   collider_pair,
                                        ContactPoint<3>*            first_contact,
                                        size_t                      nbr_contacts,
                                        entt::registry&             registry,
                                        entt::dispatcher&           dispatcher);

using ColliderPair2dCallback = void (*)(const ColliderEntityPair&   collider_pair,
                                        ContactPoint<2>*            first_contact,
                                        size_t                      nbr_contacts,
                                        entt::registry&             registry,
                                        entt::dispatcher&           dispatcher);

using EntityPair_Callback = void (*)(const UnorderedEntityPairType& entity_pair,
                                     entt::registry&                registry,
                                     entt::dispatcher&              dispatcher);

template<int> struct TriggeredColliderPair_EnterCallback { };
template<>      struct TriggeredColliderPair_EnterCallback<3> { ColliderPair3dCallback callback; uint callback_id; };
template<>      struct TriggeredColliderPair_EnterCallback<2> { ColliderPair2dCallback callback; uint callback_id;};
using TriggeredCollider3dPair_EnterCallback = TriggeredColliderPair_EnterCallback<3>;
using TriggeredCollider2dPair_EnterCallback = TriggeredColliderPair_EnterCallback<2>;

template<int> struct TriggeredColliderPair_StayCallback { };
template<>      struct TriggeredColliderPair_StayCallback<3> { ColliderPair3dCallback callback; uint callback_id; };
template<>      struct TriggeredColliderPair_StayCallback<2> { ColliderPair2dCallback callback; uint callback_id; };
using TriggeredCollider3dPair_StayCallback = TriggeredColliderPair_StayCallback<3>;
using TriggeredCollider2dPair_StayCallback = TriggeredColliderPair_StayCallback<2>;

template<int> struct TriggeredColliderPair_ExitCallback { };
template<>      struct TriggeredColliderPair_ExitCallback<3> { ColliderPair3dCallback callback; uint callback_id; };
template<>      struct TriggeredColliderPair_ExitCallback<2> { ColliderPair2dCallback callback; uint callback_id; };
using TriggeredCollider3dPair_ExitCallback = TriggeredColliderPair_ExitCallback<3>;
using TriggeredCollider2dPair_ExitCallback = TriggeredColliderPair_ExitCallback<2>;

//struct TriggeredColliderPair_EnterCallback { ColliderPairCallback callback; };
//struct TriggeredColliderPair_StayCallback { ColliderPairCallback callback; };
//struct TriggeredColliderPair_ExitCallback { ColliderPairCallback callback; };

struct TriggeredEntityPair_EnterCallback { EntityPair_Callback callback; uint callback_id; };
struct TriggeredEntityPair_StayCallback { EntityPair_Callback callback; uint callback_id; };
struct TriggeredEntityPair_ExitCallback { EntityPair_Callback callback; uint callback_id; };

#endif /* CollisionSystemTypes_h */
