//
//  contact_constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-08.
//
//

#ifndef ContactConstraint_h
#define ContactConstraint_h

#include <stdio.h>
#include <unordered_map>
#include <iomanip> /* std::setprecision */
#include "vec.h"
//#include "hash_combine.h"
#include "CoreComponents.hpp"
//#include "RigidBody.hpp"

// --- Contacts ---

// Penetration tolerance (m)
#define PenetrationTolerance3d 0.002f
#define PenetrationTolerance2d 0.002f

// Error Reduction Parameter (fraction)
// Practical range ~0.2-0.5
#define ERP3d 0.4f
#define ERP2d 0.4f

// Velocity tolerance for elastic collision (m/s), i.e. for restitution > 0.
// Should be > g*dt ~= 9.82*0.016 = 0.15712
#define VelocityTolerance3d 1.0f
#define VelocityTolerance2d 1.0f

// Fraction of previous impulse to use as warm start
#define WarmStartRelaxation3d 0.90f
#define WarmStartRelaxation2d 0.95f

using namespace linalg;

//
// Contact point with collision impulse solver
//

template<int D>
struct ContactPoint;

template<>
struct ContactPoint<3>
{
    v3f cp, cn;
    float dist;         // < 0
    int cndir = 1;      // 1 when cn points A->B, -1 when cn points A<-B
    float age = 0;
    
    struct ContactID
    {
        entt::entity colliderA = entt::null;    // Collider entities
        entt::entity colliderB = entt::null;
        int face_id = -1;               // face & edge indices are set when contact heirs from a clipped edge
        int edge_id = -1;
        int vertex_id = -1;             // vertex index is set when contact heirs from a vertex
        int flip = -1;                  // 0/1 when indices of body A/B
        
        ContactID() { }
        
        ContactID(int face_id,
                  int edge_id,
                  int vertex_id,
                  int flip)
        : face_id(face_id), edge_id(edge_id), vertex_id(vertex_id), flip(flip) { }
        
        // todo: use one conditional
        bool operator == (const ContactID& id) const
        {
//            return false;
            
//            if (colliderA != id.colliderA) return false;
//            if (colliderB != id.colliderB) return false;
//            if (flip != id.flip) return false;
//            if (face_id != id.face_id) return false;
//            if (edge_id != id.edge_id) return false;
//            if (vertex_id != id.vertex_id) return false;
//            return true;
            
            const bool colAeq =
            (colliderA == id.colliderA && flip == id.flip) ||
            (colliderA == id.colliderB && flip != id.flip);
            const bool colBeq =
            (colliderB == id.colliderB && flip == id.flip) ||
            (colliderB == id.colliderA && flip != id.flip);
            if (!colAeq) return false;
            if (!colBeq) return false;
//            if (flip != id.flip) return false; // flip will be equal if A = A, and not equal if A = B
            if (face_id != id.face_id) return false;
            if (edge_id != id.edge_id) return false;
            if (vertex_id != id.vertex_id) return false;
            return true;
        }
    } id;
    
private:

    float lambda_n_vel_acc = 0.0f;      // ...
    float lambda_n_tot_acc = 0.0f;      // accumulated total impulse along normal
//    float lambda_n_bias_acc;          // accumulated bias impulse along normal
    float bias_n;                       // normal bias
    float iK_n;                         // normal effective mass
    
    v3f rA, rB;                         // contact anchors in local body spaces
    v3f ct0, ct1;                       // tangent directions
    float iK_t0, iK_t1;                 // tangent effective masses
    float lambda_t0_acc = 0.0f;         // accumulated tangential impulses
    float lambda_t1_acc = 0.0f;
    float bias_t;                       // tangent bias
    
    // Soft constraint test
    float beta, gamma;
    
public:

    void pre_solve(const EntityPairType& rb_pair,
                   entt::registry& registry,
                   const ContactPoint<3>* cp_warm,
                   float h);
    
    void solve(const EntityPairType& rb_pair,
               entt::registry& registry,
               float h);
    
    void post_solve(float h) { age += h; }
    
    bool operator == (const ContactPoint<3>& c) const { return id == c.id; }
    
    friend std::ostream& operator<< (std::ostream &out,
                                     const ContactPoint<3> &c);
};

//
/*
 * custom hash for contact_point_t [10]
 */
//template<> struct std::hash<ContactPoint3d>
//{
//    std::size_t operator () (const ContactPoint3d& cp) const
//    {
//        return hash_combine(cp.id.face_id,
//                            cp.id.edge_id,
//                            cp.id.vertex_id);
//
////        return
////        ((hash<int>()(c.id.face_id) ^
////          (hash<int>()(c.id.edge_id) << 1)) >> 1) ^
////        (hash<int>()(c.id.vertex_id) << 1);
//    }
//};

template<>
struct ContactPoint<2>
{
    v2f cp, cn;
    float dist;         // > 0
    int cndir = 1;      // 1 when normal points A->B, -1 when pointing A<-B
    float age = 0;
    
    struct ContactID
    {
        entt::entity colliderA = entt::null;    // Collider entities
        entt::entity colliderB = entt::null;
        int vertex_id = -1;             // vertex index is set when contact heirs from a vertex
        int edge_id = -1;
        int flip = -1;                  // 0/1 when vertex indices are of body A/B
        
