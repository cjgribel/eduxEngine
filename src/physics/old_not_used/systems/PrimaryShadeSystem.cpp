//
//  PrimaryShadeSystem.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-10.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "PrimaryShadeSystem.hpp"
#include "RigidBody.hpp"
#include "CoreComponents.hpp"
#include "SceneAPI.hpp"
#include "Scene.hpp"

using namespace entt::literals;
using namespace RigidBody;

void PrimaryShadeSystem::update(float dt,
                                Scene& scene,
                                bool editor_mode)
{
    // PE driven by RB3d
    if (!editor_mode)
    {
        auto entity_view = scene.registry.view<EntityTypeTag<PrimaryEntity>, RigidBody3dSocket>();
        for(auto primary_entity : entity_view)
        {
            const auto& rb_socket = entity_view.get<RigidBody3dSocket>(primary_entity);
            if (!rb_socket.isActive) continue;
            const auto& rb =        scene.registry.get<RigidBody3dComponent>(rb_socket.rb_entity.entity);
            auto& tfm =             scene.registry.get<Transform>(primary_entity);

//            m4f S = m4f::scaling(extract_scaling(tfm.global_tfm));
            
            tfm.global_tfm = RigidBody::get_transform(rb) * rb_socket.D; // * S;
        }
    }
    
    // PE driven by RB2d
    if (!editor_mode)
    {
        auto entity_view = scene.registry.view<EntityTypeTag<PrimaryEntity>, RigidBody2dSocket>();
        for(auto primary_entity : entity_view)
        {
            const auto& rb_socket = entity_view.get<RigidBody2dSocket>(primary_entity);
            if (!rb_socket.isActive) continue;
            const auto& rb =        scene.registry.get<RigidBody2dComponent>(rb_socket.rb_entity.entity);
            auto& tfm =             scene.registry.get<Transform>(primary_entity);
            
            // TODO: Scaling here ???
//            m4f S = m4f::scaling(extract_scaling(tfm.global_tfm));
            
            tfm.global_tfm = RigidBody::get_transform(rb) * rb_socket.D; // * S;
        }
    }
    
    // PE driven by Bone
    {
        auto entity_view = scene.registry.view<EntityTypeTag<PrimaryEntity>, BoneSocket>();
        for(auto primary_entity : entity_view)
        {
            auto socket = scene.registry.get<BoneSocket>(primary_entity);
            
            if (socket.isActive &&
                socket.index > -1)
            {
                assert(socket.primary_entity);
                auto& primary_tfm = scene.registry.get<Transform>(primary_entity).global_tfm;
                
                auto& bone_pe_tfm = scene.registry.get<Transform>(socket.primary_entity.entity).global_tfm;
                
                m4f Bone, Bib;
                bool res = SkeletonOp::Get_Bone_Transforms(socket.primary_entity, socket.index, scene, Bone, Bib);
                assert(res);
                
                m4f Si = m4f::scaling(extract_scalinginv(bone_pe_tfm));
                primary_tfm = bone_pe_tfm * Bone * socket.D * Si * primary_tfm;
                
                // xiengine
                // return G * B * B_ib.inverse() * B_si * G_si * D; // = "RB"
            }
        }
    }
    
    // TODO: hack to introduce a second transform pass
    
#if 0
    auto view2 = registry.view< entt::tag<"PrimaryEntity2"_hs>, Handle<Transform>>();
    
    for(auto entity: view2)
    {
        auto& tfm = view2.get<Handle<Transform>>(entity);
        
        RigidBodySocketComponent* rb_socket = registry.try_get<RigidBodySocketComponent>(entity);
        if (rb_socket && rb_socket->isActive)
        {
            auto& rb = registry.get<RigidBody3dComponent>(rb_socket->rb_ent);
            tfm->global_tfm = RigidBody::get_transform(rb) * rb_socket->D * tfm->global_tfm;
        }
        
        RigidBody2dSocketComponent* rb2d_socket = registry.try_get<RigidBody2dSocketComponent>(entity);
        if (rb2d_socket && rb2d_socket->isActive)
        {
            auto& rb = registry.get<RigidBody2dComponent>(rb2d_socket->rb2d_ent);
            tfm->global_tfm = RigidBody::get_transform(rb) * rb2d_socket->D * tfm->global_tfm;
        }
        
        BoneSocketComponent* bone_socket = registry.try_get<BoneSocketComponent>(entity);
        if (bone_socket && bone_socket->isActive)
        {
            const auto& BoneToWorld = Skeleton::BoneToWorldSpace(bone_socket->tfm->global_tfm,// tfm->global_tfm,
                                                                 bone_socket->D,
                                                                 bone_socket->index,
                                                                 bone_socket->mesh_bundle);
            tfm->global_tfm = BoneToWorld /* * tfm->global_tfm*/;
        }
    }
#endif
    
    // BoneSocket
    // Example: Ragdoll – the entity is "dragged along" by a Bone
    
    // RBSocket
    // Example: a physical crate
    // tfm->global_tfm = m4f::translation(rb.X) * rb.R * tfm->global_tfm;
    
    /*
     NOTE
     What is really the pivot point between a PHYSICAL SKELETON and the ENTITY
     -"Aux RB" ~ at Mario's feet.
        Animation:
            Manipulated by Controller
            RB controls tfm via Socket: Tfm -> RBaux
            =>
                Controller -[script]-> RBaux -[RBSocket in main entity]-> Tfm
                <=>
                PlayerController -> PrimaryShade
        Ragdoll:
            RB attached to Bone - eg Pelvis (which is in turn attached to another RB)
            As above: RB controls Tfm via Socket: Tfm -> RB
            =>
                RB_Pelvis -[BoneMapping]-> Bone_Pelvis -[BoneSocket]-> RBaux -[RBSocket in main entity]-> Tfm
                <=>
                BoneShade -> RBShade -> PrimaryShade (would require multiple passes)
        
        Alt. Ragdoll
            Bone controls Tfm
            =>
                RB_Pelvis -[BoneMapping]-> Bone_Pelvis -[BoneSocket in main entity]-> Tfm
                <=>
                BoneShade -> PrimaryShade
     */
}
