//
//  CoreSystems.cpp
//  editor
//
//  Created by Carl Johan Gribel on 2023-02-28.
//  Copyright © 2023 Carl Johan Gribel. All rights reserved.
//

#include <entt/entt.hpp>
#include "imgui.h"

#include "CoreSystems.hpp"
#include "Scene.hpp"

//#include "CoreComponents.hpp"
//#include "Scene.hpp"
//#include "SceneAPI.hpp"
#include "Forces.hpp"
//#include "Camera.h"

// Environment
#include "InputManager.h"
#include "inputdefs.h"

//#include "ColliderPrimitives.h"
//#include "ImPrimitiveRenderer.hpp"

using namespace entt::literals;
using namespace linalg;
using namespace ImPrimitiveRendererNS;
using namespace RigidBody;

DebugRenderFlags DebugRendererSystem::Flags = DebugRenderFlags {};

// TODO: Move to UI section (print text, make info widgets ...)

void ImguiPrintTextAt(const v3f& world_pos,
                      const m4f& VP_PROJ_MV,
                      const int win_h,
                      const char* str,
                      const char* window_name,
                      const unsigned color_bg,
                      const unsigned color_text)
{
    v4f pos4_ss = VP_PROJ_MV * world_pos.xyz1();

    if (pos4_ss.w < 0) return; // Behind near plane
    // TODO: Cull against the other frustum planes
    
    v2f pos2_ss = pos4_ss.xy() * 1.0f/pos4_ss.w;
    
//    bool op = true; // ???
//    bool* p_open = &op;
    
    //    ImGui::SetWindowPos(ImVec2{pos2_ss.x, camera.frustum.h - pos2_ss.y});
    //    if (!ImGui::GetID(window_name))
    
    ImGui::SetNextWindowPos(ImVec2{pos2_ss.x, win_h - pos2_ss.y},
                            ImGuiCond_Always,
                            ImVec2 {0.0f, 0.0f});
    // ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + ImVec2(80, 80));
    
    //ImGui::SetNextWindowBgAlpha(0.5f); // Transparent background
    ImGui::PushStyleColor(ImGuiCol_WindowBg, color_bg);
    ImGui::PushStyleColor(ImGuiCol_Text, color_text); // ABGR red 0xff0000ff, green 0xff00ff00
    
    ImGuiWindowFlags flags =
    ImGuiWindowFlags_NoDecoration |
    // ImGuiWindowFlags_NoBackground |
    ImGuiWindowFlags_AlwaysAutoResize |
    ImGuiWindowFlags_NoDocking;
    
    flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNavFocus;
    
    if (ImGui::Begin(window_name, nullptr, flags))
    {
        ImGui::Text("%s", str);
        ImGui::End();
    }
    ImGui::PopStyleColor(2);
}

static bool BeginViewportWidgetAt(const v3f& world_pos,
                                  const m4f& VP_PROJ_MV,
                                  const int win_h,
                                  const char* window_name,
                                  const unsigned color_bg,
                                  const unsigned color_text)
{
    v4f pos4_ss = VP_PROJ_MV * world_pos.xyz1();

    if (pos4_ss.w < 0) return false; // Behind near plane
    // TODO: Cull against the other frustum planes
    
    v2f pos2_ss = pos4_ss.xy() * 1.0f/pos4_ss.w;
    
//    bool op = true; // ???
//    bool* p_open = &op;
    
    //    ImGui::SetWindowPos(ImVec2{pos2_ss.x, camera.frustum.h - pos2_ss.y});
    //    if (!ImGui::GetID(window_name))
    ImGui::SetNextWindowPos(ImVec2{pos2_ss.x, win_h - pos2_ss.y},
                            ImGuiCond_Always,
                            ImVec2 {0.0f, 0.0f});
    // ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + ImVec2(80, 80));
    
    //ImGui::SetNextWindowBgAlpha(0.5f); // Transparent background
    ImGui::PushStyleColor(ImGuiCol_WindowBg, color_bg);
    ImGui::PushStyleColor(ImGuiCol_Text, color_text); // ABGR red 0xff0000ff, green 0xff00ff00
    
    ImGuiWindowFlags flags =
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoInputs |
    // ImGuiWindowFlags_NoBackground |
    ImGuiWindowFlags_AlwaysAutoResize;
    
    return ImGui::Begin(window_name, nullptr, flags);
}

static void EndViewportWidgetAt()
{
    ImGui::End();
    ImGui::PopStyleColor(2);
}

// MARK: --- Mesh hierarchy system

void SkeletonTraversalSystem::update(float t,
                                     float dt,
                                     entt::registry& entity_reg,
                                     ResourceRegistry& resource_reg)
{
    auto view = entity_reg.view<MeshComponent, Transform>();
    
    for(auto entity: view)
    {
        auto& meshcomp = entity_reg.get<MeshComponent>(entity);
        
        if (!resource_reg.validate(meshcomp.mesh)) continue;
        const auto& meshres = resource_reg.get(meshcomp.mesh);
        if (meshres.desc.ignore_hierarchy) continue;
        
        bool has_ACC = false; // registry.all_of<AnimationClipComponent>(entity);
        bool has_controller = false; // bundle->animation_controller
        
        if (has_controller)
        {
            // - Ok also if not skinned? Should traverse nodes same way as animate()
            // (so keep the FIX below)
            // Also note that the controller of course should use THE SAME SourceMesh as the one in MeshBundle
//                if (bundle->is_skinned())
//                    bundle->animation_controller->update(dt,
//                                                         bundle->bonearrayUBO->data());
//                else
//                    bundle->animation_controller->update(dt,
//                                                         nullptr);
        }
        else if (!has_ACC)
        {
            // "Legacy" - no controller, run plain animate() with given clip index
            float t_anim = meshcomp.anim_speed * t + meshcomp.anim_phase_offset;
            if (meshcomp.is_skinned)
            {
                auto& bonearray = resource_reg.get(meshcomp.bonearray);
                assert(bonearray.data_().size() == meshres.src_mesh->m_bones.size());
                meshres.src_mesh->animate(meshcomp.anim_clip_index,
                                          t_anim,
                                          bonearray.data());
            }
            else
            {
                // Non-skinned
                meshres.src_mesh->animate(meshcomp.anim_clip_index,
                                          t_anim);
            }
        }
        
        // Transfer mesh AABBs (updated in animate()) to entity's MeshBundle.
        // * Skinned meshes: use the model AABB = union of all bone AABBs
        // * Non-skinned meshes: use the mesh's own AABB
        //
        // Note that AABBs are transfered only for entities which has a
        // MeshBundle AND a hierarchy (few within  a scene, comparable speaking)
        // AABBs for renderable entities which does not are
        // stored in the Scene's RenderEntity list.
        //
        // This requires AABB's to exist (temporarily) in SourceMesh which is not
        // ideal - ideally, the SourceMesh(/Skeleton) class only peforms operations
        // on state that exists elsewhere, e.g. in MeshBundle & MeshComponent.
        //
        for (int i = 0; i < meshcomp.submeshes.size(); i++)
        {
            SubMeshComponent& submesh = meshcomp.submeshes[i];
            if (submesh.is_skinned)
                submesh.aabb = meshres.src_mesh->m_model_aabb;
            else
                submesh.aabb = meshres.src_mesh->m_mesh_aabbs_pose[i];
            
            // Shade SubMeshComponent to world
//                submesh.aabb = submesh.aabb.post_transform(tfm.global_tfm);
//                std::cout << i <<  ", min " << submesh.aabb.min << ", max " << submesh.aabb.max << std::endl;
        }
        
        // FIX: Store global node tfm's to meshes that inherit from non-bone nodes
        //if (mesh_bundle) // have this earlier, or at all?
        for (int i = 0; i < meshcomp.submeshes.size(); i++)
        {
            const auto& submeshres = meshres.submeshes[i];
            auto& submeshcomp = meshcomp.submeshes[i];
            if (submeshres.node_index > -1 && !submeshres.is_skinned)
                submeshcomp.global_node_tfm = meshres.src_mesh->m_nodetree.nodes[submeshres.node_index].global_tfm;
        }
    }
}

// MARK: --- DebugRendererSystem -----------------------------------------------

