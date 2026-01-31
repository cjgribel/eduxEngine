
/*
 * Tau3D Dynamics 
 * Carl Johan Gribel (c) 2011, cjgribel@gmail.com
 * Updated July 2021
 *
 */

#pragma once
#ifndef Forces_hpp
#define Forces_hpp

//#include "tau3d.h"
//#include "body.h"
#include "vec.h"
#include <vector>
#include <entt/entt.hpp>
#include "RigidBody.hpp"

using namespace linalg;
using RigidBody::RigidBody3dComponent;
using RigidBody::RigidBody2dComponent;

//using linalg::vec3f;
//class body_t;

// Linear spring damper
//
class SpringDamperForce
{
public:
//	body_t	*bodyA, *bodyB;
    v3f	rA {0.0f, 0.0f, 0.0f};
    v3f rB {0.0f, 0.0f, 0.0f};
	float K, D, L;

    SpringDamperForce();
    
    SpringDamperForce(float K,
                      float D,
                      float L);
    
	SpringDamperForce(v3f rA,
                      v3f rB,
                      float K,
                      float D,
                      float L);

	void apply(RigidBody3dComponent& rbA,
               RigidBody3dComponent& rbB);

//	void render();
};

struct LinearSpringDamper3dComponent
{
    entt::entity    rb3d_entityA = entt::null;
    entt::entity    rb3d_entityB = entt::null;
    v3f             rA = v3f_000;
    v3f             rB = v3f_000;
    bool            is_active = true;
    
    float K;    // Spring constant
    float D;    // Damping constant
    float L;    // esting length (m)
    
    using VecType = v3f;
    using RigidBodyType = RigidBody3dComponent;
};

struct LinearSpringDamper2dComponent
{
    entt::entity    rb2d_entityA = entt::null;
    entt::entity    rb2d_entityB = entt::null;
    v2f             rA = v2f_00;
    v2f             rB = v2f_00;
    bool            is_active = true;
    
    float K;    // Spring constant
    float D;    // Damping constant
    float L;    // Resting length (m)
    
//    using VecType = v2f;
};

struct AngularSpringDamper3dComponent
{
    entt::entity    rb3d_entityA = entt::null;
    entt::entity    rb3d_entityB = entt::null;
    bool            is_active = true;
    
    float   K;    // Torsion constant
    float   D;    // Rotational damping
    
    m3f     R;    // Initial relative rotation from body A to B, e.g. B * A^T
};

struct AngularSpringDamper2dComponent
{
    entt::entity    rb2d_entityA = entt::null;
    entt::entity    rb2d_entityB = entt::null;
    bool            is_active = true;
    
    float K;    // Torsion constant
    float D;    // Rotational damping
    float R;    // Resting angle (rad)
};

struct LinearSpringDamperDrawAttribsComponent
{
    float r_outer;
    float r_inner;
    float revs;
    uint color;
};

struct AngularSpringDamperDrawAttribsComponent
{
    int dont_know_what_to_have_here;
};

// Systems

class LinearSpringDamper3dSystem
{
public:

    static void update(float dt, entt::registry& registry);
};

class LinearSpringDamper2dSystem
{
public:

    static void update(float dt, entt::registry& registry);
};

class AngularSpringDamper3dSystem
{
public:

    static void update(float dt, entt::registry& registry);
};

class AngularSpringDamper2dSystem
{
public:

    static void update(float dt, entt::registry& registry);
};

#if 0
class field_F_t
{
    virtual void apply_force(std::vector<body_t*> bodies) const = 0;
};

/*
 component #1: perpendicular to radial vecor. strength increase inversely with dinstance to center (faster and faster spin)
 component #2: radial direction. constant strength?
 */
class vortex_F_t : public field_F_t
{
    vec3f center, up;
    // inverse distance coefficients
    // their ratio decides how aggressively the vortex spirals
    // note: very strong forces are not handles well by an explicit integrator
    float Crad = 750;
    float Cang = 100;
    float Cup = 100;
    
public:
    
    vortex_F_t(const vec3f& center, const vec3f& up) : center(center), up(up)
    {
    
    }
    
    void apply_force(std::vector<body_t*> bodies) const override
    {
        for (body_t* b : bodies)
        {
            vec3f bvec = b->X-center;
            vec3f bproj = up*(dot(up, bvec));
            vec3f brej = bvec-bproj;
            float brej_norm = brej.norm2();
            if (brej_norm < 1) brej_norm = 1; // avoid singularity-like circumstances in the middle
            float idist = (brej_norm<1e-4? 0 : 1.0f/brej_norm);
            
            // angular & radial components
            vec3f u = normalize(up%brej);
            vec3f v = normalize(brej);
            
            vec3f angular_F = u*idist*Cang;
            vec3f radial_F = -v*sqrtf(idist)*Crad;
            vec3f up_F = up*idist*idist*idist*Cup;
            
            b->apply_force(radial_F+angular_F+up_F, vec3f(0,0,0));
        }
    }
};
#endif

#endif /* force_t_H */
