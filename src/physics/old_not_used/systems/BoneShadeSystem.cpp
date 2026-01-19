//
//  BoneShadeSystem.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-10.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "BoneShadeSystem.hpp"
#include "RigidBody.hpp"
#include "CoreComponents.hpp"
#include "SourceMesh.hpp"
#include "Auxiliary.hpp"
#include "Scene.hpp"

using namespace RigidBody;
using namespace linalg;

void BoneShadeSystem::update(float dt,
                             Scene& scene)
{
#if 0
    // EXPERIMENTAL
    // ADD EXPLICIT RB SOCKET TO A BONE
    //
    {
        auto view = scene.registry.view<MeshComponent, Transform>();
        for(auto entity: view)
        {
            auto& mesh_comp = view.get<MeshComponent>(entity);
            auto& tfm = view.get<Transform>(entity).global_tfm;
            
            if (!scene.resources.validate(mesh_comp.mesh)) continue;
            
            auto& bones = scene.resources.get(mesh_comp.mesh).src_mesh->m_bones;
            auto& nodes = scene.resources.get(mesh_comp.mesh).src_mesh->m_nodetree.nodes;
            
            // To bind
            //        auto& mesh_res = scene.resources.get<MeshResource>(mesh_comp.mesh);
            //        auto& bonearray = scene.resources.get(mesh_comp.bonearray);
            //        mesh_res.src_mesh->animate(-1, 0.0f, bonearray.data()); // to bind
            
            for (int i = 0; i < bones.size(); i++)
            {
                auto& bone = bones[i];
                auto& Bone = scene.resources.get(mesh_comp.bonearray).data_().at(i);
                
                // Inspect the first couple of Bs
                //            if (i < 10)
                //            {
                //                std::cout << Bone << std::endl;
                //            }
                
                if (nodes[bone.node_index].name != "spine_01") continue;
                
                if (first)
                {
                    auto& B_ib = scene.resources.get(mesh_comp.mesh).src_mesh->m_bones[i].inversebind_tfm; // <- B_ib
                    
                    m4f tfm_init = m4f::scaling(0.05, 0.05, 0.05);
                    m4f RB_init = m4f::translation({0,0,0});
                    D = RB_init.inverse() * tfm_init * Bone; // * B_ib.inverse(); // B = Bbind * (Bib * Mesh) = Mesh
                    first = false;
                }
                
                m4f RB = m4f::translation({0,0,5}) * m4f::rotation(fPI/4*0, 0, 0, 1);
                Bone = tfm.inverse() * RB * D; // * B_ib;
                //            std::cout << B << std::endl;
            }
        }
    }
#endif
    
    // Updates bone array bones w.r.t. RB-sockets
    // EntityTypeTag<PrimaryEntity>
    {
        auto view = scene.registry.view<Transform, MeshComponent, SkeletonRigidBody3dSocketComponent>();
        for(auto entity: view)
        {
            auto tfm_inverse = view.get<Transform>(entity).global_tfm.inverse();
            auto& mesh_comp = view.get<MeshComponent>(entity);
            auto& rb_sockets = view.get<SkeletonRigidBody3dSocketComponent>(entity);
            
            if (!scene.resources.validate(mesh_comp.mesh)) continue;
            
            auto& bone_array = scene.resources.get(mesh_comp.bonearray).data_();
            
            // For all RB-sockets
            for (auto& elem : rb_sockets.sockets)
            {
                auto& [bone_index, rb_socket] = elem;
                if (!rb_socket.isActive) continue;
                
                // Bone transform
                auto& Bone = bone_array.at(bone_index);
                
                auto& rb = scene.registry.get<RigidBody3dComponent>(rb_socket.rb_entity.entity);
                const m4f RB_transform = RigidBody::get_transform(rb);
                
                Bone = tfm_inverse * RB_transform * rb_socket.D;
            }
        }
    }
}