void DebugRendererSystem::update(float t,
                                 float dt,
                                 int long frame_number,
                                 entt::registry& entity_reg,
                                 ResourceRegistry& resource_reg,
                                 std::shared_ptr<ImPrimitiveRenderer> imrend,
                                 RayContact& camera_ray,
//                                 DebugRenderFlags& flags,
                                 const m4f& ProjView,
                                 const m4f& VPProjView,
                                 const int win_h)
{
    entt::entity ray_collider_ent = static_cast<entt::entity>(camera_ray.collider_entityu);
//    entt::entity ray_collider_ent = entt::null;
    
    // MARK: View & Light frusta
    
    if (Flags.ViewFrusta)
    {
        imrend->push_states(Color4u::Silver, DepthTest::True);
        auto view = entity_reg.view<Camera>();
        for (auto& entity : view)
        {
            const auto& camera = view.get<Camera>(entity);
            imrend->push_frustum(camera.ProjView_inverse);
        }
        imrend->pop_states<Color4u, DepthTest>();
    }
    
    if (Flags.LightFrusta)
    {
        imrend->push_states(Color4u::Yellow, DepthTest::True);
        auto view = entity_reg.view<LightSource>();
        for (auto& entity : view)
        {
            const auto& light = view.get<LightSource>(entity);
            imrend->push_frustum(light.ProjView.inverse());
        }
        imrend->pop_states<Color4u, DepthTest>();
    }
    
    // MARK: Mesh AABBs
    if (Flags.MeshAABB)
    {
        imrend->push_states(DepthTest::True);
        auto view = entity_reg.view<Transform, MeshComponent>();
        for (auto& entity : view)
        {
            auto& meshcomp = view.get<MeshComponent>(entity);
            auto& tfm = view.get<Transform>(entity);
            
            for (auto& submeshcomp : meshcomp.submeshes)
            {
                AABB3d aabbw = submeshcomp.aabb.post_transform(tfm.global_tfm);
                //                AABB3d aabbw = re.mesh_->aabb.post_transform(re.transform_->global_tfm);
                // Render AABB
                vec3f size = aabbw.max - aabbw.min;
                vec3f pos = aabbw.min + size*0.5f;
                mat4f M = mat4f::translation(pos) * mat4f::scaling(size);
                
                imrend->push_states(Color4u {0xFFE61A80}, M); // {0.5f, 0.1f, 0.9f}
                imrend->push_cube_wireframe();
                imrend->pop_states<Color4u, m4f>();
            }
        }
        imrend->pop_states<DepthTest>();
    }
    
    // MARK: Transforms
    if (Flags.TransformOn)
    {
        imrend->push_states(DepthTest::False);
        auto view = entity_reg.view<HeaderComponent, Transform>();
        for(auto entity: view)
        {
            const auto& h = view.get<HeaderComponent>(entity);
            const auto& tfm = view.get<Transform>(entity).global_tfm;
            
            if (Flags.TransformAxes)
            {
                float bsr = 1.0f; //aimesh->mSceneAABB.get_boundingsphere().w;
//                const ArrowDescriptor arrdesc = { .cone_fraction=0.1f, .cone_radius=bsr/50, .cylinder_radius=bsr/150};
//                imrend->push_basis(tfm, bsr, arrdesc);
                imrend->push_basis_basic(tfm, bsr);
            }
            if (Flags.TransformCubes)
            {
                const float scale = 0.1f;
                const m4f tfm_nrm = normalize(tfm) * m4f::scaling(scale);
                const v3f color {1.0f, 0.0f, 0.0f};
                
                imrend->push_states(Color4u {color}, tfm_nrm);
                imrend->push_cube_wireframe();
                imrend->pop_states<Color4u, m4f>();
            }
            
            if(Flags.TransformLabels)
                if (BeginViewportWidgetAt(extract_translation(tfm),
                                          VPProjView,
                                          win_h,
                                          std::to_string(h.guid).c_str(),
                                          Flags.TransformLabelBgColor,
                                          Flags.TransformLabelTextColor))
                {
                    ImGui::Text("%s", h.str);
                    ImGui::Text("(%1.2f,%1.2f,%1.2f)", tfm.m14, tfm.m24,tfm.m34);
                    
                    EndViewportWidgetAt();
                }
        }
        imrend->pop_states<DepthTest>();
    }
    
#if 0
    // TODO: Bounding spheres (REMOVE)
    if (flags.BoundingSpheres)
    {
        auto view = registry.view<BoundingSphereComponent, Handle<SceneGraph::Transform>>();
        
        for(auto entity: view)
        {
            const auto& tfm = view.get<Handle<SceneGraph::Transform>>(entity);
            const auto& bscomp = view.get<BoundingSphereComponent>(entity);
            imrend->push_sphere(bscomp.sphere.w,
                                bscomp.sphere.w,
                                tfm->global_tfm * m4f::translation(bscomp.sphere.xyz()),
                                DBGCOLOR::YELLOW);
        }
    }
#endif
    
    // MARK: Rigid bodies (3d)
    // Assume RB's are in world space and does not have SG parents
    // What if the RB is child to a node? <- should be baked to the RB (like colliders)
    if (Flags.RigidBodyOn)
    {
        imrend->push_states(DepthTest::False);
        auto view_rb3d = entity_reg.view<HeaderComponent, RigidBody3dComponent>();
        auto view_rb2d = entity_reg.view<HeaderComponent, RigidBody2dComponent>();
        int c = 0;
        for(auto entity: view_rb3d)
        {
            const auto& rb = view_rb3d.get<RigidBody3dComponent>(entity);
            const auto& header = view_rb3d.get<HeaderComponent>(entity);
            
            const float scale = 0.15f;
            const m4f M_rb = RigidBody::get_transform(rb); //m4f::translation(rb.X) * rb.R;
            const v3f color {0.5f, 0.5f, 1.0f};
                        
            // Cube
            if (Flags.RigidBodyCubes)
            {
                imrend->push_states(Color4u {color}, M_rb * m4f::scaling(scale));
                imrend->push_cube_wireframe();
                imrend->pop_states<Color4u, m4f>();
            }
            
            // Axes
            if (Flags.RigidBodyAxes) 
            {
                float bsr = 0.15f;
                imrend->push_states(M_rb);
                imrend->push_basis_basic(M_rb, bsr);
                imrend->pop_states<m4f>();
            }

            // Label
            if (Flags.RigidBodyLabels)
            {
                if (BeginViewportWidgetAt(rb.X,
                                          VPProjView,
                                          win_h,
                                          std::to_string(header.guid).c_str(),
                                          Flags.RigidBodyLabelBgColor,
                                          Flags.RigidBodyLabelTextColor))
                {
                    ImGui::Text("%s", header.str);
                    ImGui::Text("mass %f", rb.m);
                    EndViewportWidgetAt();
                }
            }
        }
        imrend->pop_states<DepthTest>();

        // MARK: Rigid bodies (2d)
        imrend->push_states(DepthTest::False);
        for(auto entity: view_rb2d)
        {
            const auto& rb = view_rb2d.get<RigidBody2dComponent>(entity);
            const auto& header = view_rb2d.get<HeaderComponent>(entity);
            
            const float scale = 0.15f;
            const m4f M_rb = RigidBody::get_transform(rb);
            const v3f color {0.5f, 0.5f, 1.0f};
            
            // Cube
            if (Flags.RigidBodyCubes)
            {
                imrend->push_states(Color4u {color}, M_rb * m4f::scaling(scale));
                imrend->push_cube_wireframe();
                imrend->pop_states<Color4u, m4f>();
            }
            
            // Axes
            if (Flags.RigidBodyAxes) {
                float bsr = 0.15f;

                imrend->push_states(M_rb);
                imrend->push_basis_basic2d(M_rb, bsr);
                imrend->pop_states<m4f>();
            }

            // Label
            if (Flags.RigidBodyLabels)
            {
                if (BeginViewportWidgetAt(xy0(rb.X),
                                          VPProjView,
                                          win_h,
                                          std::to_string(header.guid).c_str(),
                                          Flags.RigidBodyLabelBgColor,
                                          Flags.RigidBodyLabelTextColor))
                {
                    ImGui::Text("%s", header.str);
                    ImGui::Text("mass %f", rb.m);
                    EndViewportWidgetAt();
                }
            }
        }
        imrend->pop_states<DepthTest>();
    }
    
    // MARK: Colliders (3d)
    // Assume colliders have been "vertex shaded" to world space
    
    // Base collider
    if (Flags.ColliderOn)
    {
        {
            imrend->push_states(Color4u {0xFFE61A80}, DepthTest::False);
            auto view = entity_reg.view<HeaderComponent, Base3dCollider, RigidBody3dEntity>();
            for(auto entity: view)
            {
                const auto& h = view.get<HeaderComponent>(entity);
                auto& basecol = view.get<Base3dCollider>(entity);
                auto& rb_ent = view.get<RigidBody3dEntity>(entity);
                
                if (Flags.ColliderAABB)
                {
                    const v3f size = basecol.aabb_w.max - basecol.aabb_w.min;
                    const v3f pos = basecol.aabb_w.min + size * 0.5f;
                    const m4f M = m4f::translation(pos) * m4f::scaling(size);
                    
                    imrend->push_states(M);
                    imrend->push_cube_wireframe();
                    imrend->pop_states<m4f>();
                }
                
                if (Flags.ColliderLabels)
                {
                    if (BeginViewportWidgetAt(basecol.aabb_w.get_midpoint(),
                                              VPProjView,
                                              win_h,
                                              std::to_string(h.guid).c_str(),
                                              Flags.ColliderLabelBgColor,
                                              Flags.ColliderLabelTextColor))
                    {
                        ImGui::Text("%s", h.str);
                        ImGui::Text("Type: %s", enum_name(basecol.type));
                        ImGui::Text("Trigger: %i", (int)basecol.is_trigger);
                        if (rb_ent.entity != entt::null)
                        {
                            const char* rb_name = entity_reg.get<HeaderComponent>(rb_ent.entity).str;
                            ImGui::Text("%s", rb_name);
                        }
                        EndViewportWidgetAt();
                    }
                }
            }
            imrend->pop_states<Color4u, DepthTest>();
        }
        
        // Sphere
        //
        if (Flags.ColliderShape || ray_collider_ent != entt::null)
        {
            imrend->push_states(Color4u {Flags.ColliderShapeColor}, DepthTest::True);
            auto view = entity_reg.view<SphereCollider>();
            for(auto entity : view)
            {
                if (!Flags.ColliderShape && entity != ray_collider_ent)
                    continue;
                
                auto& sphere = view.get<SphereCollider>(entity);
                
                // The collider itself doesn't have a rotation,
                // so get the rotation of the RB if there is one.
                m4f R = m4f_identity;
                auto& rb_entity = entity_reg.get<RigidBody3dEntity>(entity);
                if (rb_entity)
                    R = m4f( entity_reg.get<RigidBody3dComponent>(rb_entity.entity).R );

                // ...or extract R from Transform
                // auto& parent_entity = entity_reg.get<PrimaryEntity>(entity);
                // const auto& tfm = entity_reg.get<Transform>(parent_entity.entity).global_tfm;
                // m4f R = set_translation(normalize(tfm), v3f_000);
                
                const auto M = m4f::translation(sphere.pos_w) * R;
                
                imrend->push_states(M);
                imrend->push_sphere_wireframe(sphere.r_w, sphere.r_w);
                imrend->pop_states<m4f>();
            }
            imrend->pop_states<Color4u, DepthTest>();
        }
        
        // Polyhedron
        //
        if (Flags.ColliderShape || ray_collider_ent != entt::null)
        {
            imrend->push_states(DepthTest::True);
            auto view = entity_reg.view<PolyhedronCollider>();
            for(auto entity : view)
            {
                if (!Flags.ColliderShape && entity != ray_collider_ent)
                    continue;
                
                auto& poly = view.get<PolyhedronCollider>(entity);
                
                imrend->push_states(Color4u {Flags.ColliderShapeColor});
                imrend->push_lines(poly.vertices_w.data(),
                                   poly.vertices_w.size(),
                                   poly.edges.data(),
                                   poly.edges.size());
                imrend->pop_states<Color4u>();
                
                if (Flags.ColliderFaceNormals)
                {
                    // Normals
                    imrend->push_states(Color4u {Flags.ColliderFaceNormalColor});
                    unsigned vindex = 0;
                    for (int i = 0; i < poly.nbr_faces; i++)
                    {
                        const v3f& v0 = poly.vertices_w[poly.faces[vindex+0]];
                        const v3f& n = poly.normals_w[i];
                        vindex += poly.face_strides[i];
                        
                        imrend->push_line(v0,
                                          v0 + n);
                    }
                    imrend->pop_states<Color4u>();
                }
            }
            imrend->pop_states<DepthTest>();
        }
        
        // MeshCollider
        //
        if (Flags.ColliderShape || ray_collider_ent != entt::null)
        {
            imrend->push_states(DepthTest::True);
            auto view = entity_reg.view<MeshCollider>();
            for(auto entity : view)
            {
                if (!Flags.ColliderShape && entity != ray_collider_ent)
                    continue;
                
                auto& mesh = view.get<MeshCollider>(entity);
                
                imrend->push_states(Color4u {Flags.ColliderShapeColor});
                imrend->push_lines(mesh.vertices_w.data(),
                                   mesh.vertices_w.size(),
                                   mesh.edges.data(),
                                   mesh.edges.size());
                imrend->pop_states<Color4u>();
                
                if (Flags.ColliderFaceNormals)
                {
                    // Normals
                    imrend->push_states(Color4u {Flags.ColliderFaceNormalColor});
                    for (int i = 0; i < mesh.faces.size(); i += 3)
                    {
                        const v3f& v0 = mesh.vertices_w[mesh.faces[i]];
                        const v3f& n = mesh.normals_w[i/3];
                        imrend->push_line(v0,
                                          v0 + n);
                    }
                    imrend->pop_states<Color4u>();
                }
            }
            imrend->pop_states<DepthTest>();
        }
        
#if 0
        //auto view = registry.view<Handle<ColliderBase>>();
        auto view = registry.view<entt::tag<"ColliderEntity"_hs>>();

        int c = 0;
        for(auto entity: view)
        {
            Handle<ColliderBase> collider = registry.get<Handle<ColliderBase>>(entity);
            auto& rb_ent = registry.get<RigidBody3dEntity>(entity);
            bool is_selected = entity == ray_collider_ent;

            // MARK: Collider inside points (inactive)
            //                imrend->push_cube_wireframe(m4f::translation(collider->inside_point_w) * m4f::scaling(0.2f),
            //                                            {1,0,0});

            if (Flags.ColliderAABB)
            {
                v3f size = collider->aabb_w.max - collider->aabb_w.min;
                v3f pos = collider->aabb_w.min + size * 0.5f;
                m4f M = m4f::translation(pos) * m4f::scaling(size);
                imrend->push_cube_wireframe(M, v3f {0.5f, 0.1f, 0.9f});
            }

            Handle<SphereCollider> sphere;
            Handle<PolyhedronCollider> poly;
            std::string typestr {};

            if (Flags.ColliderShape || is_selected)
            {
                if ((sphere = dynamic_handle_cast<SphereCollider>(collider)))
                {
                    imrend->push_sphere(sphere->sphere_w.w,
                                        sphere->sphere_w.w,
                                        /*tfm->global_tfm **/ m4f::translation(sphere->sphere_w.xyz()),
                                        BaseColors::Magenta); // TODO: Use color preset
                    typestr = "sphere3d";
                }
                else if ((poly = dynamic_handle_cast<PolyhedronCollider>(collider)))
                {
                    imrend->push_lines(poly->vertices_w.data(),
                                       poly->vertices_w.size(),
                                       poly->edges.data(),
                                       poly->edges.size(),
                                       BaseColors::Magenta); // TODO: Use color preset
                    typestr = "poly3d";
                }
            }

            if (Flags.ColliderLabels) {
                const std::string win_name = "col3d" + std::to_string(c++); // unique window name
                const std::string label =
                typestr + (collider->is_trigger?"[trig]":"") +
                (rb_ent.entity != entt::null? "[rb]":"");
                ImguiPrintTextAt(collider->inside_point_w,
                                 VPProjView,
                                 win_h,
                                 label.c_str(),
                                 win_name.c_str(),
                                 flags.ColliderLabelBgColor,
                                 flags.ColliderLabelTextColor);
            }
        }
#endif
    }
    
    // MARK: Colliders (2d)
    // Assume colliders have been "vertex shaded" to world space
    if (Flags.ColliderOn)
    {
        // BaseColliders
        //
        imrend->push_states(Color4u {0xFFE61A80}, DepthTest::True);
        auto basecol_view = entity_reg.view<HeaderComponent, Base2dCollider, RigidBody2dEntity>();
        for(auto basecol2d_entity: basecol_view)
        {
            const auto& h = basecol_view.get<HeaderComponent>(basecol2d_entity);
            auto& basecol = basecol_view.get<Base2dCollider>(basecol2d_entity);
            auto& rb_ent = basecol_view.get<RigidBody2dEntity>(basecol2d_entity);
//            bool has_rb = rb_ent.entity != entt::null;
            
            if (Flags.ColliderAABB)
            {
                v2f size = basecol.aabb_w.max - basecol.aabb_w.min;
                v2f pos = basecol.aabb_w.min + size * 0.5f;
                m4f M = m4f::translation(xy0(pos)) * m4f::scaling(xy1(size));
                
                imrend->push_states(M);
                imrend->push_quad_wireframe();
                imrend->pop_states<m4f>();
            }
            
            if (Flags.ColliderLabels)
            {
                if (BeginViewportWidgetAt(xy0(basecol.aabb_w.get_midpoint()),
                                          VPProjView,
                                          win_h,
                                          std::to_string(h.guid).c_str(),
                                          Flags.ColliderLabelBgColor,
                                          Flags.ColliderLabelTextColor))
                {
                    ImGui::Text("%s", h.str);
                    ImGui::Text("Type: %s", enum_name(basecol.type));
                    ImGui::Text("Trigger: %i", (int)basecol.is_trigger);
                    if (rb_ent.entity != entt::null)
                    {
                        const char* rb_name = entity_reg.get<HeaderComponent>(rb_ent.entity).str;
                        ImGui::Text("Rigid Body: %s", rb_name);
                    }
                    EndViewportWidgetAt();
                }
                
//                const std::string win_name = "col2d" + std::to_string(c++); // unique window name
//                const std::string label =
//                //ColliderType_::NameFromType.at(basecol.type) +
//                std::string(enum_name(basecol.type)) +
//                (basecol.is_trigger?"[trig]":"") +
//                (has_rb? "[rb]":"");
//                ImguiPrintTextAt(xy0(basecol.aabb_w.get_midpoint()),
//                                 VPProjView,
//                                 win_h,
//                                 label.c_str(),
//                                 win_name.c_str(),
//                                 Flags.ColliderLabelBgColor,
//                                 Flags.ColliderLabelTextColor);
            }
        }
        imrend->pop_states<Color4u, DepthTest>();
        
        // AABB2dColliders
        //
//        auto aabb2dcol_view = registry.view<Base2dCollider, AABB2dCollider, RigidBody2dEntity>();
//        for(auto aabb2dcol_entity: aabb2dcol_view)
//        {
//            bool is_selected = aabb2dcol_entity == ray_collider_ent;
//
//            if (DebugRenderFlags.ColliderShape || is_selected)
//            {
//                auto& basecol = aabb2dcol_view.get<Base2dCollider>(aabb2dcol_entity);
//                v2f size = basecol.aabb_w.max - basecol.aabb_w.min;
//                v2f pos = basecol.aabb_w.min + size * 0.5f;
//                m4f M = m4f::translation(xy0(pos)) * m4f::scaling(xy1(size));
//                imrend->push_quad_wireframe(M, v3f {0.5f, 0.1f, 0.9f});
//            }
//        }
        
        // Polygon entities
        //
        imrend->push_states(DepthTest::True);
        auto poly2d_view = entity_reg.view<Base2dCollider, Polygon2dCollider, RigidBody2dEntity>();
        for(auto poly2d_entity : poly2d_view)
        {
            bool is_selected = poly2d_entity == ray_collider_ent;
            
            if (Flags.ColliderShape || is_selected)
            {
                auto& poly = poly2d_view.get<Polygon2dCollider>(poly2d_entity);
                
                for (int i = 0; i < poly.nbr_vertices; i++)
                {
                    const auto v0 = poly.vertices_w[i];
                    const auto v1 = poly.vertices_w[(i+1)%poly.nbr_vertices];
                    
                    imrend->push_states(Color4u {Flags.ColliderShapeColor});
                    imrend->push_line(xy0(v0), xy0(v1));
                    imrend->pop_states<Color4u>();
                    
                    if (Flags.ColliderFaceNormals)
                    {
                        const auto vn = poly.normals_w[i];
                        
                        imrend->push_states(Color4u {Flags.ColliderFaceNormalColor});
                        imrend->push_line(xy0(v0), xy0(v0 + vn));
                        imrend->pop_states<Color4u>();
                    }
                }
            }
        }
        imrend->pop_states<DepthTest>();
        
        // Circle entities
        //
        imrend->push_states(DepthTest::True);
        auto circle_view = entity_reg.view<Base2dCollider, CircleCollider, RigidBody2dEntity>();
        for(auto circle_entity : circle_view)
        {
            bool is_selected = circle_entity == ray_collider_ent;
            
            if (Flags.ColliderShape || is_selected)
            {
                auto& circle = circle_view.get<CircleCollider>(circle_entity);
                
                // The collider itself doesn't have a rotation,
                // so get the rotation of the RB if there is one.
                float rad = 0.0f;
                auto& rb_ent = basecol_view.get<RigidBody2dEntity>(circle_entity);
                if (rb_ent.entity != entt::null)
                    rad = entity_reg.get<RigidBody2dComponent>(rb_ent.entity).R;
                
                const v3f sv = {circle.r_w, circle.r_w, 1.0f};
                const m4f M = m4f::TRS(xy0(circle.pos_w),
                                       rad, v3f_001,
                                       sv);
                static const auto circle_ring = CircleRing3d<16>();
                
                imrend->push_states(Color4u {Flags.ColliderShapeColor}, M);
                imrend->push_lines(circle_ring.vertices, circle_ring.nbr_vertices);
                imrend->pop_states<Color4u, m4f>();
            }
        }
        imrend->pop_states<DepthTest>();
        
        // Other collider types ...
    }
    
    // MARK: Aux system: render skeleton hierarchy & node labels
    
    // Note: re-traverses all skeletons which is quite demanding
    if (Flags.SkeletonOn)
    {
        imrend->push_states(Color4u::Yellow, DepthTest::False);
        auto view = entity_reg.view<HeaderComponent, MeshComponent, Transform>();
        
        // EXPERIMENTAL - uses existing bone arrays. Faster & takes modifications
        // made by RB-sockets into account, although bone relations are lost.
        for(auto entity: view)
        {
            const auto& header = view.get<HeaderComponent>(entity);
            const auto& global_tfm = view.get<Transform>(entity).global_tfm;
            const auto& mesh_comp = view.get<MeshComponent>(entity);
            
            if (!resource_reg.validate(mesh_comp.mesh)) continue;
            const auto& mesh_res = resource_reg.get(mesh_comp.mesh);
            if (mesh_res.desc.ignore_hierarchy) continue;
            if (!mesh_comp.is_skinned) continue;
            
            auto& bonearray = resource_reg.get(mesh_comp.bonearray);
            
            for(int i = 0; i < bonearray.data_().size(); i++)
            {
                const m4f& boneM = bonearray.data_().at(i);
                const auto& bone = mesh_res.src_mesh->m_bones[i];
                const auto& node = mesh_res.src_mesh->m_nodetree.nodes[bone.node_index];
                const m4f& bibM = bone.inversebind_tfm;
                const auto& G = global_tfm * boneM * bibM.inverse();
                v3f p = extract_translation(G);
                
                if (Flags.SkeletonNodes)
                {
                    imrend->push_point(p, 4);
//                    imrend->push_sphere(0.05, 0.05, m4f::translation(p), BaseColors::Red);
//                    imrend->push_sphere_wireframe(0.05, 0.05, m4f::translation(p), BaseColors::Red);
                    
//                    for (int i = 0; i < 10; i++)
//                        imrend->push_sphere(0.05, 0.05, m4f::translation(p+v3f(i,0,0)), BaseColors::Red);
                }

                if (Flags.SkeletonNodeAxes)
                    imrend->push_basis_basic(G, 0.2f);
                
                if (Flags.SkeletonNodeLabels)
                {
                    // Note: PushID doesn't apply to windows, so this is a way
                    // to produce unique label names for all entities & bones.
                    // Could add "###" to the string in order to explicitly hide
                    // the id -- but the window name is not visible anyway.
                    size_t labelhash = hash_combine(header.guid, i);
                    std::string labelstr = std::to_string(labelhash);
                    
                    std::string str = std::to_string(i) + "_" + node.name;
                    unsigned bgcolor;
                    if (node.bone_index > -1) bgcolor = Flags.SkeletonNodeLabelBgColor;
                    else bgcolor = Flags.SkeletonBoneLabelBgColor;
                    ImguiPrintTextAt(p,
                                     VPProjView,
                                     win_h,
                                     str.c_str(),
                                     labelstr.c_str(),
                                     bgcolor,
                                     Flags.SkeletonNodeLabelTextColor);
                }
            }
        }
        imrend->pop_states<Color4u, DepthTest>();
        
#if 0
        for(auto entity: view)
        {
            auto& global_tfm = view.get<Transform>(entity).global_tfm;
            auto& meshcomp = view.get<MeshComponent>(entity);
            
            if (!resource_reg.validate(meshcomp.mesh)) continue;
            const auto& meshres = resource_reg.get(meshcomp.mesh);
            if (meshres.desc.ignore_hierarchy) continue;
            
            bool has_ACC = false; // registry.all_of<AnimationClipComponent>(entity);
            bool has_controller = false; // bundle->animation_controller
            
            if (has_controller)
            {
            }
            else if (!has_ACC)
            {
                // "Legacy" - no controller, run plain animate() with given clip index
                float t_anim = meshcomp.anim_speed * t + meshcomp.anim_phase_offset;
                meshres.src_mesh->animate(meshcomp.anim_clip_index, t_anim);
            }
            
            const auto& nodes = meshres.src_mesh->m_nodetree.nodes;
            
            if (Flags.SkeletonNodes)
                render_skeleton_nodes(nodes, global_tfm, 0.1f, imrend);
            
            if (Flags.SkeletonNodeLabels || Flags.SkeletonNodeAxes)
            {
                for (int i = 0; i < nodes.size(); i++)
                    //    for (auto& node: m_nodetree.nodes)
                {
                    const auto& node = nodes[i];
                    m4f G = global_tfm * node.global_tfm;
                    
                    if (Flags.SkeletonNodeAxes)
                        imrend->push_basis_basic(G, 0.2f);
                    
                    if (Flags.SkeletonNodeLabels)
                    {
                        std::string str = "[" + std::to_string(i) + "] " + node.name;
                        if (node.bone_index > -1) str += " (" + std::to_string(node.bone_index) + ")";
                        // Clunky way to provide a per-instance & per-bone unique window name
                        std::string window_name = std::to_string(i/*mesh_bundle.m_byte_index*/) + str;
                        unsigned bgcolor;
                        if (node.bone_index > -1) bgcolor = Flags.SkeletonNodeLabelBgColor;
                        else bgcolor = Flags.SkeletonBoneLabelBgColor;
//                        ImGui::PushID(i);
                        ImguiPrintTextAt(extract_translation(G),
                                         VPProjView,
                                         win_h,
                                         str.c_str(),
                                         window_name.c_str(),
                                         bgcolor,
                                         Flags.SkeletonNodeLabelTextColor);
//                        ImGui::PopID();
                    }
                }
            }
        }
#endif
    }
    
    // MARK: Selected entity
    // TODO: Find & list all components of the entity
    //      https://github.com/skypjack/entt/issues/88#issuecomment-761892912
    //      https://github.com/skypjack/entt/blob/47ada87ba2fab08c70fdd61fa2835a27fb9ce8a7/src/entt/entity/registry.hpp#L1492
    // TODO: The camera_ray RayContact may point to either a 3D or a 2D collider/RB;
    //
//    if (camera_ray.isColliderContact())
//    {
//        entt::entity collider_entity = static_cast<entt::entity>(camera_ray.collider_entityu);
//        auto& collider = registry.get<Handle<ColliderBase>>(collider_entity);
//        auto& parent_ent = registry.get<ParentEntity>(collider_entity).entity;
//        auto& rb_ent = registry.get<RigidBody3dEntity>(collider_entity).entity;
//
//        assert(parent_ent != entt::null);
//        if (parent_ent != entt::null)
//        {
//            auto& tfm = registry.get<Handle<Transform>>(parent_ent);
//            v3f pos = extract_translation(tfm->global_tfm);
//
//            std::stringstream ss;
//            ss << std::fixed << std::setprecision(1);
//            if (collider->is_trigger) ss << "Trigger ELIAS" << std::endl;
//            ss << "Entity" << std::endl;
//            ss << "X = " << extract_translation(tfm->global_tfm) << std::endl;
//            if (rb_ent != entt::null)
//            {
//                auto& rb = registry.get<RigidBody3dComponent>(rb_ent);
//                pos = rb.X;
//                ss << "Rigid body" << std::endl;
//                ss << "X = " << rb.X << std::endl;
//                ss << "V = " << rb.V << std::endl;
//                ss << "W = " << rb.W << std::endl;
//                ss << "m = " << rb.m << std::endl;
//                ss << "I =\n" << rb.I << std::endl;
//            }
//            ImguiPrintTextAt(pos,
//                             VPProjView,
//                             win_h,
//                             ss.str().c_str(),
//                             "parent_selection",
//                             flags.SelectionLabelBgColor,
//                             flags.SelectionLabelTextColor);
//        }
//#if 0
//        ImguiPrintTextAt(camera_ray.point_of_contact(),
//                         VPProjView,
//                         win_h,
//                         "X",
//                         "hit_selection",
//                         flags.SelectionLabelBgColor,
//                         flags.SelectionLabelTextColor);
//#endif
//    }
    
    // MARK: Trail
    //
//    if (1)
//    {
//        auto view = entity_reg.view<PlayerController2dComponent>();
//        for(auto entity: view)
//        {
//            auto& player = view.get<PlayerController2dComponent>(entity);
//            if (!player.trail.nbr_vertices) continue;
//
//            imrend->push_lines_from_cyclic_source(player.trail.vertices,
//                                                  player.trail.start_index,
//                                                  player.trail.nbr_vertices,
//                                                  player.trail.max_vertices);
//        }
//    }
    
    // MARK: Particles
    //
//    if (1)
//    {
//        imrend->push_points(PointParticleSystem::points,
//                            PointParticleSystem::nbr_points,
//                            5);
//
//        // Point size can be scaled with depth.
//        // If the points are to have individual sizes, they have to be added
//        // one by one, since they are hashed wrt point size within the renderer.
//        // Each point size requires one draw call.
//
//        // Available matrices: VPProjView (depth [0,1], ProjView (depth [znear,zfar])
//        //        vec3f rAw = bsctr->bodyA->X + bsctr->bodyA->R * bsctr->rA;
//        //        // Point size: 10-1 for view distance 1-10
//        //        float rAviewZ = dot( PROJ_VIEW.row(3), rAw.xyz1() );
//        //        float size = clamp<float>( lerp<float>(10, 1, rAviewZ/10), 1, 10);
//    }
    
    // MARK: Sticky notes
    //
    if (Flags.StickyNotes)
    {
        auto view = entity_reg.view<StickyNoteComponent>();

        int c = 0;
        for(auto entity: view)
        {
            auto& note = entity_reg.get<StickyNoteComponent>(entity);
            v3f pos;

            // Figure what kind of entity this is, in order to know where
            // to locate the sticky note
            Transform* tfm_ptr; // Primary or RB Entity
            Base2dCollider* col2d_ptr;
            Base3dCollider* col3d_ptr;
            if ((tfm_ptr = entity_reg.try_get<Transform>(entity)))
            {
                pos = extract_translation(tfm_ptr->global_tfm);
            }
            else if ((col2d_ptr = entity_reg.try_get<Base2dCollider>(entity)))
            {
                pos = xy0(col2d_ptr->aabb_w.get_midpoint());
            }
            else if ((col3d_ptr = entity_reg.try_get<Base3dCollider>(entity)))
            {
                pos = col3d_ptr->aabb_w.get_midpoint();
            }
            else
                continue;
//                assert(0 && "Sticky note attached to unexpected entity...");

            // Update component here - not the prettiest way :/
            StickyNoteComponent_Update(note, dt);
            if (!note.is_active) continue;

            // Retrieve sticky note content
            char notestr[StickyNoteBufferSize*StickyNoteMaxLen] = "";
            StickyNoteComponent_Dump(note, notestr);
            // Create label
            const std::string win_name = "stickynote" + std::to_string(c++);
            ImguiPrintTextAt(pos,
                             VPProjView,
                             win_h,
                             notestr,
                             win_name.c_str(),
                             Flags.StickyNoteBgColor,
                             Flags.StickyNoteTextColor);
        }
    }
    
    // MARK: Distance Constraint
    //
//    if (flags.ConstraintsOn && flags.DistanceConstraint)
//    {
//        auto view = registry.view<ConstraintComponent<DistanceConstraint>>();
//
//        for(auto entity: view)
//        {
//            auto& ctr = registry.get<ConstraintComponent<DistanceConstraint>>(entity);
//            EntityPairType& rb_pair = ctr.rb_pair.pair;
//            auto& rbA = registry.get<RigidBody3dComponent>(rb_pair.first);
//            auto& rbB = registry.get<RigidBody3dComponent>(rb_pair.second);
//
//            imrend->push_line(rbA.X + rbA.R*ctr.constraint.rA,
//                              rbB.X + rbB.R*ctr.constraint.rB,
//                              v3f {1,1,1});
//        }
//    }
    
    // MARK: BallSocket constraint
    //
//    if (flags.ConstraintsOn && flags.BallSocketConstraint)
//    {
//        auto view = registry.view<ConstraintComponent<BallSocketConstraint>>();
//
//        for(auto entity: view)
//        {
//            auto& ctr = registry.get<ConstraintComponent<BallSocketConstraint>>(entity);
//            EntityPairType& rb_pair = ctr.rb_pair.pair;
//            auto& rbA = registry.get<RigidBody3dComponent>(rb_pair.first);
//            auto& rbB = registry.get<RigidBody3dComponent>(rb_pair.second);
//
//            v3f rAw = rbA.X + rbA.R * ctr.constraint.rA;
//            // Point size: 10-1 for view distance 1-10
//            float rAviewZ = dot( ProjView.row(3), rAw.xyz1() );
//            float size = clamp<float>( lerp<float>(10, 1, rAviewZ/10), 1, 10);
//
//            imrend->push_line(rbA.X, rAw, v3f {1,1,1});
//            imrend->push_line(rbB.X, rAw, v3f {1,1,1});
//            imrend->push_point(rAw, v3f {1,1,1}, size);
//        }
//    }
    
    // MARK: ConeTwist constraint
    //
//    if (flags.ConstraintsOn && flags.ConeTwistConstrant)
//    {
//        auto view = registry.view<ConstraintComponent<ConeTwistConstraint>>();
//
//        for(auto entity: view)
//        {
//            auto& ctr = registry.get<ConstraintComponent<ConeTwistConstraint>>(entity);
//            EntityPairType& rb_pair = ctr.rb_pair.pair;
//            auto& rbA = registry.get<RigidBody3dComponent>(rb_pair.first);
//            //                auto& rbB = registry.get<RigidBodyComponent>(rb_pair.second);
//
//            v3f p = rbA.X + rbA.R * ctr.constraint.rA; // rA & rB may be null vectors
//            float hyp = 0.3; // hypotenuse length
//            float r = hyp * sin(ctr.constraint.cone_max);
//            float h = hyp * cos(ctr.constraint.cone_max);
//            mat4f M =   mat4f::translation(p) * mat4f(ctr.constraint.frameAw) * mat4f::translation(vec3f(0,0,h)) * mat4f::rotation(fPI, 0, 1, 0);
//            imrend->push_cone(h, r, M, BaseColors::Lime);
//            imrend->push_cone(h, r, M, BaseColors::Lime, true);
//        }
//    }
    
    // MARK: Angular motor constraint
    //
//    if (flags.ConstraintsOn && flags.AngularMotorConstraint)
//    {
//        auto view = registry.view<ConstraintComponent<AngularMotorConstraint>>();
//
//        int c = 0;
//        for(auto entity: view)
//        {
//            auto& ctr = registry.get<ConstraintComponent<AngularMotorConstraint>>(entity);
//            EntityPairType& rb_pair = ctr.rb_pair.pair;
//            auto& rbA = registry.get<RigidBody3dComponent>(rb_pair.first);
//            //                auto& rbB = registry.get<RigidBodyComponent>(rb_pair.second);
//
//            const std::string win_name = "angmotor" + std::to_string(c++);
//            ImguiPrintTextAt(rbA.X,
//                             VPProjView,
//                             win_h,
//                             (std::to_string((int)ctr.constraint.get_applied_torque()) + " Nm").c_str(),
//                             win_name.c_str(),
//                             flags.ConstraintLabelBgColor,
//                             flags.ConstraintLabelTextColor);
//        }
//    }
    
    // MARK: Spring dampers (3d)
    //
    if (1) // (flags.ConstraintsOn && flags.AngularMotorConstraint)
    {
        imrend->push_states(DepthTest::True);
        auto view = entity_reg.view<LinearSpringDamper3dComponent, LinearSpringDamperDrawAttribsComponent>();
        for(auto entity: view)
        {
            auto& spring = view.get<LinearSpringDamper3dComponent>(entity);
            if (!spring.is_active) continue;

            auto& drawattr = view.get<LinearSpringDamperDrawAttribsComponent>(entity);
            auto& rbA = entity_reg.get<RigidBody3dComponent>(spring.rb3d_entityA);
            auto& rbB = entity_reg.get<RigidBody3dComponent>(spring.rb3d_entityB);
            v3f vAw = rbA.X + rbA.R * spring.rA;
            v3f vBw = rbB.X + rbB.R * spring.rB;

            imrend->push_states(Color4u {Flags.ColliderShapeColor});
            imrend->push_helix(vAw,
                               vBw,
                               drawattr.r_outer,
                               drawattr.r_inner,
                               drawattr.revs);
            imrend->pop_states<Color4u>();
        }
        imrend->pop_states<DepthTest>();
    }
    
    // MARK: Spring dampers (2d)
    //
//    if (1) // (flags.ConstraintsOn && flags.AngularMotorConstraint)
//    {
//        auto view = registry.view
//        <
//        LinearSpringDamper2dComponent,
//        LinearSpringDamperDrawAttribsComponent
//        >();
//
//        int c = 0;
//        for(auto entity: view)
//        {
//            auto& spring = view.get<LinearSpringDamper2dComponent>(entity);
//            if (!spring.is_active) continue;
//
//            auto& drawattr = view.get<LinearSpringDamperDrawAttribsComponent>(entity);
//            auto& rbA = registry.get<RigidBody2dComponent>(spring.rb2d_entityA);
//            auto& rbB = registry.get<RigidBody2dComponent>(spring.rb2d_entityB);
//            v2f vAw = rbA.X + m2f::rotation(rbA.R) * spring.rA;
//            v2f vBw = rbB.X + m2f::rotation(rbB.R) * spring.rB;
//
//            imrend->push_helix(xy0(vAw),
//                               xy0(vBw),
//                               drawattr.r_outer,
//                               drawattr.r_inner,
//                               drawattr.revs,
//                               drawattr.color);
//        }
//    }
    
#if 0
    // Mesh AABB's
    // - Render mesh AABBs from the Scene's RenderEntities
    //
    if (flags.MeshAABB)
    {
        auto view = registry.view<Handle<MeshBundle>, Handle<SceneGraph::Transform>>();
        
        //int c = 0;
        for(auto entity: view)
        {
            auto& mesh_bundle = registry.get<Handle<MeshBundle>>(entity);
            auto& tfm = registry.get<Handle<SceneGraph::Transform>>(entity)->global_tfm;
            
            for (MeshComponent& meshc : mesh_bundle->meshes)
            {
                // "Shade" the AABB here - have a separate system for this?
                AABB_t aabbw = meshc.aabb.post_transform(tfm);
                // Render AABB
                vec3f size = aabbw.vmax - aabbw.vmin;
                vec3f pos = aabbw.vmin + size*0.5f;
                mat4f M = mat4f::translation(pos) * mat4f::scaling(size);
                imrend->push_cube_wireframe(M, {0.5f, 0.1f, 0.9f});
            }
        }
    }
#endif
    
}

