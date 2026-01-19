//
//  angle_constraint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-24.
//
//

#ifndef tau3d_angular_limits_constraint_h
#define tau3d_angular_limits_constraint_h

#include "constraint.h"


/*
 quaternion derivation notes
 
 step 1: implement the hinge ctr using quaternions
 step 2: add limits to third axis
 
 main source: Game Physics Pearls, c9

 [PCV]
 P_hinge = [ 0 1 0 0  ]
           [ 0 0 1 0  ]
           [ 0 0 0 1  ]
 
 [HINGE]
 
 – hinge with limits on the free axis, i.e. z:
 
 two original constraints unchanged
 third ctr (my interpretation):
 pos,
 theta - limit_theta = 0, where theta = 2*atan(qs/qz)
 vel,
 thetadot = [-qz 0 0 qs] * qdot,
 where [-qz 0 0 qs] are part of the third row of P_hinge of the Jacobian
 ->  P_hinge_limits = [ 0   1 0 0  ]
                      [ 0   0 1 0  ]
                      [ -qz 0 0 qs ]
 
 so?:
 for a hinge with limitations on the free axis, e.g. the z-axis,
 the two first ctr's, on the locked axes x and y, are derived from C = q = 0, Cdot = J*qdot = 0
 for the z-axis, the ctr is derived from C = theta - theta_limit = 0, Cdot = J*thetadot = 0 (if active?)
 lambda is computed as lambda = -(Cdot + bias)/iK
 the impulse is P = J^T * lambda
 
 
 relative orinetation: q
 P_hinge = [ 0 1 0 0  ]
           [ 0 0 1 0  ]
 (assumed bodies are aligned and locked along z)
 
 angle level: C = 0
    C = P_hinge * q = 0
 i.e. 2x4 * 4x1 -> 2x1 = the error to be corrected, for each respective axis (x and y here)
 acos(error) would be the error in radians (see definition on page 199 - the 'error' is the result of a dot product)
 limits -> allowed range for acos(error)
 for a full 3dof angle limits ctr -> use the 'lock' constraint instead
 
 velocity level: , Cdot = Jv=0
 E(q) and G(q) are 4x3 matrices formed from q
 Jacobians of body 1, 2
    J_hinge = 0.5 * P_hinge * E(q)^T,
    J_hinge = 0.5 * P_hinge * G(q)^T

 i.e. each J_hinge is 2x4 * 4x3 -> 2x3
 ok - 2 rows -> two vectors are constrained, i.e. two constraint equations
 expected 4 elements to match the orientation quat of each body, i.e. Jv = 0,
 but the first element, the angle, doesn't matter anyway on the velocity level!
 
 misc: Qdot = 0.5*Q*w
 
 idea
 C = qA - qB
 Cdot = dC/dt = Jv = J [wA wB ]T, where angular vel's wA and wB are quaternions
 
 
 
 quaternion for 'relative orientation'? Quaternion relative = Quaternion.Inverse(a) * b;
 http://answers.unity3d.com/questions/35541/problem-finding-relative-rotation-from-one-quatern.html
 
 http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?t=8830
 I am representing the orientation of a rigid body as a quaternion and the angular velocity as a vector. I am attempting to implement an angle constraint directly from the quaternion without having to store a vector alongside the quaternion in the rigid body class to represent the orientation.
 C = Qa - Qb - Qi where Qi is the initial difference in orientation between the bodies
 dC/dt = wa - wb which are the angular velocities of the bodies already known
 So the Jacobian is [ 0 1 0 -1]
 When I get to the bias calculation of bias = beta/dt * C, this will return a quaternion
 So -Jv = wb - wa - bias will yield and incompatible equation as we are subtracting a quaternion from a vector. What is the preferred method without having to convert C to a vector at each frame?
 
 */

#if 0
/*
 angular limits constraint using quaternions
 
 impose angular limits to all 3 angular dof's on body A relative body B
 */
class angular_limits_constraint_quat_t : public angular_constraint_t
{
public:
    angular_limits_constraint_quat_t(body_t *bodyA, body_t *bodyB) : angular_constraint_t(bodyA, bodyB)
    {
    
    }
};
#endif

/*
 
 Constrain and limit (one, two?) angular DoF's - Euler derivation
 
 Note: this class currently blocks all angular DoF's
 Limits and limit state (active or not) are computed but not applied
 
 
 notes:	angular limits using dRoll/dPitch/dYaw (around x/y/z).
 constraint acts on all 3 angular dofs -> generalization of axle constraint
 uses u.v = [min_angle,max_angle]
 if all limits are [0,0], then all rotation is removed
 e.g. body A: right, up, B: right, up. then (ignoring limits) Aright.Bup=0 (roll), (Aright%Aup).Bup = 0 (pitch), Aright.(Bright%Bup)=0 (yaw)
 or in terms of bases (3x3 matrix, cardinal base per default), Ax.By=0 (roll), Az.By=0 (pitch), Ax.Bz=0 (yaw)
 */

