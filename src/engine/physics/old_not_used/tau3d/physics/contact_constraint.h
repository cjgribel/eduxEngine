//
//  contact_constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-08.
//
//

#ifndef __tau3d__contact_constraint__
#define __tau3d__contact_constraint__

#include <stdio.h>
#include <unordered_map>
#include "vec.h"
#include "body.h"


//
// Contact point with collision impulse solver
//
struct contact_point_t
{
    vec3f cp, cn;
    float dist;
    int cndir = 1;          // 1 when normal points A->B, -1 when pointing A<-B
    float age = 0;
    
    struct contact_id_t
    {
        collider_t *geomA = nullptr;    // geometry pointers
        collider_t *geomB = nullptr;
        int face_id = -1;               // face & edge indices is set when contact heirs from a clipped edge
        int edge_id = -1;
        int vertex_id = -1;             // vertex index is set when contact heirs from a vertex
        int flip = -1;                  // 0/1 when indices of body A/B
        
        contact_id_t() { }
        
        contact_id_t(int face_id, int edge_id, int vertex_id, int flip) : face_id(face_id), edge_id(edge_id), vertex_id(vertex_id), flip(flip) { }
        
        // todo: use one conditional
        bool operator == (const contact_id_t& id) const
        {
            if (geomA != id.geomA) return false;
            if (geomB != id.geomB) return false;
            if (flip != id.flip) return false;
            if (face_id != id.face_id) return false;
            if (edge_id != id.edge_id) return false;
            if (vertex_id != id.vertex_id) return false;
            return true;
        }
    } id;
    
private:
    float lambda_n_vel_acc;
    float lambda_n_tot_acc;   // accumulated normal impulse
    float lambda_n_bias_acc;
    float bias_n;                               // normal bias
    float iK_n;                                 // normal effective mass
    
    vec3f rA, rB;                               // contact anchors in local body spaces
    vec3f ct0, ct1;                             // tangent directions
    float iK_t0, iK_t1;                         // tangent effective masses
    float lambda_t0_acc, lambda_t1_acc;         // accumulated tangential impulses
    float bias_t;                               // tangent bias
    
    // test
    float beta, gamma;
    
public:
    void pre_solve(body_t *bodyA, body_t *bodyB, contact_point_t* c_warm, float h);
    
    void solve(body_t *bodyA, body_t *bodyB, float h);
    
    void post_solve(float h) { age += h; }
    
    bool operator == (const contact_point_t& c) const { return id == c.id; }
};

/*
 * custom hash for contact_point_t [10]
 */
template<> struct std::hash<contact_point_t>
{
    std::size_t operator () (const contact_point_t& c) const
    {
        return  ((hash<int>()(c.id.face_id) ^
                (hash<int>()(c.id.edge_id) << 1)) >> 1) ^
                (hash<int>()(c.id.vertex_id) << 1);
    }
};

inline std::ostream& operator<< (std::ostream &out, const contact_point_t &c)
{
    return out << "contact: p = " << c.cp << ", n = " << c.cn;
}

//
// Manifold, i.e. set of contacts, for a pair of bodies
//
struct contact_manifold_t
{
    body_t *bodyA, *bodyB;
    
    std::vector<contact_point_t> contacts; // hash?
    //std::unordered_map<contact_point_t, contact_point_t> contacts_;
    
    void pre_solve(contact_manifold_t *cm_warm, float h)
    {
        if (cm_warm)
        {
//            int found_c = 0;
            for (contact_point_t& c : contacts)
            {
                contact_point_t* c_warm = nullptr;
                // Look for existing contact point
                auto c_warm_it = std::find(cm_warm->contacts.begin(), cm_warm->contacts.end(), c);
                if (c_warm_it != cm_warm->contacts.end())
                {
                    c_warm = &*c_warm_it;
                    c.age = c_warm->age;
//                    found_c++;
                    //render_marker(c_warm->cp, 0.05, 1, {0,0,1,1});
                }
                c.pre_solve(bodyA, bodyB, c_warm, h);
            }
//            printf("found: %s, %s. contacts %d (%d)\n",
//                   cm_warm->bodyA->id.c_str(), cm_warm->bodyB->id.c_str(), found_c, (int)contacts.size());
        }
        else {
            for (contact_point_t& c : contacts)
                c.pre_solve(bodyA, bodyB, nullptr, h);
        }
    }
    
    void solve(float h) {
        for (contact_point_t& c : contacts) c.solve(bodyA, bodyB, h);
    }
    
    void post_solve(float h) {
        for (contact_point_t& c : contacts) c.post_solve(h);
    }
    
    bool operator == (const contact_manifold_t& cm)
    {
        return (bodyA == cm.bodyA && bodyB == cm.bodyB) || (bodyA == cm.bodyB && bodyB == cm.bodyA);
    }
};

inline std::ostream& operator<< (std::ostream &out, const contact_manifold_t& cm)
{
    out << "arbiter, " << cm.contacts.size() << " contacts\n";
    for (int i=0; i<cm.contacts.size(); i++) out << "  " << cm.contacts[i] << "\n";
    return out;
}

#endif /* defined(__tau3d__contact_constraint__) */
