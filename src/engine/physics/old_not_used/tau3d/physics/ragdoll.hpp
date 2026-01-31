//
//  blockman_ragdollv1.hpp
//  tau3d
//
//  Created by Carl Johan Gribel on 2017-02-02.
//
//

#ifndef ragdoll_hpp
#define ragdoll_hpp

#include <stdio.h>
#include <unordered_map>
#include "config.h"
#include "body.h"
#include "world.h"
#include "sim_mesh.h"
#include "skinning.h"
#include "ballsocket_constraint.h"
#include "conetwist_constraint.h"
#include "angle_constraint.h"


namespace ragdoll
{
    //
    // Basic ragdoll
    //
    // 15 limbs
    //      Octagon (head), box (feet), custom polyhedra (hands, Maya obj),
    //      and polygonal capsules (everything else, Maya obj)
    //
    // 19 positional joints
    // 14 kinematic joints, consisting of
    //      14 positional constraints (ball-socket)
    //      14 angular constraints (4 hinge + 10 cone-twist)
    //
    // [limbs] and (joints) are organized relationally, starting at the waist.
    //
    //                    (jheadtip)
    //                        |
    //                     [lhead]
    //                        |
    //  (jshoulderR) ----- (jneck) ----- ...
    //      |                 |
    //   [lupperarmR]      [ltorso]
    //      |                 |
    //   (jelbowR)         (jwaist) = start position
    //      |                 |
    //   [lforearmR]          |
    //      |             [lpelvis]
    //   (jwristR)         /     \
    //      |         (jipR)     ...
    //   [lhandR]       |
    //               [lthighR]
    //                  |
    //               (jkneeR)
    //                  |
    //               [lcalfR]
    //                  |
    //               (jwristR)
    //                  |
    //               (jheelR) - [lfootR] - (jtoeR)
    //
    
    //
    // update, receive events -- inherit from ui_bodyset_t
    // - or wait until GLFW is used
    //
    
    enum RAGDOLL_JOINTS
    {
        // Upper body
        JT_WAIST = 0,
        JT_NECK, JT_HEADTIP,
        // Arms
        JT_LSHOULDER, JT_RSHOULDER,
        JT_LELBOW, JT_RELBOW,
        JT_LWRIST, JT_RWRIST,
        JT_LPALM, JT_RPALM,
        // Lower body
        JT_LHIP, JT_RHIP,
        JT_LKNEE, JT_RKNEE,
        JT_LANKLE, JT_RANKLE,
        JT_LHEEL, JT_RHEEL,
        JT_LTOE, JT_RTOE,
        //
        JT_COUNT
    };
    enum RAGDOLL_LIMBS
    {
        // Upper body
        LB_PELVIS = 0,
        LB_TORSO,
        LB_HEAD,
        // Arms
        LB_LUPPERARM, LB_RUPPERARM,
        LB_LFOREARM, LB_RFOREARM,
        LB_LHAND, LB_RHAND,
        // Lower body
        LB_LTHIGH, LB_RTHIGH,
        LB_LCALF, LB_RCALF,
        LB_LFOOT, LB_RFOOT,
        //
        LB_COUNT
    };
    
    class basic_ragdollv1_t
    {
        std::vector<body_t*> limbs;
//        std::vector<constraint_t*> joints;
        
//        std::vector<vec3f> limbpos = std::vector<vec3f>(LB_COUNT);
        
    public:
        
        void reset_to_bindpos()
        {
            
        }
        
//        void reset()
//        {
            
//        }
        
        // move (waist) to pos
        //
        void set_position()
        {
            // Pre-todo
            // Store limb bodies & joints in arrays (indexed by enum)
            //
            // Step 2
            // Also store vector<vec3> with limb bind pos? I.e. lxxxx below & the provided p (bind pos)
            // Might require bind-rotation as well (if used) -- if rest-operation involves only setting body rotations to identity
            
            // wv = waist movement vector = pos - waist.X
            
            // for all limbs
            //    dp = current vector limb.X <- waist.X
            //    translate limb by wv + dp
        }
        
//        void set_velocity()
//        {
//        
//        }
        