/*
 angular limits constraint using Euler angles
 
 impose angular limits to all 3 angular dof's on body A relative body B
 
 note: doesn't handle limits upwards of +/-90 degrees well
 */

/*
 derivation for removal of 2 dofs:
 http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?t=8686
 
 Say you have a joint frame u,v,w on both bodies. The w axis on body 1 defines your hinge.
 
 // 1. Define the position constraints
 C1 = u2 * w1
 C2 = v2 * w1
 
 // 2. Build the time derivative
 dC1/dt = J1 * v = cross( u2, w1 ) * ( omega2 - omega1 )
 dC2/dt = J2 * v = cross( v2, w1 ) * ( omega2 - omega1 )
 
 // 3. Identify the Jacobian by inspection
 J1 = [ 0 | -cross( u2, w1 ) | 0 | cross( u2, w1 ) ]
 J2 = [ 0 | -cross( v2, w1 ) | 0 | cross( v2, w1 ) ]
 
 // 4. Build inverse effective mass K = J * M^-1 * JT (the first '*' is a dot product and the second '*' is a matrix times vector multiplication -> the result is a scalar)
 K1 = cross( u2, w1 ) * [ ( invI1 + invI2 ) * cross( u2, w1 ) ]
 K2 = cross( v2, w1 ) * [ ( invI1 + invI2 ) * cross( v2, w1 ) ]
 
 // 5. Solve (use 2. to compute J*v and choose beta between 0.1 - 0.2)
 DeltaLambda1 = -( J1 * v + beta * C1 / dt ) / K1
 DeltaLambda2 = -( J2 * v + beta * C2 / dt ) / K2
 
 */
#if 0
class angular_limits_constraint_euler_t : public angular_constraint_t
{
public:
    mat3f frameA, frameAw;   // body A constraint frame
    mat3f frameB, frameBw;   // body B constraint frame
    
    float min_roll = 0, max_roll = 0;   // angular limits around x
    float min_pitch = 0, max_pitch = 0; // angular limits around y
    float min_yaw = 0, max_yaw = 0;     // angular limits around z
    bool roll_limits_active, pitch_limits_active, yaw_limits_active;
    bool roll_upper_limits_active, pitch_upper_limits_active, yaw_upper_limits_active;
    
    float lambda_roll_acc, lambda_pitch_acc, lambda_yaw_acc;
    
    vec3f Axw, Azw, Byw, Bzw;    // frame axes in world space
    vec3f Ayw, Bxw;
    
    float roll_bias, pitch_bias, yaw_bias;  // error correcting biases
    float roll_iK, pitch_iK, yaw_iK;         // effective masses
    
    
    vec3f uA, uAw;                   // reference vector (axle) (body A)
    vec3f uB, vB, uBw, vBw;          // rotation vectors to constrain (body B)
    
    float biasu, biasv;             // bias
    float iKu, iKv;                 // effective mass
    
    angular_limits_constraint_euler_t(body_t *bodyA, body_t *bodyB) : angular_constraint_t(bodyA, bodyB)
    {
        this->uA = uA;
        this->uB = uB;
        this->vB = vB;
        this->uA.normalize();
        this->uB.normalize();
        this->vB.normalize();
    }
    
    void set_limits(float min_roll, float max_roll,
                    float min_pitch, float max_pitch,
                    float min_yaw, float max_yaw,
                    const mat3f &frameA = mat3f(1), const mat3f &frameB = mat3f(1)) // default frames to identities
    {
        this->min_roll = min_roll; this->max_roll = max_roll;
        this->min_pitch = min_pitch; this->max_pitch = max_pitch;
        this->min_yaw = min_yaw; this->max_yaw = max_yaw;
        this->frameA = frameA;
        this->frameB = frameB;
    }
    