void DebugRendererSystem::render_skeleton_nodes(const std::vector<SkeletonNode>& nodes,
                                                const m4f& W,
//                                                bool render_basis_arrows,
                                                float cyl_radius,
                                                std::shared_ptr<ImPrimitiveRenderer> imrend)
{
    const ArrowDescriptor arrdesc =
    {
        .cone_fraction = 0.1f,
        .cone_radius = cyl_radius*3,
        .cylinder_radius = cyl_radius
    };
    const auto nlcol = Color4u(0xFFFF8080);
    const auto blcol = Color4u(0xFF00FFFF);
//    const v3f nlcol = v3f(0.5,0.5,1);   // node line color
//    const v3f blcol = v3f(1,1,0);       // bone line color
    auto bccol = Color4u::Blue;    // bone cone color
    
    imrend->push_states(DepthTest::False);
    auto node = nodes.begin();
    while(node != nodes.end())
    {
        m4f nodeMx = W * node->global_tfm;
        v3f p0 = extract_translation(nodeMx);
        bool is_bone = (node->bone_index > -1);
        
        imrend->push_states((is_bone ? blcol : nlcol));
        imrend->push_point(p0, 4);
        imrend->pop_states<Color4u>();

//        if (render_basis_arrows)
//        {
//            const mat3f wbasis = nodeMx.get_3x3()*cyl_radius;
//            //dbgrenderer->push_arrow(p0, p0+wbasis.col[0], BaseColors::Red, arrdesc);
//            //dbgrenderer->push_arrow(p0, p0+wbasis.col[1], BaseColors::Lime, arrdesc);
//            //dbgrenderer->push_arrow(p0, p0+wbasis.col[2], BaseColors::Blue, arrdesc);
//        }
        
        // Draw connections to child nodes
        auto cnode = node+1;
        for (int i=0; i<node->m_nbr_children; i++)
        {
            m4f cnodeMx = W * cnode->global_tfm;
            v3f p1 = extract_translation(cnodeMx);
            bool child_is_bone = (cnode->bone_index > -1);
  
            if (is_bone && child_is_bone)
            {
                imrend->push_states(blcol);
                imrend->push_line(p0, p1);
                imrend->pop_states<Color4u>();
                
                imrend->push_states(bccol);
                imrend->push_cone(p0, p1, cyl_radius);
                imrend->pop_states<Color4u>();
            }
            else
            {
                imrend->push_states(nlcol);
                imrend->push_line(p0, p1);
                imrend->pop_states<Color4u>();
            }

            cnode += cnode->m_branch_stride;
        }
        node++;
    }
    imrend->pop_states<DepthTest>();
}

