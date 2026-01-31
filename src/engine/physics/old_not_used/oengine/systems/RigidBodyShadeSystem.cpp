//
//  RigidBodyShadeSystem.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-10.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "RigidBodyShadeSystem.hpp"
#include "Scene.hpp"
#include "RigidBody.hpp"
#include "CoreComponents.hpp"
#include "SceneAPI.hpp"

using namespace RigidBody;

template<class RigidBodyType>
void RigidBodyShadeSystem_<RigidBodyType>::update(float dt,
                                                  Scene& scene)
{
    auto& registry = scene.registry;
    auto& resources = scene.resources;
    
    // RB driven by Bone
    auto rb_entity_view = registry.view<RigidBodyType, BoneSocket>();
    
    for(auto rb_entity : rb_entity_view)
    {
        auto& rb        = rb_entity_view.template get<RigidBodyType>(rb_entity);
        auto& socket    = rb_entity_view.template get<BoneSocket>(rb_entity);
        if (!socket.isActive || socket.index == -1) continue;
        
        assert(socket.primary_entity);
        auto& bone_pe_tfm = registry.get<Transform>(socket.primary_entity.entity).global_tfm;

        m4f Bone, Bib;
        bool res = SkeletonOp::Get_Bone_Transforms(socket.primary_entity, socket.index, scene, Bone, Bib);
        assert(res);
        
        const m4f M_rb = bone_pe_tfm * Bone * socket.D;
        
        RigidBody::set_X(rb, extract_translation(M_rb));
        RigidBody::set_R(rb, M_rb.get_3x3());
        RigidBody::set_V(rb, v3f_000);
        RigidBody::set_W(rb, v3f_000);
        // TODO: velocities via differentiation
        //                RigidBody::set_VW_by_differentiation(rb, dt);
    }
}

template<class RigidBodyType>
void RigidBodyShadeSystem_<RigidBodyType>::late_update(float dt,
                                                       Scene& scene,
                                                       bool editor_mode)
{
    // RB driven by PE --> RBShadeSystem ("late" update) ???
    if (editor_mode)
    {
        auto rb_entity_view = scene.registry.view<RigidBodyType, PrimarySocket>();
        for (auto rb_entity : rb_entity_view)
        {
            auto& rb = rb_entity_view.template get<RigidBodyType>(rb_entity);
            auto& primary_socket = rb_entity_view.template get<PrimarySocket>(rb_entity);
            if (!primary_socket.isActive) continue;
            
            auto& primary_entity =  scene.registry.get<PrimaryEntity>(rb_entity);
            auto& tfm =             scene.registry.get<Transform>(primary_entity.entity);
//            auto& tfm =             scene.registry.get<Transform>(primary_socket.primary_entity.entity); // equivalent
            
            m4f G = tfm.global_tfm * primary_socket.D;
            RigidBody::set_X(rb, extract_translation(G));
            RigidBody::set_R(rb, G.get_3x3() * m3f::scaling(extract_scalinginv(G)));
        }
    }
}

template
class RigidBodyShadeSystem_<RigidBody3dComponent>;

template
class RigidBodyShadeSystem_<RigidBody2dComponent>;
