//
//  ConeTwistConstraint.cpp
//  xiengine
//
//  Created by Carl Johan Gribel on 2021-08-25.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include "ConeTwistConstraint.hpp"

void ConeTwistConstraint::set_limits(float cone_max,
                                     float twist_max,
                                     const m3f &frameA,
                                     const m3f &frameB)
{
    this->cone_max = cone_max;
    this->twist_max = twist_max;
    
    this->frameA = frameA;
    this->frameB = frameB;
}

void ConeTwistConstraint::pre_solve(float dt_inverse,
                                    const RigidBody3dComponent& rbA,
                                    const RigidBody3dComponent& rbB)
{
#define conetwist_ctr_ERP 0.6f /*0.2f*/
#define conetwist_angular_tol 5.0f*fTO_RAD
    
    // rotate frames
    frameAw = rbA.R*frameA;
    frameBw = rbB.R*frameB;
    // extract axes
    Axw = frameAw.column(0);
    Ayw = frameAw.column(1);
    Azw = frameAw.column(2);
    Bxw = frameBw.column(0);
    Byw = frameBw.column(1);
    Bzw = frameBw.column(2);
    
    // Cone
    // Cone axis = frame z's
    
    // Ellipsoid cone - different limit angles in the x and y cone directions
    //
    // Need (good) angles of one z towards xz- and yz-planes of the oher frame
    // Good = project z-axis onto planes before calculating angles
    //
    // Is z-axis within ellipse?
    // Find an expression which can answer this based on the two angles (1)
    // -> Evaluate
    //
    // (1) Ellipse equation (area):
    //      (x/a)^2 + (y/b)^2 <= 0
    //      where a and b are scale factor in x and y (i.e. it ranges from -a to a and -b to b)
    // sine of 2x angle(2) limits -> a & b
    // sine of z-axis' angle(2) to zy-plane -> x, to xy-plane -> y
    //
    // (2) These angles can't be calculated via axis dots in the rotated frames
    // a): Take angle of one axis projected onto the plane of the other plane
    // b): Transform the entire frame onto the other (wrt one of the vetcors),
    //      then take direct angles of the other two
    //
    // Frame-transform: Bz -> Bz
    // Reveals correct 'rotation around' z (via x-x or y-y)
    // I.e. the twist angle
    // Cone angles are lost here
    // There are _two_ cone angles, so frame-transform might not even be useful here (unless used 2 times)
    //
    // Note that *any* implicit form can be used once (x,y) are obtained
    
#if 0
    // Ellipse experiments
    // project A's z-axis on B's xz-plane
    vec3f Bxz_n = Byw; // trivial
    vec3f Azw_Bxz = normalize( Azw - Bxz_n * Azw.dot(Bxz_n) ); //printf("%f\n", Azw.dot(Bxz_n));
    // Now take angle to B's z-axis
    float a = acos(Azw_Bxz.dot(Azw));
    // project A's z-axis on B's yz-plane
    vec3f Byz_n = Bxw; // trivial
    vec3f Azw_Byz = normalize( Azw - Byz_n * Azw.dot(Byz_n) );
    // Now take angle to B's z-axis
    float b = acos(Azw_Byz.dot(Azw));
    //float r = ( Bxz_n * Azw.dot(Bxz_n)).norm2();
    //float t = ( Byz_n * Azw.dot(Byz_n)).norm2();
    //printf("%f, %f (%f)\n", r, t, sin(45*fPI/180));
    //        printf("a %f, b %f\n", a*fTO_DEG, b*fTO_DEG);
#endif
#if 0
    // Ellipse experiments #2
    // Just project Az on B's xy-plane
    // Then take angle to B's x and y -> (x,y) for cone
    //        vec3f Bxy_n = Bzw; // trivial
    //        vec3f Azw_Bxy = normalize( Azw - Bxy_n * Azw.dot(Bxy_n) );
    //        // Now take angle to B's x- and y-axes
    //        float ax = acos(Azw_Bxy.dot(Bxw));
    //        float ay = acos(Azw_Bxy.dot(Byw));
    //        printf("ax %f, ay %f\n", ax*fTO_DEG, ay*fTO_DEG);
#endif
    
    
    // Circular cone case
    v_cone = Bzw % Azw; // = (0,0,0) if z's are aligned, but then ctr should not be enforced
    float cone_dot = Azw.dot(Bzw);
    float cone_theta = acos(cone_dot);
    float cone_theta_diff = cone_theta - cone_max;
    
    // Activate
    cone_ctr_active = ( cone_theta_diff > 0 );
    
    // Position constraint, C = 0
    float C_cone = -cone_theta_diff; /* - init_theta */;
    bias_cone = C_cone * conetwist_ctr_ERP * dt_inverse;
    
    // Inverse effective mass
    m3f iI_w_sum = rbA.iI_w + rbB.iI_w;
    iK_cone = 1.0f/( v_cone.dot(iI_w_sum * v_cone) );
    
    //printf("dot=%f, theta=%f (%f), max %f, %s\n", cone_dot, cone_theta, cone_theta*180.0/fPI, max_roll, cone_upper_ctr_active?"1":"0");
    //        printf("error %f\n", cone_theta-cone_max);
    
    
    // Twist
    //
    // Problem: don't want this impulse to interfere with the cone impulse
    // Also, don't want the twist angle to "depend" on the cone angle,
    // which seem to happens when dot product are taken between axes in
    // rotated frames - since the angle may be over the limit but not in the desired axis/plane
    //
    // Approach #1 (Below)
    // Transform frame B -> A, then check angle diff around z
    // This as a form of projection, which eliminates cone rotation
    //
    // Approach #2 (Similar to #1 and probably easier/cheaper)
    // Project one vector onto the plane pf the other before calculating angle via dot
    // E.g. angle between x-axes within xy-plane (twist); project the A x-axis onto the xy-plane of the other
    //
    // Approach #3
    // Project-approach again, but on the impulse vector
    //
    
    // Obtain twist angle
    // a) Ax dot By
    // b) Transform one basis into the other, and then Ax dot By
    //
    // Generate matrix R which transforms B such that Bz aligns with Az
    m3f R;
#if 0
    // Impl #1
    vec3f v = v_cone;
    float s = v_cone.norm2();
    float c = cone_dot;
    mat3f vskew = mat3f::skew(v);
    R = linalg::mat3f_identity + vskew + vskew*vskew * ((1.0f-c)/(s*s));
#endif
#if 1
    // Impl #2
    R = mat3f::rotation_to_vector(Bzw, Azw);
#if 0
    // Quaternion decompose test
    //
    mat3f frameAT = frameAw; frameAT.transpose();
    mat3f R_BtoA = frameAT * frameBw;
    quatf q = quatf(R_BtoA), qtwist, qswing;
    q.twistswing_decomposition({0,0,1}, qtwist, qswing); // <-- twist & cone angle's
    printf("%f, %f\n", qtwist.getEulerAngle()*fTO_DEG, qswing.getEulerAngle()*fTO_DEG);
#endif
#endif
    // Transform B to A
    m3f frameBw_A = R * frameBw;
    // Now check x-x angle (twist)
    v3f Bxw_A = frameBw_A.column(0);
    float twist_theta = acos( Bxw_A.dot(Axw) );
    //        /**/ twist_theta = acos( Bxw.dot(Axw) ); <- interfere with cone limits
    //        printf("twist %f\n", twist_theta*fTO_DEG);
    
#if 0
    // ** Experiment: test Impl #1 and #2 ***
    mat3f R2 = mat3f::rotation_to_vector(Bzw, Azw);
    // Invent a per-element matrix norm
    float maxe = 0;
    for (int i=0; i<9; i++)
        maxe = fmaxf(maxe, fabsf( R.array[i]-R2.array[i]) );
    printf("%f\n", maxe);
#endif
    
    // Activate
    //float twist_max = 40*fTO_RAD;
    float twist_theta_diff = twist_theta - twist_max;
    twist_ctr_active = (twist_theta_diff > 0);
    
    // Position constraint: C = 0
    float C_twist = -twist_theta_diff; /* - init_theta */;
    bias_twist = C_twist * conetwist_ctr_ERP * dt_inverse;
    
    // Inverse effective mass
    v_twist = -Axw % Bxw; // = (0,0,0) if x's are aligned (zero twist), but then ctr should not be enforced
    iK_twist = 1.0f/( v_twist.dot(iI_w_sum * v_twist) );
    
    lambda_cone_acc = 0;
    lambda_twist_acc = 0;
}