// MARK: --- CameraSystem ---------------------------------------------------

void CameraSystem::init(Scene& scene,
                        entt::dispatcher& dispatcher)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    
    dispatcher.sink<ViewportResizeEvent>().connect<&CameraSystem::viewport_resize>();
}

void CameraSystem::update(Scene& scene,
                          entt::dispatcher& dispatcher,
                          float dt)
{
    // Not just for camera, so don't do here - do in App.update somewhere
    dispatcher.update<ViewportResizeEvent>();
    dispatcher.clear<ViewportResizeEvent>();
    
    const bool key_w = Input::Keyboard->key_pressed(XI_KEY_W);
    const bool key_s = Input::Keyboard->key_pressed(XI_KEY_S);
    const bool key_a = Input::Keyboard->key_pressed(XI_KEY_A);
    const bool key_d = Input::Keyboard->key_pressed(XI_KEY_D);
    const bool key_q = Input::Keyboard->key_pressed(XI_KEY_Q);
    const bool key_e = Input::Keyboard->key_pressed(XI_KEY_E);
    const v2f mouse_xy = v2f {(float)Input::Mouse.x, (float)Input::Mouse.y};
    const bool mouse_left = Input::Mouse.left_pressed();
    const bool mouse_right = Input::Mouse.right_pressed();
    
    // Input -> update Transform of main camera
    //
    auto entity = scene.get_main_camera_entity().entity;
    {
        auto& camera = scene.registry.get<Camera>(entity);
        auto& tfm = scene.registry.get<Transform>(entity);

        // WASDQE
        camera.curvel = v3f_000;
        camera.curvel.z -= camera.vel.z * key_w; // * dt
        camera.curvel.z += camera.vel.z * key_s;
        camera.curvel.x -= camera.vel.x * key_a;
        camera.curvel.x += camera.vel.x * key_d;
        camera.curvel.y -= camera.vel.y * key_q;
        camera.curvel.y += camera.vel.y * key_e;
        
        // Record mouse movement
        camera.mousetrack.diff = mouse_xy - camera.mousetrack.prev;
        camera.mousetrack.prev = mouse_xy;
        
        // Convert mouse movement to transform
        if (mouse_left)
        {
            tfm.rotation.y += -camera.mousetrack.diff.x * camera.trackball.sensitivity;
            tfm.rotation.x += -camera.mousetrack.diff.y * camera.trackball.sensitivity;
            tfm.rotation.x = clamp<float>(tfm.rotation.x, -90.0f, 90.0f);
        }
    }
    
    auto view = scene.registry.view<Camera, Transform>();
    for(auto entity: view)
    {
        auto& camera = view.get<Camera>(entity);
        auto& tfm = view.get<Transform>(entity);
        
        float yaw = tfm.rotation.y * fTO_RAD;
        float pitch = tfm.rotation.x * fTO_RAD;
//        camera.trackball.yaw = tfm.rotation.x;
//        camera.trackball.pitch = tfm.rotation.y;
        
        //
//        camera.updateView(); // uses yaw & pitch; could use tfm.y & tfm.x instead
        // FPV
        camera.position = tfm.position;
        camera.R = tfm.get_R_local(tfm); // mat4f::rotation(0.0f, yaw, pitch);
        camera.RInverse = linalg::transpose(camera.R);
        camera.WorldViewInverse = mat4f::translation(tfm.position) * camera.R;
        camera.WorldView = camera.RInverse * mat4f::translation(-tfm.position);
        
        camera.ProjView = camera.Proj * camera.WorldView;
        camera.ProjView_inverse = camera.WorldViewInverse * camera.ProjInverse;
        
        camera.VPProjView = camera.Viewport * camera.ProjView;
        camera.VPProjView_inverse = camera.ProjView_inverse * camera.ViewportInverse;
        
        // Look-at (override)
//        mat4f L =   mat4f::lookatRHS(tfm.position, v3f{0.0f, 0.0f, 0.0f}, camera.frame.up);
//        mat4f Li =  mat4f::lookatRHS_inverse(tfm.position, v3f{0.0f, 0.0f, 0.0f}, camera.frame.up);
//        camera.WorldViewInverse = Li;
//        camera.WorldView = L;
        
        // Move
        v3f vView = camera.curvel;
        v3f vw = (camera.WorldViewInverse * vView.xyz0()).xyz();
        tfm.position += vw;
        // TODO
        camera.frame.eye = tfm.position;
    }
}

