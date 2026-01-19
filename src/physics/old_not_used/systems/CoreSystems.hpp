//
//  CoreSystems.hpp
//  editor
//
//  Created by Carl Johan Gribel on 2023-02-28.
//  Copyright © 2023 Carl Johan Gribel. All rights reserved.
//

#ifndef CoreSystems_hpp
#define CoreSystems_hpp

#include <stdio.h>
#include "vec.h"
#include "mat.h"
#include "ResourceRegistry.hpp"
#include "CollisionSystem.hpp"

namespace ImPrimitiveRendererNS { class ImPrimitiveRenderer; }
using namespace ImPrimitiveRendererNS;
using namespace linalg;

class Camera;
class Scene;
class ViewportResizeEvent; // AppEvents.h??
//using ScenePtr = std::shared_ptr<Scene>;

// MARK: --- CollisionLayer (Game-side???) -------------------------------------

struct CollisionLayer
{
    static inline const uint Default    = 0;
    static inline const uint Ground     = 1;
    static inline const uint Sector     = 2;
    static inline const uint Raycast    = 3;
    static inline const uint Custom1    = 4;
    static inline const uint Custom2    = 5;
    static inline const uint Custom3    = 6;
    static inline const uint Custom4    = 7;
    
    static inline std::unordered_map<std::string, uint> FromName =
    {
        {"default", Default},
        {"ground",  Ground},
        {"sector",  Sector},
        {"raycast", Raycast},
        {"custom1", Custom1},
        {"custom2", Custom2},
        {"custom3", Custom3},
        {"custom4", Custom4}
    };
};

struct SkeletonTraversalSystem
{
    static void update(float t,
                       float dt,
                       entt::registry& entity_reg,
                       ResourceRegistry& resource_reg);
};

// MARK: --- DebugRendererSystem -----------------------------------------------

struct DebugRenderFlags
{
    bool SceneOn = true;
    
//    bool HeaderOn = true;
    
    bool TransformOn = true;
    bool TransformCubes = false;
    bool TransformAxes = false;
    bool TransformLabels = false;
    unsigned TransformLabelTextColor = 0xffffffff;
    unsigned TransformLabelBgColor = 0x80000000;
//    long long TransformRenderDuration = 0;
    
    bool RigidBodyOn = true;
    bool RigidBodyAxes = true;
    bool RigidBodyCubes = true;
    bool RigidBodyLabels = false;
    unsigned RigidBodyLabelTextColor = 0xffffffff; //0xff000000;
    unsigned RigidBodyLabelBgColor = 0x80ff8080;
//    long long RigidBodyRenderDuration = 0;
    
    bool ColliderOn = true;
    bool ColliderAxes = true;
    bool ColliderShape = true;
    bool ColliderFaceNormals = false;
    bool ColliderLabels = false;
    bool ColliderAABB = false;
    unsigned ColliderShapeColor = Color4u::Magenta;
    unsigned ColliderFaceNormalColor = Color4u::Green;
    unsigned ColliderLabelTextColor = 0xffffffff; //0xff000000;
    unsigned ColliderLabelBgColor = 0x80ff00ff;
    unsigned ColliderAABBColor = 0xffE6E6FA;
//    long long ColliderRenderDuration = 0;

    bool SkeletonOn = true;
    bool SkeletonNodes = true;
    bool SkeletonNodeAxes = false;
    bool SkeletonNodeLabels = false;
    unsigned SkeletonNodeLabelTextColor = 0xff000000;
    unsigned SkeletonNodeLabelBgColor = 0x40ffffff;
    unsigned SkeletonBoneLabelBgColor = 0x4000ffff;
    
    bool ConstraintsOn = true;
    bool BallSocketConstraint = true;
    bool ConeTwistConstrant = true;
    bool AngleConstraint = true;
    bool DistanceConstraint = true;
    bool PrismaticConstraint = true;
    bool AngularMotorConstraint = true;
    bool LinearMotorConstraint = true;
    unsigned ConstraintColor = 0xffffffff;
    unsigned ConstraintLabelTextColor = 0xff202020;
    unsigned ConstraintLabelBgColor = 0x80ffffff;

    unsigned SelectionLabelTextColor = 0xff000000;
    unsigned SelectionLabelBgColor = 0x80ffffff;
    
    bool StickyNotes = true;
    unsigned StickyNoteTextColor = 0xff000000;
    unsigned StickyNoteBgColor = 0xffa5ffff;
    
