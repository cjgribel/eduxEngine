//
//  angle_constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-24.
//
//

#ifndef tau3d_angle_constraint_h
#define tau3d_angle_constraint_h

#include "constraint.h"

/*
 * "Angle" constraint
 * Removes 2 angular DoF's from rel motion of bodies
 * This is equivalent to the rotational part of a hinge joint (which constrains positions as well)
 */

/*
 derivation:
 http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?t=8686
 
 Say you have a joint frame u,v,w on both bodies.
 The w axis on body 1 defines your hinge.
 
 // 1. Define the position constraints
 C1 = u2 * w1
 C2 = v2 * w1
 
 // 2. Build the time derivative
 dC1/dt = J1 * v = cross( u2, w1 ) * ( omega2 - omega1 )
 dC2/dt = J2 * v = cross( v2, w1 ) * ( omega2 - omega1 )
 
 // 3. Identify the Jacobian by inspection
 J1 = [ 0 | -cross( u2, w1 ) | 0 | cross( u2, w1 ) ]
 J2 = [ 0 | -cross( v2, w1 ) | 0 | cross( v2, w1 ) ]
 
 // 4. Build inverse effective mass K = J * M^-1 * JT 
 //    (the first '*' is a dot product and the second '*' is a matrix times 
 //    vector multiplication -> the result is a scalar)
 K1 = cross( u2, w1 ) * [ ( invI1 + invI2 ) * cross( u2, w1 ) ]
 K2 = cross( v2, w1 ) * [ ( invI1 + invI2 ) * cross( v2, w1 ) ]
 
 // 5. Solve (use 2. to compute J*v and choose beta between 0.1 - 0.2)
 DeltaLambda1 = -( J1 * v + beta * C1 / dt ) / K1
 DeltaLambda2 = -( J2 * v + beta * C2 / dt ) / K2
 
 */
class angle_constraint_t : public angular_constraint_t
{
public:
    
//    mat3f frameA = linalg::mat3f_identity;
//    mat3f frameB = linalg::mat3f_identity;
    
    // Angular part
    
    // (Remove) Angle/Hinge constraint
//    vec3f uA, uAw;                  // reference vector (axle) (body A)
//    vec3f uB, vB, uBw, vBw;         // rotation vectors to constrain (body B)

    vec3f zA, xB, yB;               // World frame vectors
    
    float biasu, biasv;             // Baumgarte bias
    float iKu, iKv;                 // Effective mass
    
    // Limits part
    
    bool limits_active = false;     // Constraint flag
    float theta_max;                // Min/max angle
    
    vec3f v_limits;                 // Constraint vector
    float bias_limits;              // Bias
    float iK_limits;                // Effectiv mass
    bool limits_ctr_active;         // Actvation flag
    float lambda_limits_acc;        // Impulse accumulation (unilateral constraint)
    
    angle_constraint_t(body_t *bodyA, body_t *bodyB) : angular_constraint_t(bodyA, bodyB)
    {
//        this->uA = uA;
//        this->uB = uB;
//        this->vB = vB;
//        this->uA.normalize();
//        this->uB.normalize();
//        this->vB.normalize();
    }
    
    void set_limits(float angle_max,
                    const mat3f &frameA = linalg::mat3f_identity,
                    const mat3f &frameB = linalg::mat3f_identity)
    {
        theta_max = angle_max;
        limits_active = true;
        
        this->frameA = frameA;
        this->frameB = frameB;
    }
    