void CameraSystem::primitive_render(Scene& scene,
                                    std::shared_ptr<ImPrimitiveRenderer> renderer)
{
    renderer->push_states(BackfaceCull::False, DepthTest::True, Color4u::Silver);
    auto view = scene.registry.view<Camera>();
    for(auto entity: view)
    {
        auto& camera = view.get<Camera>(entity);
        
        renderer->push_states(camera.WorldViewInverse);
        renderer->push_cone(1.0f, 0.5f);
        renderer->pop_states<m4f>();
    }
    renderer->pop_states<BackfaceCull, DepthTest, Color4u>();
}

// projection_changed (e.g. via UI)

void CameraSystem::viewport_resize(const ViewportResizeEvent& event)
{
    // All cameras?
    // Or just auto camera = event.scene->get_main_camera()
    
    auto view = event.scene->registry.view<Camera>();
    for(auto entity: view)
    {
        auto& camera = view.get<Camera>(entity);
        camera.WindowResizeCallback(event.viewport);
    }
}

// MARK: --- PointLightSystem ---------------------------------------------------

void PointLightSystem::init(Scene& scene,
                        entt::dispatcher& dispatcher)
{
//    std::cout << __PRETTY_FUNCTION__ << std::endl;
    
//    dispatcher.sink<ViewportResizeEvent>().connect<&CameraSystem::viewport_resize>();
}