        basic_ragdollv1_t(const vec3f& p, const mat3f& frame, const vec3f& v, world_t* world, sim_mesh_factory_t *factory)
        {
            std::string capsule_path = EXTASSETPATH("maya_rigs/polycapsule/polycapsule.obj");
            std::string hand_path = EXTASSETPATH("maya_rigs/hand_collider/hand_collider.obj");
            
            std::vector<vec3f> jointpos;
            jointpos.resize(JT_COUNT);
            
            //
            // Build skeleton joints & limbs
            //
            
            // Waist -> Head
            vec3f jwaist = {0,0,0}; // p;
            vec3f jneck = jwaist + vec3f(0,0.8,0);
            vec3f jheadtip = jneck + vec3f(0,0.6,0);
            
            vec3f ltorso = jwaist + (jneck-jwaist)*0.5f;
            vec3f lhead = jneck + (jheadtip-jneck)*0.5f;
            
            // Waist -> Ankles
            vec3f jhipL = jwaist + vec3f(0.25, -0.5f, 0);
            vec3f jhipR = jwaist + vec3f(-0.25, -0.5f, 0);
            vec3f jkneeL = jhipL + vec3f(0, -0.7f, 0);
            vec3f jkneeR = jhipR + vec3f(0, -0.7f, 0);
            vec3f jankleL = jkneeL + vec3f(0, -0.6f, 0);
            vec3f jankleR = jkneeR + vec3f(0, -0.6f, 0);
            
            vec3f lpelvis = jwaist + vec3f(0, (jhipR-jwaist).y*0.5f, 0);
            vec3f lthighL = jhipL + (jkneeL-jhipL)*0.5f;
            vec3f lthighR = jhipR + (jkneeR-jhipR)*0.5f;
            vec3f lcalfL = jkneeL + (jankleL-jkneeL)*0.5f;
            vec3f lcalfR = jkneeR + (jankleR-jkneeR)*0.5f;
            
            // Ankles -> Toes
            vec3f jheelL = jankleL + vec3f(0, -0.05, -0.05);
            vec3f jheelR = jankleR + vec3f(0, -0.05, -0.05);
            vec3f jtoeL = jheelL + vec3f(0, 0, 0.4);
            vec3f jtoeR = jheelR + vec3f(0, 0, 0.4);
            
            vec3f lfootL = jheelL + (jtoeL-jheelL)*0.5f;
            vec3f lfootR = jheelR + (jtoeR-jheelR)*0.5f;
            
            // Chest -> Hands
            vec3f jshoulderL = jneck + vec3f(0.5, -0.1, 0);
            vec3f jshoulderR = jneck + vec3f(-0.5, -0.1, 0);
            vec3f jelbowL = jshoulderL + vec3f(0.6, 0, 0);
            vec3f jelbowR = jshoulderR + vec3f(-0.6, 0, 0);
            vec3f jwristL = jelbowL + vec3f(0.4, 0, 0);
            vec3f jwristR = jelbowR + vec3f(-0.4, 0, 0);
            vec3f jpalmL = jwristL + vec3f(0.3, 0, 0);
            vec3f jpalmR = jwristR + vec3f(-0.3, 0, 0);
            
            vec3f lupperarmL = jshoulderL + (jelbowL-jshoulderL)*0.5f;
            vec3f lupperarmR = jshoulderR + (jelbowR-jshoulderR)*0.5f;
            vec3f lforearmL = jelbowL + (jwristL-jelbowL)*0.5f;
            vec3f lforearmR = jelbowR + (jwristR-jelbowR)*0.5f;
            vec3f lhandL = jwristL + (jpalmL-jwristL)*0.5f;
            vec3f lhandR = jwristR + (jpalmR-jwristR)*0.5f;
            
            //
            // Create physical limbs and joints
            //
            
            // (Waist) - Torso - (Neck)
//            octagon_t* torso = new octagon_t( ltorso, {0.75f, (jneck-jwaist).y, 0.4f}, "torso");
            obj_body_t* torso = new obj_body_t(ltorso, capsule_path, "torso", mat4f::scaling({0.65f, (jneck-jwaist).y, 0.3f}));
            
            // (Neck) - Head - (Headtip)
            octagon_t* head = new octagon_t( lhead, {0.35f, (jheadtip-jneck).y, 0.35f}, "head");
            
            // Neck joint
            auto neck_pctr = new ballsocket_constraint_t(torso, head, jneck-ltorso, jneck-lhead);
            auto neck_actr = new conetwist_constraint_t(torso, head, neck_pctr);
            neck_actr->set_limits(40*fTO_RAD, 60*fTO_RAD, mat3f::rotation(-fPI/2, 1, 0, 0), mat3f::rotation(-fPI/2, 1, 0, 0));
            
            // (Waist) - Pelvis - (Hips)
//            octagon_t* pelvis = new octagon_t( lpelvis, {0.6f, (jwaist-jhipL).y, 0.3f}, "pelvis");
            obj_body_t* pelvis = new obj_body_t(lpelvis, capsule_path, "pelvis", mat4f::scaling({0.5f, (jwaist-jhipL).y, 0.3f}));
            
            // Waist joint
            auto waist_pctr = new ballsocket_constraint_t(torso, pelvis, jwaist-ltorso, jwaist-lpelvis);
            auto waist_actr = new conetwist_constraint_t(torso, pelvis, waist_pctr);
            waist_actr->set_limits(20*fTO_RAD, 20*fTO_RAD, mat3f::rotation(fPI/2, 1, 0, 0), mat3f::rotation(fPI/2, 1, 0, 0));
            
            // (Hip) - Thigh - (Knee)
            obj_body_t* lthigh = new obj_body_t(lthighL, capsule_path, "lthigh", mat4f::scaling({0.2f, (jhipL-jkneeL).y, 0.2f}));
            obj_body_t* rthigh = new obj_body_t(lthighR, capsule_path, "rthigh", mat4f::scaling({0.2f, (jhipR-jkneeR).y, 0.2f}));
            
            // Hip joints
            auto lhip_pctr = new ballsocket_constraint_t(pelvis, lthigh, jhipL-lpelvis, jhipL-lthighL);
            auto lhip_actr = new conetwist_constraint_t(pelvis, lthigh, lhip_pctr);
            lhip_actr->set_limits(55*fTO_RAD, 20*fTO_RAD, mat3f::rotation(-fPI/2 - 55*fTO_RAD + fPI, 1, 0, 0), mat3f::rotation(-fPI/2 + fPI, 1, 0, 0));
            
            auto rhip_pctr = new ballsocket_constraint_t(pelvis, rthigh, jhipR-lpelvis, jhipR-lthighR);
            auto rhip_actr = new conetwist_constraint_t(pelvis, rthigh, rhip_pctr);
            rhip_actr->set_limits(55*fTO_RAD, 20*fTO_RAD, mat3f::rotation(-fPI/2 - 55*fTO_RAD + fPI, 1, 0, 0), mat3f::rotation(-fPI/2 + fPI, 1, 0, 0));
            
            // (Knee) - Calf - (Ankle)
            obj_body_t* lcalf = new obj_body_t(lcalfL, capsule_path, "lcalf", mat4f::scaling({0.15f, (jkneeL-jankleL).y, 0.15f}));
            obj_body_t* rcalf = new obj_body_t(lcalfR, capsule_path, "rcalf", mat4f::scaling({0.15f, (jkneeR-jankleR).y, 0.15f}));
            
            // Knee joints
            auto lknee_pctr = new ballsocket_constraint_t(lthigh, lcalf, jkneeL-lthighL, jkneeL-lcalfL);
            auto lknee_actr = new angle_constraint_t(lthigh, lcalf);
            lknee_actr->set_limits(50*fTO_RAD,
                                   mat3f::rotation(90*fTO_RAD, 0, 1, 0) * mat3f::rotation(50*fTO_RAD, 0, 0, 1),
                                   mat3f::rotation(90*fTO_RAD, 0, 1, 0));

            auto rknee_pctr = new ballsocket_constraint_t(rthigh, rcalf, jkneeR-lthighR, jkneeR-lcalfR);
            auto rknee_actr = new angle_constraint_t(rthigh, rcalf);
            rknee_actr->set_limits(50*fTO_RAD,
                                   mat3f::rotation(90*fTO_RAD, 0, 1, 0) * mat3f::rotation(50*fTO_RAD, 0, 0, 1),
                                   mat3f::rotation(90*fTO_RAD, 0, 1, 0));
            
            // (Ankle) - Foot - (Foottip)
            box_body_t* lfoot = new box_body_t( lfootL, {0.2f, (jankleL-jheelL).y*2, (jtoeL-jheelL).z}, "lfoot");
            box_body_t* rfoot = new box_body_t( lfootR, {0.2f, (jankleL-jheelL).y*2, (jtoeR-jheelR).z}, "rfoot");
            
            // Ankle joints
            auto lankle_pctr = new ballsocket_constraint_t(lcalf, lfoot, jankleL-lcalfL, jankleL-lfootL);
            auto lankle_actr = new conetwist_constraint_t(lcalf, lfoot, lankle_pctr);
            lankle_actr->set_limits(30*fTO_RAD, 20*fTO_RAD, mat3f::rotation(-fPI/2*0 - 55*fTO_RAD*0, 1, 0, 0), mat3f::rotation(-fPI/2*0, 1, 0, 0));
            
            auto rankle_pctr = new ballsocket_constraint_t(rcalf, rfoot, jankleR-lcalfR, jankleR-lfootR);
            auto rankle_actr = new conetwist_constraint_t(rcalf, rfoot, rankle_pctr);
            rankle_actr->set_limits(30*fTO_RAD, 20*fTO_RAD, mat3f::rotation(-fPI/2*0 - 55*fTO_RAD*0, 1, 0, 0), mat3f::rotation(-fPI/2*0, 1, 0, 0));
            
            // (Neck) - Upper arm - (Elbow)
            obj_body_t* lupperarm = new obj_body_t(lupperarmL, capsule_path, "lupperarm",
                                                   mat4f::rotation(fPI/2, 0, 0, 1) * mat4f::scaling({0.15f, (jelbowL-jshoulderL).x, 0.15f}));
            obj_body_t* rupperarm = new obj_body_t(lupperarmR, capsule_path, "rupperarm",
                                                   mat4f::rotation(fPI/2, 0, 0, 1) * mat4f::scaling({0.15f, (jshoulderR-jelbowR).x, 0.15f}));
            
            // Shoulder joint
            auto lshoulder_pctr = new ballsocket_constraint_t(torso, lupperarm, jshoulderL-ltorso, jshoulderL-lupperarmL);
            auto lshoulder_actr = new conetwist_constraint_t(torso, lupperarm, lshoulder_pctr);
            lshoulder_actr->set_limits(80*fTO_RAD, 80*fTO_RAD, mat3f::rotation(fPI/2 - 45*fTO_RAD, 0, 1, 0), mat3f::rotation(fPI/2, 0, 1, 0));
            
            auto rshoulder_pctr = new ballsocket_constraint_t(torso, rupperarm, jshoulderR-ltorso, jshoulderR-lupperarmR);
            auto rshoulder_actr = new conetwist_constraint_t(torso, rupperarm, rshoulder_pctr);
            rshoulder_actr->set_limits(80*fTO_RAD, 80*fTO_RAD, mat3f::rotation(-fPI/2 + 45*fTO_RAD, 0, 1, 0), mat3f::rotation(-fPI/2, 0, 1, 0));
            
            
            // (Elbow) - Forearm - (Wrist)
            obj_body_t* lforearm = new obj_body_t(lforearmL, capsule_path, "lforearm",
                                                  mat4f::rotation(fPI/2, 0, 0, 1) * mat4f::scaling({0.15f, (jwristL-jelbowL).x, 0.15f}));
            obj_body_t* rforearm = new obj_body_t(lforearmR, capsule_path, "rforearm",
                                                  mat4f::rotation(fPI/2, 0, 0, 1) * mat4f::scaling({0.15f, (jelbowR-jwristR).x, 0.15f}));
            
            // Elbow joints
            //
            auto lelbow_pctr = new ballsocket_constraint_t(lupperarm, lforearm, jelbowL-lupperarmL, jelbowL-lforearmL);
            auto lelbow_actr = new angle_constraint_t(lupperarm, lforearm);
            lelbow_actr->set_limits(60*fTO_RAD,
                                    mat3f::rotation(60*fTO_RAD, 0, 0, 1),
                                    linalg::mat3f_identity);
            
            auto relbow_pctr = new ballsocket_constraint_t(rupperarm, rforearm, jelbowR-lupperarmR, jelbowR-lforearmR);
            auto relbow_actr = new angle_constraint_t(rupperarm, rforearm);
            relbow_actr->set_limits(60*fTO_RAD,
                                    mat3f::rotation(-60*fTO_RAD, 0, 0, 1),
                                    linalg::mat3f_identity);
            
            // (Wrist) - Hand - (Palm)
            obj_body_t* lhand = new obj_body_t(lhandL, hand_path, "lhand",
                                               mat4f::rotation(-fPI/2, 0, 0, 1)*mat4f::rotation(-fPI/2, 0, 1, 0)*mat4f::scaling( (jpalmL-jwristL).x ));
            obj_body_t* rhand = new obj_body_t(lhandR, hand_path, "rhand",
                                               mat4f::rotation(fPI/2, 0, 0, 1)*mat4f::rotation(-fPI/2, 0, 1, 0)*mat4f::scaling( (jwristR-jpalmR).x ));
            
            // Wrist joint
            auto lwrist_pctr = new ballsocket_constraint_t(lforearm, lhand, jwristL-lforearmL, jwristL-lhandL);
            auto lwrist_actr = new conetwist_constraint_t(lforearm, lhand, lwrist_pctr);
            lwrist_actr->set_limits(30*fTO_RAD, 20*fTO_RAD, mat3f::rotation(fPI/2, 0, 1, 0), mat3f::rotation(fPI/2, 0, 1, 0));
            
            auto rwrist_pctr = new ballsocket_constraint_t(rforearm, rhand, jwristR-lforearmR, jwristR-lhandR);
            auto rwrist_actr = new conetwist_constraint_t(rforearm, rhand, rwrist_pctr);
            rwrist_actr->set_limits(30*fTO_RAD, 20*fTO_RAD, mat3f::rotation(-fPI/2, 0, 1, 0), mat3f::rotation(-fPI/2, 0, 1, 0));
            
#if 0
            // This is just needed in order to re-bind pose
            
            //
            // Assign skeleton arrays
            //
            
            jointpos[JT_WAIST] = jwaist;
            jointpos[JT_NECK] = jneck;
            jointpos[JT_HEADTIP] = jheadtip;
            jointpos[JT_LHIP] = jhipL;
            jointpos[JT_RHIP] = jhipR;
            jointpos[JT_LKNEE] = jkneeL;
            jointpos[JT_RKNEE] = jkneeR;
            jointpos[JT_LANKLE] = jankleL;
            jointpos[JT_RANKLE] = jankleR;
            jointpos[JT_LHEEL] = jheelL;
            jointpos[JT_RHEEL] = jheelR;
            jointpos[JT_LTOE] = jtoeL;
            jointpos[JT_RTOE] = jtoeR;
            jointpos[JT_LSHOULDER] = jshoulderL;
            jointpos[JT_RSHOULDER] = jshoulderR;
            jointpos[JT_LELBOW] = jelbowL;
            jointpos[JT_RELBOW] = jelbowR;
            jointpos[JT_LWRIST] = jwristL;
            jointpos[JT_RWRIST] = jwristR;
            jointpos[JT_LPALM] = jpalmL;
            jointpos[JT_RPALM] = jpalmR;
            
            limbpos[LB_TORSO] = ltorso;
            limbpos[LB_HEAD] = lhead;
            limbpos[LB_PELVIS] = lpelvis;
            limbpos[LB_LTHIGH] = lthighL;
            limbpos[LB_RTHIGH] = lthighR;
            limbpos[LB_LCALF] = lcalfL;
            limbpos[LB_RCALF] = lcalfR;
            limbpos[LB_LFOOT] = jheelL;
            limbpos[LB_RFOOT] = jheelR;
            limbpos[LB_LUPPERARM] = lupperarmL;
            limbpos[LB_RUPPERARM] = lupperarmR;
            limbpos[LB_LFOREARM] = lforearmL;
            limbpos[LB_RFOREARM] = lforearmR;
            limbpos[LB_LHAND] = lhandL;
            limbpos[LB_RHAND] = lhandR;
#endif
            
            limbs =
            {
                torso, head, pelvis,
                lthigh, rthigh, lcalf, rcalf, lfoot, rfoot,
                lupperarm, rupperarm, lforearm, rforearm,
                lhand, rhand
            };
            
            // Set position, rotation and velocity of limbs
            mat3f R = frame;
            mat4f TR = mat4f::translation(p) * mat4f(R);
            for (auto* body : limbs)
            {
                body->V = v;
                body->X = (TR * body->X.xyz1()).xyz();
                body->apply_rotation(R);
            }
            
//            joints = {
                // Need this ?
//            };
            
//            for (auto limb : limbs)
//            {
//
//            }
            
            // Add bodies
            world->add_body(torso);
            world->add_body(head);
            world->add_body(pelvis);
            world->add_body(lthigh);
            world->add_body(rthigh);
            world->add_body(lcalf);
            world->add_body(rcalf);
            world->add_body(lfoot);
            world->add_body(rfoot);
            world->add_body(lupperarm);
            world->add_body(rupperarm);
            world->add_body(lforearm);
            world->add_body(rforearm);
            world->add_body(lhand);
            world->add_body(rhand);
            
            // Add constraints
            world->constraints.push_back(neck_pctr);
            world->constraints.push_back(neck_actr);
            world->constraints.push_back(waist_pctr);
            world->constraints.push_back(waist_actr);
            
            world->constraints.push_back(lhip_pctr);
            world->constraints.push_back(lhip_actr);
            world->constraints.push_back(rhip_pctr);
            world->constraints.push_back(rhip_actr);
            
            world->constraints.push_back(lknee_pctr);
            world->constraints.push_back(lknee_actr);
            world->constraints.push_back(rknee_pctr);
            world->constraints.push_back(rknee_actr);
            
            world->constraints.push_back(lankle_pctr);
            world->constraints.push_back(lankle_actr);
            world->constraints.push_back(rankle_pctr);
            world->constraints.push_back(rankle_actr);
            
            world->constraints.push_back(lshoulder_pctr);
            world->constraints.push_back(lshoulder_actr);
            world->constraints.push_back(rshoulder_pctr);
            world->constraints.push_back(rshoulder_actr);
            
            world->constraints.push_back(lelbow_pctr);
            world->constraints.push_back(lelbow_actr);
            world->constraints.push_back(relbow_pctr);
            world->constraints.push_back(relbow_actr);
            
            world->constraints.push_back(lwrist_pctr);
            world->constraints.push_back(lwrist_actr);
            world->constraints.push_back(rwrist_actr);
            world->constraints.push_back(rwrist_pctr);
            
            // Render materials
            material_t red_mtl, blue_mtl, dark_mtl, skin_mtl;
            red_mtl.Ka = {0.4, 0.2, 0.2};
            red_mtl.Kd = {0.6, 0.2, 0.2};
            red_mtl.Ks = {1, 0, 0};
            red_mtl.Ns = 20;
            red_mtl.name = "ragdoll_red";
            blue_mtl.Ka = {0.2, 0.2, 0.4};
            blue_mtl.Kd = {0.2, 0.2, 0.6};
            blue_mtl.Ks = {0, 0, 1};
            blue_mtl.Ns = 20;
            blue_mtl.name = "ragdoll_blue";
            dark_mtl.Ka = {0.2, 0.2, 0.2};
            dark_mtl.Kd = {0.4, 0.4, 0.4};
            dark_mtl.Ks = {0.6, 0.6, 0.6};
            dark_mtl.Ns = 20;
            dark_mtl.name = "ragdoll_dark";
            skin_mtl.Ka = {0.2, 0.16, 0.1156};
            skin_mtl.Kd = {1.0, 0.8, 0.578};
            skin_mtl.Ks = {0.8, 0.8, 0.8};
            skin_mtl.Ns = 20;
            skin_mtl.name = "ragdoll_skin";
            factory->add_material(red_mtl);
            factory->add_material(blue_mtl);
            factory->add_material(dark_mtl);
            factory->add_material(skin_mtl);
            
            // Generate render meshes from colliders
            factory->create_from_polygon_geometry(torso, static_cast<poly_collider_t*>(torso->colliders[0]), "ragdoll_red", mat4f(1), "");
            factory->create_from_polygon_geometry(head, static_cast<poly_collider_t*>(head->colliders[0]), "ragdoll_skin", mat4f(1), "");
            factory->create_from_polygon_geometry(pelvis, static_cast<poly_collider_t*>(pelvis->colliders[0]), "ragdoll_blue", mat4f(1), "");
            factory->create_from_polygon_geometry(lthigh, static_cast<poly_collider_t*>(lthigh->colliders[0]), "ragdoll_blue", mat4f(1), "");
            factory->create_from_polygon_geometry(rthigh, static_cast<poly_collider_t*>(rthigh->colliders[0]), "ragdoll_blue", mat4f(1), "");
            factory->create_from_polygon_geometry(lcalf, static_cast<poly_collider_t*>(lcalf->colliders[0]), "ragdoll_blue", mat4f(1), "");
            factory->create_from_polygon_geometry(rcalf, static_cast<poly_collider_t*>(rcalf->colliders[0]), "ragdoll_blue", mat4f(1), "");
            factory->create_from_polygon_geometry(lfoot, static_cast<poly_collider_t*>(lfoot->colliders[0]), "ragdoll_dark", mat4f(1), "");
            factory->create_from_polygon_geometry(rfoot, static_cast<poly_collider_t*>(rfoot->colliders[0]), "ragdoll_dark", mat4f(1), "");
            factory->create_from_polygon_geometry(lupperarm, static_cast<poly_collider_t*>(lupperarm->colliders[0]), "ragdoll_red", mat4f(1), "");
            factory->create_from_polygon_geometry(rupperarm, static_cast<poly_collider_t*>(rupperarm->colliders[0]), "ragdoll_red", mat4f(1), "");
            factory->create_from_polygon_geometry(lforearm, static_cast<poly_collider_t*>(lforearm->colliders[0]), "ragdoll_red", mat4f(1), "");
            factory->create_from_polygon_geometry(rforearm, static_cast<poly_collider_t*>(rforearm->colliders[0]), "ragdoll_red", mat4f(1), "");
            factory->create_from_polygon_geometry(lhand, static_cast<poly_collider_t*>(lhand->colliders[0]), "ragdoll_skin", mat4f(1), "");
            factory->create_from_polygon_geometry(rhand, static_cast<poly_collider_t*>(rhand->colliders[0]), "ragdoll_skin", mat4f(1), "");
        }
    };
    

    
    struct enumhash
    {
        template <typename T>
        std::size_t operator()(const T& t) const
        {
            return static_cast<std::size_t>(t);
        }
    };
    
