//
//  RigidBody.hpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-07-21.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef RigidBody_hpp
#define RigidBody_hpp

#include <stdio.h>
#include "config.h"
#include "vec.h"
#include "mat.h"

// --- Geometry ---

// 3D presets
#define FrictionStaticDefault3d 0.2f    /* Static friction >= 0 */
#define FrictionDynamicDefault3d 0.5f   /* Kinetic friction >= 0 */
#define RestitutionDefault3d 0.0f       /* Restitution, [0,1] */

// 2D presets
#define FrictionStaticDefault2d 0.5f    /* Static friction >= 0 */
#define FrictionDynamicDefault2d 0.5f   /* Kinetic friction >= 0 */
#define RestitutionDefault2d 0.0f       /* Restitution, [0,1] */

namespace RigidBody {

using namespace linalg;

template<int D>
struct RigidBodyComponent;

template<>
struct RigidBodyComponent<3>
{
//    m4f D_toPE = m4f_1;              // EXPERIMENTAL - parent offset w.r.t. PE (and vice versa)
//    m4f D = m4f_1;
//    bool joint = false;
    
//    bool bounce = false;        // for debugging
//    bool control_tfm = true;    // for debugging - RB control should be decided elsewhere
    v3f W_mask {1.0f, 1.0f, 1.0f};
    v3f V_mask {1.0f, 1.0f, 1.0f};
    v3f g {0.0f, -9.82f, 0.0f};
    
    // Mass state
    float   m = 1.0f;       // Mass
    float   im = 1.0f;      // Inverse mass
//    float   rho = 1.0f;     // Density
    m3f     I {1.0f};       // Inertia tensor
    m3f     iI {1.0f};      // Inverse inertia tensor (aux)
    m3f     iI_w {1.0f};    // Inverse inertia tensor in world space (aux)
    // Linear state
    v3f     X {0.0f, 0.0f, 0.0f};   // Position
    v3f     X_prev {0.0f, 0.0f, 0.0f}; // Buffered position
    v3f     V {0.0f, 0.0f, 0.0f};   // Velocity
    v3f     F {0.0f, 0.0f, 0.0f};   // Force accumulator
    v3f     X_r {0.0f, 0.0f, 0.0f}; // Anchor point of initial X wrt to com (X), when computed
    // Angular state
    quatf   Q {};                   // Orientation
    quatf   Q_prev {};              // Buffered orientation
    m3f     R {1.0f};               // Orientation (matrix) (aux)
    m3f     Ri {1.0f};              // Orientation inverse (matrix) (aux)
    v3f     W {0.0f, 0.0f, 0.0f};   // Angular velocity
    v3f     T {0.0f, 0.0f, 0.0f};   // Torque accumulator
    
    // Constitutive state
    float restitution = RestitutionDefault3d;   // Coeff. of restitution
    float my_s = FrictionStaticDefault3d;          // Coeff. of static friction
    float my_d = FrictionDynamicDefault3d;          // Coeff. of dynamic friction
    
    float V_damp = 0;
    float W_damp = 0;
    
    bool is_static = false; // Static bodies have constant state
//    std::string id;
    //std::vector<collider_t*> colliders;
    //AABB_t AABB_w;
    
    //    RigidBodyComponent(const vec3f &p, const std::string &id = "") :
    //    X(p), X_r(0,0,0), V(0,0,0), F(0,0,0),
    //    R(1), Ri(1), Q(),
    //    W(0,0,0), T(0,0,0),
    //    m(1), im(1), rho(1),
    //    I(1), iI(1), iI_w(1),
    //    is_static(false),
    //    id(id)
    //    {
    //
    //    }
};

template<>
struct RigidBodyComponent<2>
{
    bool bounce = false;        // for debugging
    bool control_tfm = true;    // for debugging - RB control should be decided elsewhere
    
//    m4f D_toPE = m4f_1;              // EXPERIMENTAL
    m4f D = m4f_1;
    bool joint = false;
    
    float W_mask    {1.0f};
    v2f V_mask      {1.0f, 1.0f};
    v2f g           {0.0f, -9.82f};
    
    // Mass state
    float   m       {1.0f};         // Mass
    float   im      {1.0f};         // Inverse mass
//    float   rho = 1.0f;           // Density
    float   I       {1.0f};         // Inertia tensor
    float   iI      {1.0f};         // Inverse inertia tensor (aux)
    float   iI_w    {1.0f};         // Inverse inertia tensor in world space (aux)
    // Linear state
    v2f     X       {0.0f, 0.0f};   // Position
    v2f     X_prev  {0.0f, 0.0f};   // Buffered position
    v2f     V       {0.0f, 0.0f};   // Velocity
//    v2f     V_prev  {0.0f, 0.0f}; // Buffered velocity
    v2f     F       {0.0f, 0.0f};   // Force accumulator
    v2f     X_r     {0.0f, 0.0f};   // Anchor point of initial X wrt to com (X), when computed
    // Angular state
//    quatf   Q {};                 // Orientation
//    quatf   Q_prev {};            // Buffered orientation
    float   R       {0.0f};         // Orientation
    float   R_prev  {0.0f};         // Buffered orientation
//    m3f     Ri {1.0f};            // Orientation inverse (matrix) (aux)
    double   W       {0.0};         // Angular velocity
    float   T       {0.0f};         // Torque accumulator
    