    // Contact & collision stuff
    bool RigidBodyColliderPairs = true;
    bool TriggeredColliderPairs = true;
    bool CachedRays = true;
    
    bool MeshAABB = false;
    bool ViewFrusta = false;
    bool LightFrusta = false;
    
    bool BoundingSpheres = false; // ?
    
};

void ImguiPrintTextAt(const v3f& world_pos,
                      const m4f& VP_PROJ_MV,
                      const int win_h,
                      const char* str,
                      const char* window_name,
                      const unsigned color_bg,
                      const unsigned color_text);

class DebugRendererSystem
{
public:
    
    static DebugRenderFlags Flags;
    
    static void update(float t,
                       float dt,
                       int long frame_number,
                       entt::registry& registry,
                       ResourceRegistry& resource_reg,
                       std::shared_ptr<ImPrimitiveRenderer> imrend,
                       RayContact& camera_ray,
//                       DebugRenderFlags& flags,
                       const m4f& ProjView,
                       const m4f& VPProjView,
                       const int win_h);
private:
    
    static void render_skeleton_nodes(const std::vector<SkeletonNode>& nodes,
                                      const m4f& W,
//                                      bool render_basis_arrows,
                                      float cyl_radius,
                                      std::shared_ptr<ImPrimitiveRenderer> imrend);
};

// MARK: --- CameraSystem ------------------------------------------------------

struct CameraSystem
{
    static void init(Scene& scene,
                     entt::dispatcher& dispatcher);
    
    static void update(Scene& scene,
                       entt::dispatcher& dispatcher,
                       float dt);
    
    static void primitive_render(Scene& scene,
                                 std::shared_ptr<ImPrimitiveRendererNS::ImPrimitiveRenderer> renderer);
    
    static void viewport_resize(const ViewportResizeEvent& event);
    
    static void test(int x) 
    {
        std::cout << "VALUE " << x << std::endl;
    }
};

// MARK: --- PointLightSystem ------------------------------------------------------

struct PointLightSystem
{
    static void init(Scene& scene,
                     entt::dispatcher& dispatcher);
    
    static void update(Scene& scene,
                       entt::dispatcher& dispatcher,
                       float dt);
    
    static void primitive_render(Scene& scene,
                                 std::shared_ptr<ImPrimitiveRendererNS::ImPrimitiveRenderer> renderer);
};

// MARK: --- Physics3dSystem ---------------------------------------------------

struct Physics3dSystem
{
    static void updateV(float dt,
                        entt::registry& registry);
    
    static void updateX(float dt,
                        entt::registry& registry);
};

// MARK: --- Physics2dSystem ---------------------------------------------------

struct Physics2dSystem
{
    static void updateV(float dt,
                        entt::registry& registry);
    
    static void updateX(float dt,
                        entt::registry& registry);
};

// MARK: --- MouseForce3dComponent ---------------------------------------------

struct MouseForce3dComponent
{
    entt::entity force_entity;
    RayContact last_camera_ray;
    float dist;
    bool is_engaged = false;
};

// MARK: --- MouseForce3dComponent ---------------------------------------------

struct MouseForce2dComponent
{
    entt::entity force_entity;
    RayContact last_camera_ray;
//    float dist; // TODO: dist not in 2d version
    bool is_engaged = false;
};

// MARK: --- MouseForce3dSystem ------------------------------------------------

class MouseForce3dSystem
{
    static void engage(MouseForce3dComponent& mfc,
                       const RayContact& rayc,
                       const v3f& camera_pos,
                       entt::registry& registry);
    
    static void disengage(MouseForce3dComponent& mfc,
                          entt::registry& registry);
    
public:
    
    static void update(const Camera& camera,
                       entt::registry& registry);
    
    // Note: does not use the scene's EntityRegistry
    static entt::entity spawn(float K_lin,
                              float D_lin,
                              float K_ang,
                              float D_ang,
                              entt::registry& registry);
};

// MARK: --- MouseForce2dSystem ------------------------------------------------

class MouseForce2dSystem
{
    static void engage(MouseForce2dComponent& mfc,
                       const RayContact& rayc,
                       const v3f& camera_pos,
                       entt::registry& registry);
    
    static void disengage(MouseForce2dComponent& mfc,
                          entt::registry& registry);
    
public:
    
    static void update(const Camera& camera,
                       entt::registry& registry);
    
    // Note: does not use the scene's EntityRegistry
    static entt::entity spawn(float K_lin,
                              float D_lin,
                              float K_ang,
                              float D_ang,
                              entt::registry& registry);
};

#endif /* CoreSystems_hpp */