    void pre_solve()
    {
#define angle_ctr_ERP 0.2f
#define limits_ctr_ERP 0.2f
#define limits_ctr_tol (2.0f*fTO_RAD)
        
        // Alt for clarity
        //mat3f frameA = linalg::mat3f_identity, frameB = linalg::mat3f_identity;
        mat3f frameAw = bodyA->R * frameA;
        mat3f frameBw = bodyB->R * frameB;
        vec3f xA = frameAw.col[0];
//        vec3f yA = frameAw.col[1];
        zA = frameAw.col[2];
        xB = frameBw.col[0];
        yB = frameBw.col[1];
//        vec3f zB = frameBw.col[2];
        
//        uAw = bodyA->R*uA; // = z of frame A ("axle")
//        uBw = bodyB->R*uB; // = x of frame B
//        vBw = bodyB->R*vB; // = y of frame B
//        uAw = zA;
//        uBw = xB;
//        vBw = yB;
        
        //
        // Rotation constraint
        //
        
        // Position constraint, C = 0
        float Cu = zA.dot(xB);
        float Cv = zA.dot(yB);
        // bias
        biasu = Cu * angle_ctr_ERP * SIM_IDT;
        biasv = Cv * angle_ctr_ERP * SIM_IDT;
        
        // Effective mass
        vec3f cru = xB % zA;
        vec3f crv = yB % zA;
        mat3f iI_w_sum = bodyA->iI_w + bodyB->iI_w;
        iKu = 1.0f/( cru.dot(iI_w_sum * cru) );
        iKv = 1.0f/( crv.dot(iI_w_sum * crv) );
        
        //
        // Angle limits constraint
        //
        
        if (limits_active)
        {
            
            // Angle
            // No need to transform bases (as in conetwist),
            // because the z-axes should be aligned by design of this ctr
            float theta = acos( xB.dot(xA) );
            float theta_err = theta - theta_max;
            
            // Activate
            limits_ctr_active = (theta_err > 0);
            
            // Constraint vector = xA % aB
            // Undefined for small angles, but then limits are not active
            v_limits = -xA % xB;
            
            // Position constraint: C = 0
            float C_limits = -theta_err + limits_ctr_tol; /* - init_theta */;
            bias_limits = C_limits * limits_ctr_ERP * SIM_IDT;
            
            // Inverse effective mass
            iK_limits = 1.0f/( v_limits.dot(iI_w_sum * v_limits) );
            
            lambda_limits_acc = 0;
        }
    }
    
    void solve()
    {
        vec3f P_rotation = {0,0,0};
        vec3f P_limits = {0,0,0};
        
        //
        // Rotation constraint
        //
        
        // Velocity constraint, Cdot = Jv = 0
        float Cdotu = (xB % zA).dot(bodyB->W - bodyA->W);
        float Cdotv = (yB % zA).dot(bodyB->W - bodyA->W);
        // impulse, lambda = -(Jv+bias)/(J*M^-1*J^T)
        float lambdau = -(Cdotu + biasu)*iKu;
        float lambdav = -(Cdotv + biasv)*iKv;
        
        // Create impulses, P = J^T lambda
        vec3f Pu = (zA % xB) * lambdau;
        vec3f Pv = (zA % yB) * lambdav;
        P_rotation = Pu + Pv;
        
        //
        // Angle limits constraint
        //

        if (limits_active && limits_ctr_active)
        {
            // Velocity constraint
            float Cdot_limits = (bodyB->W - bodyA->W).dot(v_limits);
            float lambda_limits = -(Cdot_limits + bias_limits)*iK_limits;
            
            // Impulse magnitude
            float lambda_limits_tot;
            
            // Constraint is unilateral to clamp
            lambda_limits_tot = clamp(lambda_limits_acc + lambda_limits, 0.0f, (float)fINF) - lambda_limits_acc;
            lambda_limits_acc += lambda_limits_tot;
            
            // Create impulse, P = J^T lambda
            P_limits = -v_limits * lambda_limits_tot;
        }
        
        // Apply impulse
        vec3f P = P_rotation + P_limits;
        bodyA->apply_angular_impulse(P);
        bodyB->apply_angular_impulse(-P);
    }
};

/*
 
 draft for a quaternion based angle constraint:
 
 */

#if 0