    // Constitutive state
    float restitution   = RestitutionDefault2d;
    float my_s          = FrictionStaticDefault2d;
    float my_d          = FrictionDynamicDefault2d;
    
    float V_damp = 0;
    float W_damp = 0;
    
    bool is_static = false; // Static bodies have constant state
//    std::string id;
    //std::vector<collider_t*> colliders;
    //AABB_t AABB_w;
    
    //    RigidBodyComponent(const vec3f &p, const std::string &id = "") :
    //    X(p), X_r(0,0,0), V(0,0,0), F(0,0,0),
    //    R(1), Ri(1), Q(),
    //    W(0,0,0), T(0,0,0),
    //    m(1), im(1), rho(1),
    //    I(1), iI(1), iI_w(1),
    //    is_static(false),
    //    id(id)
    //    {
    //
    //    }
};

using RigidBody3dComponent = RigidBodyComponent<3>;
using RigidBody2dComponent = RigidBodyComponent<2>;

inline void set_X(RigidBody3dComponent& rb,
                  const v3f& x)
{
    rb.X = x;
}

inline void set_X(RigidBody2dComponent& rb,
                  const v2f& x)
{
    rb.X = x;
}

inline void set_X(RigidBody2dComponent& rb,
                  const v3f& x)
{
    set_X(rb, xy(x));
}

inline void set_R(RigidBody3dComponent& rb,
                  const m3f& R)
{
    rb.R = R;
    rb.Ri = transpose(R);
    
    rb.Q = quatf {R};
    rb.Q.normalize();
}

inline void set_R(RigidBody2dComponent& rb,
                  float R)
{
    rb.R = R;
}

inline void set_R(RigidBody2dComponent& rb,
                  const m3f& R)
{
    set_R(rb, extract_Euler_angle_z(R));
    
//    rb.R = extract_Euler_angle_z(R);
    // Extract z-rotation
//    rb.R = atan2(R.m21, R.m11);
}

inline void set_Q(RigidBody3dComponent& rb,
                  const quatf& Q)
{
    rb.Q = Q;
    rb.Q.normalize();

    rb.R = m3f {Q};
    rb.Ri = transpose(rb.R);
}

inline void apply_R(RigidBody3dComponent& rb,
                    const m3f& R)
{
    set_R(rb, R * rb.R);
}

inline void set_V(RigidBody3dComponent& rb,
                  const v3f& V)
{
    rb.V = V;
}

inline void set_V(RigidBody2dComponent& rb,
                  const v3f& V)
{
    rb.V = xy(V);
}

inline void set_W(RigidBody3dComponent& rb,
                  const v3f& W)
{
    rb.W = W;
}

inline void set_W(RigidBody2dComponent& rb,
                  const v3f& W)
{
    rb.W = W.x;
}

inline void set_XR(RigidBody3dComponent& rb,
                   const m4f& M)
{
    set_X(rb, extract_translation(M));
    set_R(rb, M.get_3x3());
}

/*
 Use X and Q from this and the previous frame (over dt) to derive
 V and W, respectively.
 
 X1 from X0 and V:
    X1 = X0 + V * dt
 V from X0 and X1:
    V = (X1 - X0)/dt
 
 Q1 from Q0 and W:
    Q1 = Q0 + dQ/dt * dt
 where
    dQ/dt = 0.5 * [0, w]' * Q
 W from Q0 and Q1:
    W = 2 * qQ/dt * Q^-1
 where
    qQ/dt = (Q1 - Q0)/dt
 */
inline void set_VW_by_differentiation(RigidBody3dComponent& rb,
                                      float dt)
{
    if (dt < std::numeric_limits<float>::epsilon()) return;
    
    const float idt = 1.0f/dt;
    
    const v3f V = (rb.X - rb.X_prev) * idt;
    set_V(rb, V);
    
    const quatf Qdot = (rb.Q + rb.Q_prev*-1.0f) * idt;
    const v3f W = Qdot.get_W_from_Qdot(rb.Q);
    set_W(rb, W);
}

inline void set_VW_by_differentiation(RigidBody2dComponent& rb,
                                      float dt)
{
    if (dt < std::numeric_limits<float>::epsilon()) return;
    
    const float idt = 1.0f/dt;
    
    const v2f V = (rb.X - rb.X_prev) * idt;
    set_V(rb, xy0(V));
    
    const float W = (rb.R - rb.R_prev) * idt;
    set_W(rb, x00(W));
}

inline m4f get_transform(const RigidBody3dComponent& rb)
{
    return set_translation(m4f(rb.R), rb.X);
}

inline m4f get_transform(const RigidBody2dComponent& rb)
{
    return set_translation(m4f::rotation_z(rb.R), xy0(rb.X));
}

inline m4f get_inverse_transform(const RigidBody3dComponent& rb)
{
    XI_ASSERT(0, "not tested");
    return transpose(m4f(rb.R)) * m4f::translation(-rb.X);
}

inline m4f get_inverse_transform(const RigidBody2dComponent& rb)
{
    XI_ASSERT(0, "not tested");
    return m4f::rotation_z(-rb.R) * m4f::translation(-xy0(rb.X));
}

inline void apply_impulse(RigidBody3dComponent& rb,
                          const v3f& p,
                          const v3f& r)
{
    if (rb.is_static) return;
    
    rb.V += p * rb.im;
    rb.V *= rb.V_mask;
    
    rb.W += rb.iI_w * (r % p);
    rb.W *= rb.W_mask;
}

inline void apply_impulse(RigidBody2dComponent& rb,
                          const v2f& p,
                          const v2f& r)
{
    if (rb.is_static) return;

    rb.V += p * rb.im;
    rb.V *= rb.V_mask;
    
    rb.W += cross(r, p) * rb.iI;
    rb.W *= rb.W_mask;
}

inline void apply_angular_impulse(RigidBody3dComponent& rb,
                                  const vec3f& l)
{
    if (rb.is_static) return;
    
    rb.W += rb.iI_w * l;
    rb.W *= rb.W_mask;
}

inline void apply_angular_impulse(RigidBody2dComponent& rb,
                                  float l)
{
    XI_ASSERT(0, "Check this");
    
    if (rb.is_static) return;
    
    rb.W += l * rb.iI;
    rb.W *= rb.W_mask;
}

inline void apply_force(RigidBody3dComponent& rb,
                        const v3f& f,
                        const v3f& r)
{
    if (rb.is_static) return;
    
    rb.F += f;
    rb.T += r % f;
}

inline void apply_force(RigidBody2dComponent& rb,
                        const v2f& f,
                        const v2f& r)
{
    //if(isStatic) return;
    //
    //this->F += F;
    //T += r % F;

    if (rb.is_static) return;
    
    rb.F += f;
    rb.T += cross(r, f);
}

inline void apply_force_ignorestatic(RigidBody3dComponent& rb,
                                     const v3f& f,
                                     const v3f& r)
{
    rb.F += f;
    rb.T += r % f;
}

inline void apply_torque(RigidBody3dComponent& rb,
                         const v3f& t)
{
    if (rb.is_static) return;
    
    // Check this
    rb.T += t;
}

inline void apply_torque(RigidBody2dComponent& rb,
                         float t)
{
    if (rb.is_static) return;
    
    rb.T += t;
}

inline void make_static(RigidBody3dComponent& rb)
{
    rb.m = XI_FINF;
    rb.I = m3f(XI_FINF);
    
    rb.im = 0.0f;
    rb.iI = m3f_zero;
    
    rb.is_static = true;
}

inline void make_static(RigidBody2dComponent& rb)
{
    rb.m = XI_FINF;
    rb.I = XI_FINF;
    
    rb.im = 0.0f;
    rb.iI = 0.0f;
    
    rb.is_static = true;
}

inline void set_mass(RigidBody3dComponent& rb,
                     float m,
                     const mat3f &I)
{
    if (rb.is_static) return;
    
    rb.m = m;
    rb.I = I;
    
    rb.im = 1.0f/m;
    rb.iI = I.inverse();
    
#if 1
    // "Infinite" rotational mass
    if (rb.W_mask.x == 0.0f)
    {
        rb.I.m11 = XI_FINF;
        rb.iI.m11 = 0.0f;
    }
    if (rb.W_mask.y == 0.0f)
    {
        rb.I.m22 = XI_FINF;
        rb.iI.m22 = 0.0f;
    }
    if (rb.W_mask.z == 0.0f)
    {
        rb.I.m33 = XI_FINF;
        rb.iI.m33 = 0.0f;
    }
#endif
}

inline void set_mass(RigidBody2dComponent& rb,
                     float m,
                     float I)
{
    if (rb.is_static) return;
    
    rb.m = m;
    rb.I = I;
    
    rb.im = 1.0f/m;
    rb.iI = 1.0/I;
    
#if 1
    // "Infinite" rotational mass"
    if (rb.W_mask == 0.0f)
    {
        rb.iI = 0.0f;
    }
#endif
}

#if 0
//
// Compute from colliders: center-of-mass (COM), mass, inertia tensor.
//
// Offsets body, geometries and initial position anchor to the new COM
//
void SetMassFromColliders(RigidBodyComponent& rb,
                          const std::vector<Handle<ColliderBase>> colliders,
                          float rho = DensityDefault);
#endif

// Done in PhysicsSystem
//inline void shade(RigidBodyComponent& rb)
//{
//    // World space inverted inertia tensor
//    rb.iI_w =  rb.R * rb.iI * rb.Ri;
//    
//    // Transform colliders and AABB to world space
//    //        AABB_w.reset();
//    //        for (auto c : colliders)
//    //        {
//    //            c->vertex_shade(R, X);
//    //            AABB_w.grow(c->AABB_w);
//    //        }
//}

// apply impulses etc

}; // RigidBody


#endif /* RigidBody_hpp */