    const std::unordered_map<unsigned, unsigned> limb_joint_relations =
    {
        {LB_LUPPERARM, JT_LSHOULDER},
        {LB_RUPPERARM, JT_RSHOULDER}
    };
    
//    struct JointLimbRelation_t { JOINT joint; LIMB limb; };
//    
//    JointLimbRelation_t joint_limb_relations[] =
//    {
//        {},
//        {}
//    };
    
    const std::string joint_id[] =
    {
        "jt_pelvis",
        // Upper body
        "jt_waist",
        "jt_belly",
        "jt_chest",
        // Head
        "jt_neck1",
        "jt_neck2",
        "jt_head",
        "jt_headtip",
        // Arms
        "jt_lshoulder",
        "jt_rshoulder",
        "jt_lelbow",
        "jt_relbow",
        "jt_lwrist",
        "jt_rwrist",
        "jt_lhand",
        "jt_rhand",
        // Lower body
        "jt_lhip",
        "jt_rhip",
        "jt_lknee",
        "jt_rknee",
        "jt_lankle",
        "jt_rankle",
        "jt_lballfoot",
        "jt_rballfoot",
        "jt_lfoot",
        "jt_rfoot"
    };
    const std::string limb_id[] =
    {
        "lb_pelvis",
        // Upper body
        "lb_waist",
        "lb_belly",
        "lb_chest",
        // Head
        // Arms
        "lb_lbicep",
        "lb_rbicep",
        "lb_lforearm",
        "lb_rforearm",
        "lb_lhand",
        "lb_rhand",
        // Lower body
        "lb_lthigh",
        "lb_rthigh",
        "lb_lshin",
        "lb_rshin",
        "lb_lfoot",
        "lb_rfoot"
    };
    