/*
 * constrain rotation of body B to an axis u fixed to body A
 *
 *
 *
 * angle level:
 * rotation of Q around u ("swing") = R_rel
 *
 * angular velocity level:
 * rotation of dQ/dt around u = 0
 *
 note: this should be applied to the LEVEL level; and the angular level should just be corrected
 in world.cpp: quat Qdot = body->Q.getQdot(body->W);
 
 i.e
 C = twist = 0
 Cdot = d/dt twist = 0 -- decompose Qdot too?
 
 * decompose rotation quaternion into swing (rotation of axis - disallowed) and twist (rotation around axis - allowed)
 * 2. eliminate swing by applying -swing to the original rotation quaternion
 *
 
 */
class angle_constraint_t : public angular_constraint_t
{
public:
    vec3 u;     // rotation allowed only around this axis, expressed in body A's frame
    vec3 uw;     // world space version of u
    //    vec3 C;     // body B's non-aligned rotation around w, i.e. error on angle level
    vec3 bias;
    //    vec3 Cdot;  // body B's non-aligned angular velocity around w, i.e. error on angular velocity level
    mat3 iK;
    
    angle_constraint_t(body_t *bodyA, body_t *bodyB, const vec3 &u) : angular_constraint_t(bodyA, bodyB)
    {
        this->u = u;
        this->u.normalize();
    }
    
    void pre_solve()
    {
#define angle_ctr_ERP 0.2f
        
        // constraint vector in world space
        uw = bodyA->R * u;
        
        // find body B's rotation perpendicular to w (swing) = its error on angle level
        quat Cq, twist; // not used
        bodyB->Q.decompose_rotation(uw, twist, Cq);
        // retrieve angular velocity vector from Cq (treat Cq as an angular difference over dt)
        vec3 Cw = Cq.get_W_from_Qdot(bodyB->Q);
        bias = Cw * angle_ctr_ERP * SIM_IDT;
        
        // effective mass ???
        iK = (bodyA->iI_w + bodyB->iI_w).inverse(); //bodyB->iI_w.debugPrint();
        
        /*
         // angular bias
         C_ang = bodyB->R - bodyA->R + R_rel;
         C_ang = clampf(C_ang, -maxCorr_ang, maxCorr_ang);
         bias_ang = C_ang * ERP_ang * idt;
         
         // angular constraint effective mass
         mc_ang = 1.0f / (bodyA->iI + bodyB->iI);
         */
    }
    
    void solve()
    {
        // angular velocity quaternion of body B
        quat Qdot = bodyB->Q.getQdot(bodyB->W);
        if (Qdot.length() < 0.0001) return;
        
        quat Qu_swing, twist;
        bodyB->Q.decompose_rotation(uw, twist, Qu_swing);
        // component perpendicular to w = its error on velocity level
        quat bodyB_swing;
        Qdot.decompose_rotation(uw, twist, bodyB_swing);
        //bodyB_swing.debugPrint(); printf("\n");
        vec3 Cdot = bodyB_swing.get_W_from_Qdot(Qu_swing);
        //Cdot.debugPrint();
        
        //        vec3 Qdotw = Qdot.get_W_from_Qdot(bodyB->Q);
        //        printf("W before "); bodyB->W.debugPrint();
        //        printf("W after  "); Qdotw.debugPrint();
        
        // apply impulse
        vec3 P = iK * (Cdot + bias*0); //P.debugPrint();
        bodyA->W = bodyA->iI_w * P;
        bodyB->W += bodyB->iI_w * P;
        
        /*
         // angular velocity constraint (is ideally = 0)
         Cdot_ang = bodyB->W - bodyA->W;
         
         // calculate and apply angular impulse
         P_ang = -mc_ang * (Cdot_ang + bias_ang);
         bodyA->W -= bodyA->iI * P_ang;
         bodyB->W += bodyB->iI * P_ang;
         */
    }
};

#endif

#endif