void PointLightSystem::update(Scene& scene,
                          entt::dispatcher& dispatcher,
                          float dt)
{
    auto view = scene.registry.view<LightSource, Transform>();
    for(auto entity: view)
    {
        auto& light = view.get<LightSource>(entity);
        auto& tfm = view.get<Transform>(entity);
        
        // Update transforms only needed when Transform changes
        // ~ same event as EventUpdateAllSockets ...
        
        light.desc.pos = xyz1(tfm.position);
        light.desc.dir = tfm.local_tfm.column(2) * -1.0f;
        
        light.View = tfm.local_tfm_inverse;
        light.ProjView = light.Proj * light.View;
    }
}

void PointLightSystem::primitive_render(Scene& scene,
                                        std::shared_ptr<ImPrimitiveRenderer> renderer)
{
    renderer->push_states(BackfaceCull::False, DepthTest::True, Color4u::Yellow);
    auto view = scene.registry.view<Transform, LightSource>();
    for(auto entity: view)
    {
        auto& tfm = view.get<Transform>(entity);
        
        renderer->push_states(tfm.local_tfm);
        renderer->push_cone(1.0f, 0.5f);
        renderer->pop_states<m4f>();
    }
    renderer->pop_states<BackfaceCull, DepthTest, Color4u>();
}