    void pre_solve()
    {
#define angular_limits_ctr_ERP 0.2f
#define angular_tol 5.0f*fTO_RAD
        
        // rotate frames
        frameAw = bodyA->R*frameA;
        frameBw = bodyB->R*frameB;
        // extract axes
        Axw = frameAw.column(0);
        Ayw = frameAw.column(1);
        Azw = frameAw.column(2);
        Bxw = frameBw.column(0);
        Byw = frameBw.column(1);
        Bzw = frameBw.column(2);
        
        // dbg
        uAw = frameAw.column(0);
        uBw = frameBw.column(1);
        vBw = frameBw.column(2);
//        uAw = bodyA->R*uA;
//        uBw = bodyB->R*uB;
//        vBw = bodyB->R*vB;
        
        // position constraint & bias
        float Cu = uAw.dot(uBw);
        float Cv = uAw.dot(vBw);
        biasu = Cu * angle_ctr_ERP * SIM_IDT;
        biasv = Cv * angle_ctr_ERP * SIM_IDT;
        
        // idea:
        // extract angle (0-180) from Az.Bz
        // decide max/min from sign of Az.By
        // Az.By will range 0 - -180 ('min'), and 0-180 ('max')
        // allows limits ranges to -180+tol -> 180-tol
        //
        // extract angle: theta = acos(Azw.dot(Bzw))-fPI/2; (or? asin(Azw.dot(Bzw));)
        // if Az.By < 0
        //      check min limit
        // else
        //      check max limit
        
        // dor = 0
        //Ax.By=0 (roll), Az.By=0 (pitch), Ax.Bz=0 (yaw)
        //Az.By=0 (roll), Ax.Bz=0 (pitch), Ax.By=0 (yaw)
        // angle = 0 (acos dot) = 0
        // Ax.Bx=0 (roll),
//        float theta_roll = acos(Azw.dot(Byw))-fPI/2;
//        float theta_pitch = acos(Axw.dot(Bzw))-fPI/2;
//        float theta_yaw = acos(Axw.dot(Byw))-fPI/2;
        float theta_roll = acos(Azw.dot(Bzw));
        float theta_pitch = acos(Axw.dot(Bxw));
        float theta_yaw = acos(Ayw.dot(Byw));
        //printf("roll %f, pitch %f, yaw %f\n", Azw.dot(Byw), theta_pitch*fTO_DEG, theta_yaw*fTO_DEG);
        float C_roll, C_pitch, C_yaw; // <- these should be cos (...) etc
        
        roll_bias = 0;
        pitch_bias = 0;
        yaw_bias = 0;
        
        roll_limits_active = false;
        if (Azw.dot(Byw) < 0.0f)
        {
            if (-theta_roll < min_roll)
            {
                C_roll = Azw.dot(Byw) - sin(min_roll);
                roll_bias = C_roll * angular_limits_ctr_ERP * SIM_IDT;
                roll_upper_limits_active = false;
                roll_limits_active = true;
            }
        } else {
            if (theta_roll > max_roll)
            {
                C_roll = Azw.dot(Byw) - sin(max_roll);
                roll_bias = C_roll * angular_limits_ctr_ERP * SIM_IDT;
                roll_upper_limits_active = true;
                roll_limits_active = true;
            }
        }
        //printf("%f, err %f, %f, %f, %s-%s\n", Azw.dot(Byw), C_roll, sin(min_roll), sin(max_roll), roll_limits_active?"1":"0", roll_upper_limits_active?"upper":"lower");
        
        pitch_limits_active = false;
        if (Axw.dot(Bzw) < 0.0f)
        {
            if (-theta_pitch < min_pitch) {
                // C_roll = cos deg error
                pitch_upper_limits_active = false;
                pitch_limits_active = true;
            }
        } else {
            if (theta_pitch > max_pitch) {
                // ...
                pitch_upper_limits_active = true;
                pitch_limits_active = true;
            }
        }
        
        yaw_limits_active = false;
        if (Axw.dot(Byw) < 0.0f)
        {
            if (-theta_yaw < min_yaw) {
                // C_roll = cos deg error
                yaw_limits_active = true;
            }
        } else {
            if (theta_yaw > max_yaw) {
                // ...
                yaw_limits_active = true;
            }
        }
        
//        if (theta_pitch < min_pitch || theta_pitch > max_pitch)
//        {
//            if (theta_pitch < min_pitch) {
//                C_pitch = -clamp<float>(-theta_pitch + min_pitch - angular_tol, 0.0f, fINF);
//                pitch_upper_limits_active = false;
//            }
//            else {
//                C_pitch = clamp<float>(theta_pitch - max_pitch - angular_tol, 0.0f, fINF);
//                pitch_upper_limits_active = true;
//            }
//            pitch_limits_active = true;
//        } else
//            pitch_limits_active = false;
//        
//        if (theta_yaw < min_yaw || theta_yaw > max_yaw)
//        {
//            if (theta_yaw < min_yaw) {
//                C_yaw = -clamp<float>(-theta_yaw + min_yaw - angular_tol, 0.0f, fINF);
//                yaw_upper_limits_active = false;
//            }
//            else {
//                C_yaw = clamp<float>(theta_yaw - max_yaw - angular_tol, 0.0f, fINF);
//                yaw_upper_limits_active = true;
//            }
//            yaw_limits_active = true;
//        } else
//            yaw_limits_active = false;
        
        //printf("roll %d, pitch %d, yaw %d\n", roll_limits_active?1:0, pitch_limits_active?1:0, yaw_limits_active?1:0);
        
        // effective mass
        vec3f cru = uBw % uAw;
        vec3f crv = vBw % uAw;
        iKu = 1.0f/( cru.dot((bodyA->iI_w + bodyB->iI_w) * cru) );
        iKv = 1.0f/( crv.dot((bodyA->iI_w + bodyB->iI_w) * crv) );
        
        mat3f iI_w_sum = bodyA->iI_w + bodyB->iI_w;
        vec3f crr = Byw%Azw;
        roll_iK = 1.0f/( crr.dot(iI_w_sum * crr) );
        vec3f crp = Bzw%Axw;
        pitch_iK = 1.0f/( crp.dot(iI_w_sum * crp) );
        vec3f cry = Byw%Axw;
        yaw_iK = 1.0f/( cry.dot(iI_w_sum * cry) );
        
        lambda_roll_acc = 0;
        lambda_pitch_acc = 0;
        lambda_yaw_acc = 0;
    }
    
