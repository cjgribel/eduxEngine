//
//  prismatic_constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-24.
//
//

#ifndef tau3d_prismatic_constraint_h
#define tau3d_prismatic_constraint_h

#include "constraint.h"

/*
 * "Prismatic/slider" constraint
 *
 * Remove two linear DoF's from rel motion of bodies (constrain to vector)
 *
 * Formulation:
 *
 * l = reference direction
 * u = joint vector = (xA + rA) - (xB + rB)
 *
 * Position level constraint: C = l.d = 0
 *
 * Velocity level constraint: Jv = 0, where
 *
 * d/dt( l.u ) =
 * dl/dt.u + l.du/dt =
 * (wA*l).u + l.(vB + wB x rB - vA - wA x rA) =
 * | -l         |T | vA |
 * | l x (u+rA) |  | wA | = Jv = 0      [wrong sign of cross product in my MSc thesis]
 * | l          |  | vB |
 * | rB x l     |  | wB |               [wrong index in my MSc thesis]
 *
 * Inverse of effective mass: iK = ( J * M^-1 * J^T )^-1
 */
class prismatic_constraint_t : public linear_constraint_t
{
private:
    float iK, bias;
public:
    vec3f l, lw;         // constraint vector
    vec3f rAw, rBw;  // world anchor points
    vec3f u;         // vector between anchor points
    
    prismatic_constraint_t(body_t *bodyA, body_t *bodyB, const vec3f &rA, const vec3f &rB, const vec3f &l) : linear_constraint_t(bodyA, bodyB, rA, rB)
    {
        this->l = l;
        this->l.normalize();
    }
    
    void pre_solve()
    {
#define slider_ctr_C_max 5000.0f
#define slider_ctr_ERP 0.2f
        
        // world anchor points
        rAw = bodyA->R * rA;
        rBw = bodyB->R * rB;
        u = bodyB->X + rBw - bodyA->X - rAw;
        
        // world constraint vector
        lw = bodyA->R * l;
        
        // bias
        float C = lw.dot(u);
        C = clamp(C, -slider_ctr_C_max, slider_ctr_C_max);
        bias = C * slider_ctr_ERP * SIM_IDT;
        
        // effective mass
        vec3f crAl = lw % (u + rAw);
        vec3f crBl = rBw % lw;
        iK = 1.0f/( bodyA->im + bodyB->im + (crAl*bodyA->iI_w).dot(crAl) + (crBl*bodyB->iI_w).dot(crBl) );
    }
    
    void solve()
    {
        // velocity constraint
        float Cdot = -lw.dot(bodyA->V) + (lw%(u+rAw)).dot(bodyA->W) + lw.dot(bodyB->V) + (rBw%lw).dot(bodyB->W);
        
        // apply impulse
        vec3f P = lw * (iK * (Cdot + bias));
        bodyA->apply_impulse(P, u + rAw);
        bodyB->apply_impulse(-P, rBw);
    }
    
    void render()
    {
#if 0
        render_line(bodyA->X, bodyA->X+rAw, 1, {0,1,0,1});
        render_line(bodyA->X+rAw, bodyA->X+rAw+u, 1, {1,0,0,1});
#endif
    }
};

#endif