// MARK: --- Physics3dSystem ---------------------------------------------------

void Physics3dSystem::updateV(float dt, entt::registry& registry)
{
    auto view = registry.template view<RigidBody3dComponent>();
    
    for(auto entity: view)
    {
        auto& rb = view.get<RigidBody3dComponent>(entity);

        //
        // Integrate velocities from accumulated force at t=ti (explicit integration)
        //
        // Linear velocity from gravity and accumulated linear force
        // V[ti+h] <- V[ti] + (F[ti]/m + g)*h
        //
        // Angular velocity from accumulated torque
        // W[ti+h] <- W[ti] + I^-1*T[ti]*h
        //
        if (!rb.is_static)
        {
            rb.V += (rb.F * rb.im + rb.g - rb.V * rb.V_damp) * dt;
            rb.W += rb.iI_w * ((rb.T - rb.W * rb.W_damp) * dt);
            
            rb.V *= rb.V_mask;
            rb.W *= rb.W_mask;
#if 0
            // clamp W to cap
            f32 W_norm = body->W.norm2();
            if(W_norm > W_CAP)
                body->W = body->W * (W_CAP/W_norm);
#endif
            // reset applied forces
            rb.F.set(0, 0, 0);
            rb.T.set(0, 0, 0);
        }
    }
}

void Physics3dSystem::updateX(float dt, entt::registry& registry)
{
    auto view = registry.template view<RigidBody3dComponent>();
    
    for(auto entity: view)
    {
        auto& rb = view.get<RigidBody3dComponent>(entity);

        // Buffer position & orientation
        rb.X_prev = rb.X;
        rb.Q_prev = rb.Q;
        
        // Update velocity masks
        rb.V *= rb.V_mask;
        rb.W *= rb.W_mask;
        
        //
        // Integrate linear & angular positions from velocities at t=ti+h (implicit integration)
        //
        // Position from linear velocity
        // X[ti+h] <- X[ti] + V[ti+h]*h
        //
        // Orientation from angular velocity
        // Q[ti+h] <- Q[ti] + Qdot(W[ti+h])*h (quaternion form)
        // R[ti+h] <- R[ti] + Rdot(W[ti+h])*h (matrix form)
        //
        if (!rb.is_static)
        {
            rb.X += rb.V * dt;

            // Instead using X += v0*dt + a*t^2 / 2
            // Note that F should not be reset if this is used
            // The second term is small but not zero.
            // Not sure how it fits with Seymplectic/Semi-implicit Euler
//            rb.X += rb.V * dt + (rb.F * rb.im + g - rb.V * rb.V_damp) * dt * dt * 0.5f;
//            rb.F.set(0, 0, 0);
//            rb.T.set(0, 0, 0);
            
            // quaternion orientation
            quatf Qdot = rb.Q.getQdot(rb.W);
            rb.Q += Qdot * dt;
            rb.Q.normalize();
            // auxiliary
            rb.R = m3f(rb.Q);
            rb.Ri = rb.R; rb.Ri.transpose();
        }
        
        rb.iI_w =  rb.R * rb.iI * rb.Ri;
        
#if 1
        // Hack it so that bodies don't fall below a given height
        const float yg = -0.01f - 2.0f;
        if (!rb.is_static)
            if (rb.X.y < yg)
            {
                rb.X.y = yg;
//                if (rb.bounce)
                    rb.V.y = 2.0f;
//                else
//                    rb.V = v3f_zero;
            }
#endif
    }
}

// MARK: --- Physics2dSystem ---------------------------------------------------

void Physics2dSystem::updateV(float dt, entt::registry& registry)
{
    auto view = registry.template view<RigidBody2dComponent>();
    
    for(auto entity: view)
    {
        auto& rb = view.get<RigidBody2dComponent>(entity);

        //
        // Integrate velocities from accumulated force at t=ti (explicit integration)
        //
        // Linear velocity from gravity and accumulated linear force
        // V[ti+h] <- V[ti] + (F[ti]/m + g)*h
        //
        // Angular velocity from accumulated torque
        // W[ti+h] <- W[ti] + I^-1*T[ti]*h
        //
        if (!rb.is_static)
        {
            rb.V += (rb.F * rb.im + rb.g - rb.V * rb.V_damp) * dt;
            rb.W += rb.iI_w * ((rb.T - rb.W * rb.W_damp) * dt);
            
            rb.V *= rb.V_mask;
            rb.W *= rb.W_mask;
#if 0
            // clamp W to cap
            f32 W_norm = body->W.norm2();
            if(W_norm > W_CAP)
                body->W = body->W * (W_CAP/W_norm);
#endif
            // reset applied forces
            rb.F.set(0.0f, 0.0f);
            rb.T = 0.0f;
        }
    }
}

void Physics2dSystem::updateX(float dt, entt::registry& registry)
{
    auto view = registry.template view<RigidBody2dComponent>();
    
    for(auto entity: view)
    {
        auto& rb = view.get<RigidBody2dComponent>(entity);

        // Buffer position & orientation
        rb.X_prev = rb.X;
        rb.R_prev = rb.R;
        
        // Update velocity masks
        rb.V *= rb.V_mask;
        rb.W *= rb.W_mask;
        
        //
        // Integrate linear & angular positions from velocities at t=ti+h (implicit integration)
        //
        // Position from linear velocity
        // X[ti+h] <- X[ti] + V[ti+h]*h
        //
        // Orientation from angular velocity
        // Q[ti+h] <- Q[ti] + Qdot(W[ti+h])*h (quaternion form)
        // R[ti+h] <- R[ti] + Rdot(W[ti+h])*h (matrix form)
        //
        if (!rb.is_static)
        {
            rb.X += rb.V * dt;

            // Instead using X += v0*dt + a*t^2 / 2
            // Note that F should not have been reset if this is used
            // The second term is small but not zero.
            // Not sure how it fits with Symplectic/Semi-implicit Euler
//            rb.X += rb.V * dt + (rb.F * rb.im + g - rb.V * rb.V_damp) * dt * dt * 0.5f;
//            rb.F.set(0, 0, 0);
//            rb.T.set(0, 0, 0);
            
            rb.R += rb.W * dt;
        }
        
        rb.iI_w = rb.iI;
        
#if 1
        // Hack it so that bodies don't fall below a given height
        const float yg = -0.01f - 2.0f;
        if (!rb.is_static)
            if (rb.X.y < yg)
            {
                rb.X.y = yg;
//                if (rb.bounce)
                    rb.V.y = 2.0f;
//                else
//                    rb.V = v2f_zero;
            }
#endif
    }
}

// MARK: --- MouseForce3dSystem ------------------------------------------------

void
MouseForce3dSystem::engage(MouseForce3dComponent& mfc,
                           const RayContact& rayc,
                           const v3f& camera_pos,
                           entt::registry& registry)
{
    auto& lspring = registry.get<LinearSpringDamper3dComponent>(mfc.force_entity);
    auto& aspring = registry.get<AngularSpringDamper3dComponent>(mfc.force_entity);
    
    const v3f r_world = rayc.point_of_contact();
    const v3f r_rel = rayc.rb_r;
    entt::entity engaged_rb_entity = static_cast<entt::entity>(rayc.rb_entityu);
    
    if (lspring.K > 0.0f || lspring.D > 0.0f)
    {
        lspring.rb3d_entityB = engaged_rb_entity;
        lspring.rA = r_world;    // Anchor relative RB of ref entity (world space)
        lspring.rB = r_rel;      // Anchor relative RB of engaged entity
        lspring.is_active = true;
    }

    if (aspring.K > 0.0f || aspring.D > 0.0f)
    {
        aspring.rb3d_entityB = engaged_rb_entity;
        aspring.is_active = true;
        
        // Set offset rotation at moment of engagement
        auto& aspring_rbA = registry.get<RigidBody3dComponent>(aspring.rb3d_entityA);
        auto& aspring_rbB = registry.get<RigidBody3dComponent>(aspring.rb3d_entityB);
        aspring.R = aspring_rbB.R * transpose(aspring_rbA.R); // Initial rot. A -> B
    }
    
    mfc.last_camera_ray = rayc;
    mfc.dist = (r_world - camera_pos).norm2();
    mfc.is_engaged = true;
}