    // Approach --> Cone-twist
    //
    // Cone = xy-plane - impose limits on roll/x and pitch/y
    // Twist - around z.
    
    void solve()
    {
        // velocity constraint
        float Cdotu = (uBw % uAw).dot(bodyB->W - bodyA->W); // x%z
        float Cdotv = (vBw % uAw).dot(bodyB->W - bodyA->W); // y%z
//        float lambdau = -(Cdotu + biasu*1)*iKu;
//        float lambdav = -(Cdotv + biasv*1)*iKv;
        
        float Cdot_roll = (Byw%Azw)/*Axw*/.dot(bodyB->W - bodyA->W);
        float Cdot_pitch = (Bzw%Axw)/*Ayw*/.dot(bodyB->W - bodyA->W);
        float Cdot_yaw = (Byw%Axw)/*-Azw*/.dot(bodyB->W - bodyA->W);
        float lambda_roll = -(Cdot_roll + roll_bias*0)*roll_iK;
        float lambda_pitch = -(Cdot_pitch + pitch_bias*0)*pitch_iK;
        float lambda_yaw = -(Cdot_yaw + yaw_bias*0)*yaw_iK;
        
        float lambda_roll_tot;
        if (roll_limits_active)
        {
            if (roll_upper_limits_active)
            {
                lambda_roll_tot = clamp(lambda_roll_acc + lambda_roll, (float)fNINF, 0.0f) - lambda_roll_acc;
                lambda_roll_acc += lambda_roll_tot;
            }
            else
            {
                lambda_roll_tot = clamp(lambda_roll_acc + lambda_roll, 0.0f, (float)fINF) - lambda_roll_acc;
                lambda_roll_acc += lambda_roll_tot;
            }
        }
        
        float lambda_pitch_tot;
        if (pitch_limits_active)
        {
            if (pitch_upper_limits_active)
            {
                lambda_pitch_tot = clamp(lambda_pitch_acc + lambda_pitch, (float)fNINF, 0.0f) - lambda_pitch_acc;
                lambda_pitch_acc += lambda_pitch_tot;
            }
            else
            {
                lambda_pitch_tot = clamp(lambda_pitch_acc + lambda_pitch, 0.0f, (float)fINF) - lambda_pitch_acc;
                lambda_pitch_acc += lambda_pitch_tot;
            }
        }
        
        // apply impulse
//        vec3f Pu = (uAw % uBw) * lambdau;
//        vec3f Pv = (uAw % vBw) * lambdav;
//        vec3f Puv = Pu + Pv;
//        bodyA->apply_angular_impulse(Puv);
//        bodyB->apply_angular_impulse(-Puv);
        
        vec3f P_roll = (Azw%Byw)/*-Axw*/ * lambda_roll; // lambda_roll_tot for clamping
        vec3f P_pitch = (Axw%Bzw)/*-Ayw*/ * lambda_pitch; // lambda_pitch_tot for clamping
        vec3f P_yaw = (Axw%Byw)/*Azw*/ * lambda_yaw;
//        if (!roll_limits_active) P_roll = {0,0,0};
//        if (!pitch_limits_active) P_pitch = {0,0,0};
//        if (!yaw_limits_active) P_yaw = {0,0,0};
        vec3f P = P_roll*0 + P_pitch + P_yaw;
        bodyA->apply_angular_impulse(P);
        bodyB->apply_angular_impulse(-P);
        
        // body A: right (up)
        // body B: up front
//        vec3 vAw = bodyA->R*vec3(0,1,0);
//        printf("%f\n", acos(vAw.dot(uBw))*fTO_DEG);
//        if ( fabs(acos(uAw.dot(uBw))-3.14/2) > 3.14/8 ||
//            fabs(acos(uAw.dot(vBw))-3.14/2) > 3.14/8 ||
//            fabs(acos(vAw.dot(uBw))) > 3.14/8 ) {
//            bodyA->apply_angular_impulse(Puv);
//            bodyB->apply_angular_impulse(-Puv);
//        }
    }
};
#endif

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