    class blockman_ragdollv1_t
    {
        mat4f joint_transforms[JT_COUNT];
        mat4f limb_transforms[LB_COUNT];
        
    public:
        
        blockman_ragdollv1_t(std::string anim_file, world_t* world)
        {
            mesh_anim_data_t anim_data(anim_file);
            
            mat4f S = linalg::mat4f_identity; //mat4f::translation(0,0,5);// * mat4f::scaling(0.04f);
            
            // Fetch joints
            for (int i=0; i<JT_COUNT; i++)
            {
                joint_transforms[i] =
                    S * anim_data.get_node_transforms( joint_id[i] ).front();
                printf("found joint %s\n", joint_id[i].c_str());
            }
            
            // Fetch limbs
            for (int i=0; i<LB_COUNT; i++)
            {
                limb_transforms[i] =
                    S * anim_data.get_node_transforms( limb_id[i] ).front();
                printf("found limb %s\n", limb_id[i].c_str());
            }
            
            // Create dummy joint bodies
            for (int i=0; i<JT_COUNT; i++)
            {
                vec3f p = ( joint_transforms[i] * vec4f(0,0,0,1) ).xyz();
                sphere_t* b = new sphere_t(p, 0.5, joint_id[i]);
                world->add_body(b);
            }
            
            // Create dummy limb bodies
            for (int i=0; i<LB_COUNT; i++)
            {
                mat4f joint_tfm = linalg::mat4f_identity;
                auto jit = limb_joint_relations.find( i );
                if (jit != limb_joint_relations.end())
                    joint_tfm = joint_transforms[ jit->second ];
                vec3f jpos = ( joint_tfm * vec4f(0,0,0,1) ).xyz();
                
                vec3f p = jpos*0 + ( limb_transforms[i] * vec4f(0,0,0,1) ).xyz();
                box_body_t* b = new box_body_t(p, {5,5,5}, limb_id[i]);
                world->add_body(b);
            }
        }
    };
    
}
#endif /* blockman_ragdollv1_hpp */