void
MouseForce3dSystem::disengage(MouseForce3dComponent& mfc,
                              entt::registry& registry)
{
    auto& lspring = registry.get<LinearSpringDamper3dComponent>(mfc.force_entity);
    auto& aspring = registry.get<AngularSpringDamper3dComponent>(mfc.force_entity);
    
    lspring.rb3d_entityB = aspring.rb3d_entityB = entt::null;
    lspring.is_active = aspring.is_active = false;
    
    mfc.is_engaged = false;
}

void
MouseForce3dSystem::update(const Camera& camera,
                           entt::registry& registry)
{
    auto view = registry.view<MouseForce3dComponent>();
    
    for(auto entity: view)
    {
        auto& mfc = view.get<MouseForce3dComponent>(entity);
        
        if (!Input::Mouse.right_pressed())
        {
            disengage(mfc, registry);
            return;
        }
        
        Ray camera_ray = camera.world_ray_from_window_coords(Input::Mouse.x,
                                                             Input::Mouse.y);

        if (!mfc.is_engaged)
        {
            auto rayc = RayContact {camera_ray, CollisionLayer::Raycast};
            gCollision3dSystem->raycast(rayc, registry);
            
            if (rayc.isRigidBodyColliderContact())
                engage(mfc,
                       rayc,
                       camera.position,
                       registry);
        }
        
        if (mfc.is_engaged)
        {
            mfc.dist = fmaxf(1.0f, mfc.dist + Input::Mouse.scroll_dy);
            // TODO: Determine where/when to reset scroll
            Input::Mouse.scroll_dy = 0;
            
            auto& lspring = registry.get<LinearSpringDamper3dComponent>(mfc.force_entity);
            lspring.rA = camera_ray.origin + camera_ray.dir * mfc.dist;
        }
    }
}

entt::entity
MouseForce3dSystem::spawn(float K_lin,
                          float D_lin,
                          float K_ang,
                          float D_ang,
                          entt::registry& registry)
{
    MouseForce3dComponent mfc;
 
    // Create reference RB entity
    entt::entity rb_entity_ref = registry.create();
    RigidBody3dComponent rb;
//    rb.control_tfm = false; // TODO: Legacy. Remove.
    RigidBody::make_static(rb);
    registry.emplace<RigidBody3dComponent>(rb_entity_ref, rb);
    
    // Create spring damper entity and attach a spring damper force to it
    LinearSpringDamper3dComponent lspring
    {
        .rb3d_entityA = rb_entity_ref,
        .rb3d_entityB = entt::null, // This will be the grabbed entity
        .is_active = false,
        .K = K_lin,
        .D = D_lin,
        .L = 0.0f
    };
    AngularSpringDamper3dComponent aspring
    {
        .rb3d_entityA = rb_entity_ref,
        .rb3d_entityB = entt::null, // This will be the grabbed entity
        .is_active = false,
        .K = K_ang,
        .D = D_ang,
        .R = m3f_1
    };
    mfc.force_entity = registry.create();
    registry.emplace<LinearSpringDamper3dComponent>(mfc.force_entity, lspring);
    registry.emplace<AngularSpringDamper3dComponent>(mfc.force_entity, aspring);
    //
    // Draw attributes
    LinearSpringDamperDrawAttribsComponent ldraw_attribs
    {
        .r_outer = 0.1f,
        .r_inner = 0.01f,
        .revs = 8,
        .color = Color4u::Yellow
    };
    AngularSpringDamperDrawAttribsComponent adraw_attribs
    {
    };
    registry.emplace<LinearSpringDamperDrawAttribsComponent>(mfc.force_entity, ldraw_attribs);
    registry.emplace<AngularSpringDamperDrawAttribsComponent>(mfc.force_entity, adraw_attribs);
    
    // Create main entity
    entt::entity mfc_entity = registry.create();
    registry.emplace<MouseForce3dComponent>(mfc_entity, mfc);
    return mfc_entity;
}

// MARK: --- MouseForce2dSystem ------------------------------------------------

void
MouseForce2dSystem::engage(MouseForce2dComponent& mfc,
                           const RayContact& rayc,
                           const v3f& camera_pos,
                           entt::registry& registry)
{
    auto& lspring = registry.get<LinearSpringDamper2dComponent>(mfc.force_entity);
    auto& aspring = registry.get<AngularSpringDamper2dComponent>(mfc.force_entity);
    
    const v3f r_world = rayc.point_of_contact();
    const v2f r_rel = xy(rayc.rb_r);
    entt::entity engaged_rb_entity = static_cast<entt::entity>(rayc.rb_entityu);
    
    if (lspring.K > 0.0f || lspring.D > 0.0f)
    {
        lspring.rb2d_entityB = engaged_rb_entity;
        lspring.rA = xy(r_world);   // Anchor relative RB of ref entity (world space)
        lspring.rB = r_rel;         // Anchor relative RB of engaged entity
        lspring.is_active = true;
    }

    if (aspring.K > 0.0f || aspring.D > 0.0f)
    {
        aspring.rb2d_entityB = engaged_rb_entity;
        aspring.is_active = true;
        
        // Set offset rotation at moment of engagement
        auto& aspring_rbA = registry.get<RigidBody2dComponent>(aspring.rb2d_entityA);
        auto& aspring_rbB = registry.get<RigidBody2dComponent>(aspring.rb2d_entityB);
        aspring.R =  aspring_rbB.R - aspring_rbA.R;
    }
    
    mfc.last_camera_ray = rayc;
    mfc.is_engaged = true;
}

void
MouseForce2dSystem::disengage(MouseForce2dComponent& mfc,
                              entt::registry& registry)
{
    auto& lspring = registry.get<LinearSpringDamper2dComponent>(mfc.force_entity);
    auto& aspring = registry.get<AngularSpringDamper2dComponent>(mfc.force_entity);
    
    lspring.rb2d_entityB = aspring.rb2d_entityB = entt::null;
    lspring.is_active = aspring.is_active = false;
    
    mfc.is_engaged = false;
}

void
MouseForce2dSystem::update(const Camera& camera,
                           entt::registry& registry)
{
    auto view = registry.view<MouseForce2dComponent>();
    
    for(auto entity: view)
    {
        auto& mfc = view.get<MouseForce2dComponent>(entity);
        
        if (!Input::Mouse.right_pressed())
        {
            disengage(mfc, registry);
            return;
        }
        
        Ray camera_ray = camera.world_ray_from_window_coords(Input::Mouse.x,
                                                             Input::Mouse.y);
        
        if (!mfc.is_engaged)
        {
            auto rayc = RayContact {camera_ray, CollisionLayer::Raycast};
            gCollision2dSystem->raycast(rayc, registry);
            
            if (rayc.isRigidBody2dColliderContact())
                engage(mfc,
                       rayc,
                       camera.position,
                       registry);
        }
        
        if (mfc.is_engaged)
        {
            auto scroll_y = Input::Mouse.scroll_dy;
            if (scroll_y != 0.0)
            {
                auto& aspring = registry.get<AngularSpringDamper2dComponent>(mfc.force_entity);
                aspring.R += scroll_y;
                // TODO: Determine where/when to reset scroll
                Input::Mouse.scroll_dy = 0;
            }
            
            // Set reference anchor point to where the camera ray intersects
            // the xy-plane.
            auto& lspring = registry.get<LinearSpringDamper2dComponent>(mfc.force_entity);
            float t_at_xy_plane = -camera_ray.origin.z/camera_ray.dir.z;
            lspring.rA = xy(camera_ray.origin + camera_ray.dir * t_at_xy_plane);
        }
    }
}

entt::entity
MouseForce2dSystem::spawn(float K_lin,
                          float D_lin,
                          float K_ang,
                          float D_ang,
                          entt::registry& registry)
{
    MouseForce2dComponent mfc;
 
    // Create reference RB entity
    entt::entity rb_entity_ref = registry.create();
    RigidBody2dComponent rb;
    rb.control_tfm = false; // TODO: Legacy. Remove.
    RigidBody::make_static(rb);
    registry.emplace<RigidBody2dComponent>(rb_entity_ref,
                                           rb);
    
    // Create spring damper entity and attach spring dampers to it
    LinearSpringDamper2dComponent lspring
    {
        .rb2d_entityA = rb_entity_ref,
        .rb2d_entityB = entt::null, // This will be the grabbed entity
        .is_active = false,
        .K = K_lin,
        .D = D_lin,
        .L = 0.0f
    };
    AngularSpringDamper2dComponent aspring
    {
        .rb2d_entityA = rb_entity_ref,
        .rb2d_entityB = entt::null, // This will be the grabbed entity
        .is_active = false,
        .K = K_ang,
        .D = D_ang,
        .R = 0.0f
    };
    mfc.force_entity = registry.create();
    registry.emplace<LinearSpringDamper2dComponent>(mfc.force_entity, lspring);
    registry.emplace<AngularSpringDamper2dComponent>(mfc.force_entity, aspring);
    //
    // Draw attributes
    LinearSpringDamperDrawAttribsComponent ldraw_attribs
    {
        .r_outer = 0.05f,
        .r_inner = 0.005f,
        .revs = 8,
        .color = Color4u::Yellow
    };
    AngularSpringDamperDrawAttribsComponent adraw_attribs
    {
    };
    registry.emplace<LinearSpringDamperDrawAttribsComponent>(mfc.force_entity, ldraw_attribs);
    registry.emplace<AngularSpringDamperDrawAttribsComponent>(mfc.force_entity, adraw_attribs);
    
    // Create main entity
    entt::entity mfc_entity = registry.create();
    registry.emplace<MouseForce2dComponent>(mfc_entity,
                                            mfc);
    return mfc_entity;
}