        bool operator == (const ContactID& id) const
        {
//            const bool colAeq =
//            (colliderA == id.colliderA && flip == id.flip) ||
//            (colliderA == id.colliderB && flip != id.flip);
//            const bool colBeq =
//            (colliderB == id.colliderB && flip == id.flip) ||
//            (colliderB == id.colliderA && flip != id.flip);
//            if (!colAeq) return false;
//            if (!colBeq) return false;
//            return true;
//            return false;
            
//            if (colliderA != id.colliderA) return false;
//            if (colliderB != id.colliderB) return false;
//            if (flip != id.flip) return false;
//            if (edge_id != id.edge_id) return false;
//            if (vertex_id != id.vertex_id) return false;
//            return true;
            
            const bool colAeq =
            (colliderA == id.colliderA && flip == id.flip) ||
            (colliderA == id.colliderB && flip != id.flip);
            const bool colBeq =
            (colliderB == id.colliderB && flip == id.flip) ||
            (colliderB == id.colliderA && flip != id.flip);
            if (!colAeq) return false;
            if (!colBeq) return false;
//            if (flip != id.flip) return false; // flip will be equal if A = A, and not equal if A = B
            if (edge_id != id.edge_id) return false;
            if (vertex_id != id.vertex_id) return false;
            return true;
        }
    } id;
    
    ContactPoint<2>() = default;
    
    ContactPoint<2>(const v2f& cp,
                    const v2f& cn,
                    int cndir,
                    float dist,
                    const ContactID& id) :
    cp(cp), cn(cn), cndir(cndir), dist(dist), id(id)
    {
        
    }
    
//    t2ContactJoint(    t2Body* bodyA,
//                    t2Body* bodyB,
//                    vec2f cp,
//                    vec2f cn,
//                    float depth,
//                    int id_geomA, int id_geomB, int id_vertex, int id_edge)
//        :    t2Joint(bodyA, bodyB, JOINT_CONTACT),
//            cp(cp),
//            cn(cn),
//            depth(depth),
//            id_geomA(id_geomA), id_geomB(id_geomB), id_vertex(id_vertex), id_edge(id_edge),
//            ERP_n(T2_CONTACT_ERPn),
//            depth_tol(T2_CONTACT_DEPTH_TOL), vel_tol(T2_CONTACT_VEL_TOL)
//    { }
    
public:
    
    // TODO: Expose this variable somehow. Is used by e.g. particle effects.
    vec2f   ct;
    float rABdot_n_initial, rABdot_t_initial;
    
private:
    
    vec2f   rA, rB;
    float   bias_n, bias_t;
    float   iK_n, iK_t;
    float   lambdaAcc_n = 0.0f;
    float   lambdaAcc_t = 0.0f;
    
public:
    void pre_solve(const EntityPairType& rb_pair,
                   entt::registry& registry,
                   const ContactPoint<2>* cp_warm,
                   float h);
    
    void solve(const EntityPairType& rb_pair,
               entt::registry& registry,
               float h);
    
    void post_solve(float h) { age += h; }
    
    bool operator == (const ContactPoint<2>& c) const { return id == c.id; }
};

using ContactPoint3d = ContactPoint<3>;
using ContactPoint2d = ContactPoint<2>;

//
/*
 * custom hash for contact_point_t [10]
 */
//template<> struct std::hash<ContactPoint2d>
//{
//    std::size_t operator () (const ContactPoint2d& cp) const
//    {
//        return hash_combine(cp.id.edge_id,
//                            cp.id.vertex_id);
//
////        ((hash<int>()(c.id.face_id) ^
////          (hash<int>()(c.id.edge_id) << 1)) >> 1) ^
////        (hash<int>()(c.id.vertex_id) << 1);
//    }
//};

inline std::ostream& operator<< (std::ostream &out,
                                 const ContactPoint3d &c)
{
    uint32_t colA = static_cast<std::underlying_type_t<entt::entity>>(c.id.colliderA);
    uint32_t colB = static_cast<std::underlying_type_t<entt::entity>>(c.id.colliderB);
    
    out << std::setprecision(2) << std::fixed;
    out << "contact: p " << c.cp << " n " << c.cn;
    out << " cndir " << c.cndir << " dist " << c.dist;
    out << " age " << c.age;
    out << " ct0 " << c.ct0 << " ct1 " << c.ct1;
    out << " id (colA " << colA << " colB " << colB << " face " << c.id.face_id;
    out << " edge " << c.id.edge_id << " vertex " << c.id.vertex_id << " flip " << c.id.flip << ")";
    out << " totacc " << c.lambda_n_tot_acc;
    out << " t0acc " << c.lambda_t0_acc;
    out << " t1acc " << c.lambda_t1_acc;
    out << " rA " << c.rA << " rB " << c.rB;
//    out << " rA " << c.rA
    out << std::endl;
    
    
    return out;
}

inline std::ostream& operator<< (std::ostream &out,
                                 const ContactPoint2d &c)
{
    return out << "contact: p = " << c.cp << ", n = " << c.cn;
}

#endif