void ConeTwistConstraint::solve(float dt,
                                float dt_inverse,
                                RigidBody3dComponent& rbA,
                                RigidBody3dComponent& rbB)
{
    // Angular constraint impulses
    v3f P_cone = {0,0,0};
    v3f P_twist = {0,0,0};
    
    // Cone
    //
    if (cone_ctr_active)
    {
        // Velocity constraint, Cdot = 0
        float Cdot_cone = (rbB.W - rbA.W).dot(v_cone);
        float lambda_cone = -(Cdot_cone + bias_cone)*iK_cone;
        
        // Impulse magnitude
        float lambda_cone_tot;
        
        // Constraint is unilateral to clamp
        lambda_cone_tot = clamp(lambda_cone_acc + lambda_cone, 0.0f, (float)fINF) - lambda_cone_acc;
        lambda_cone_acc += lambda_cone_tot;
        
        // Cone impulse
        P_cone = -v_cone * lambda_cone_tot;
    }
    
    // Twist
    //
    if (twist_ctr_active)
    {
        // velocity constraint
        float Cdot_twist = (rbB.W - rbA.W).dot(v_twist);
        float lambda_twist = -(Cdot_twist + bias_twist)*iK_twist;
        
        // Impulse magnitude
        float lambda_twist_tot;
        
        // Constraint is unilateral to clamp
        lambda_twist_tot = clamp(lambda_twist_acc + lambda_twist, 0.0f, (float)fINF) - lambda_twist_acc;
        lambda_twist_acc += lambda_twist_tot;
        
        P_twist = -v_twist * lambda_twist_tot;
    }
    
    // Apply impulse
    vec3f P = P_cone + P_twist;
    RigidBody::apply_angular_impulse(rbA, P);
    RigidBody::apply_angular_impulse(rbB, -P);
    //        bodyA->apply_angular_impulse(P);
    //        bodyB->apply_angular_impulse(-P);
}
