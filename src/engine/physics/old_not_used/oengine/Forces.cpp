
/*
 * Tau3D Dynamics 
 * Carl Johan Gribel (c) 2011, cjgribel@gmail.com
 * Updated July 2021
 *
 */

#include "Forces.hpp"

SpringDamperForce::SpringDamperForce()
{ }

SpringDamperForce::SpringDamperForce(float K,
                                     float D,
                                     float L)
:
K(K), D(D), L(L)
{ }

SpringDamperForce::SpringDamperForce(//body_t *bodyA,
                                     //body_t *bodyB,
                                     v3f rA,
                                     v3f rB,
                                     float K,
                                     float D,
                                     float L)
:
//bodyA(bodyA), bodyB(bodyB),
rA(rA), rB(rB),
K(K), D(D), L(L)
{ }

void SpringDamperForce::apply(RigidBody3dComponent& rbA,
                              RigidBody3dComponent& rbB)
{
	/* anchors in world space */
	v3f rAw = rbA.R * rA;
	v3f rBw = rbB.R * rB;
	v3f rABw = rbA.X + rAw - rbB.X - rBw;
	float rABw_nrm = rABw.norm2();

	/* spring (distance) */
	v3f rABw_nrmd, f_spring;
	if( rABw_nrm > 0 )
	{
		rABw_nrmd = rABw / rABw_nrm;
		f_spring = (rABw - rABw_nrmd * L) * K;
	}
	else
	{
		rABw_nrmd.set(0.0f, 0.0f, 0.0f);
		f_spring.set(0.0f, 0.0f, 0.0f);
	}

	/* damping (velocity) */
	v3f rAw_dot = rbA.V + rbA.W % rAw;
	v3f rBw_dot = rbB.V + rbB.W % rBw;
	v3f f_damper = rABw_nrmd * rABw_nrmd.dot(rAw_dot - rBw_dot) * D;

	// apply resulting force to bodies
	v3f f_sum = f_spring + f_damper;
	
    RigidBody::apply_force(rbA, -f_sum, rAw);
    RigidBody::apply_force(rbB, f_sum, rBw);
//    bodyA->apply_force(-f_sum, rAw);
//	bodyB->apply_force(f_sum, rBw);
}

void LinearSpringDamper3dSystem::update(float dt, entt::registry& registry)
{
    auto view = registry.view<LinearSpringDamper3dComponent>();
    
    for(auto entity: view)
    {
        auto& spring = view.get<LinearSpringDamper3dComponent>(entity);
        if (!spring.is_active) continue;
        
        auto& rbA = registry.get<RigidBody3dComponent>(spring.rb3d_entityA);
        auto& rbB = registry.get<RigidBody3dComponent>(spring.rb3d_entityB);
    
        // Linear spring force
        const v3f rAw = rbA.R * spring.rA;
        const v3f rBw = rbB.R * spring.rB;
        const v3f rABw = rbA.X + rAw - rbB.X - rBw;
        const float rABw_len = rABw.norm2();
        
        v3f rABw_nrmd = v3f_000, F_spring = v3f_000;
        if( rABw_len > 0.0f )
        {
            rABw_nrmd = rABw / rABw_len;
            F_spring = (rABw - rABw_nrmd * spring.L) * spring.K;
        }
        
        // Linear damper force
        const v3f rAw_dot = rbA.V + cross(rbA.W, rAw);
        const v3f rBw_dot = rbB.V + cross(rbB.W, rBw);
        const v3f F_damper = rABw_nrmd * dot(rAw_dot - rBw_dot, rABw_nrmd) * spring.D;
        
        // Apply resulting force to bodies
        v3f F_sum = F_spring + F_damper;
        RigidBody::apply_force(rbA, -F_sum, rAw);
        RigidBody::apply_force(rbB, F_sum, rBw);
    }
}
    
void LinearSpringDamper2dSystem::update(float dt, entt::registry& registry)
{
    auto view = registry.view<LinearSpringDamper2dComponent>();
    
    for(auto entity: view)
    {
        auto& spring = view.get<LinearSpringDamper2dComponent>(entity);
        if (!spring.is_active) continue;
        
        auto& rbA = registry.get<RigidBody2dComponent>(spring.rb2d_entityA);
        auto& rbB = registry.get<RigidBody2dComponent>(spring.rb2d_entityB);
    
        // Linear spring force
        const v2f rA = m2f::rotation(rbA.R) * spring.rA;
        const v2f rB = m2f::rotation(rbB.R) * spring.rB;
        const v2f rAB = rbA.X + rA - rbB.X - rB;
        const float rAB_len = rAB.norm2();
        
        v2f rABn = v2f_00, F_spring = v2f_00;
        if( rAB_len > 0.0f )
        {
            rABn = rAB / rAB_len;
            F_spring = (rAB - rABn * spring.L) * spring.K;
        }
        
        // Linear damper force
        const v2f rAdot = rbA.V + cross((float)rbA.W, rA);
        const v2f rBdot = rbB.V + cross((float)rbB.W, rB);
        const v2f F_damper = rABn * dot(rAdot - rBdot, rABn) * spring.D;
        
        // apply resulting force to bodies
        v2f F_sum = F_spring + F_damper;
        RigidBody::apply_force(rbA, -F_sum, rA);
        RigidBody::apply_force(rbB, F_sum, rB);
    }
}

void AngularSpringDamper3dSystem::update(float dt, entt::registry& registry)
{
    auto view = registry.view<AngularSpringDamper3dComponent>();
    
    for(auto entity: view)
    {
        auto& spring = view.get<AngularSpringDamper3dComponent>(entity);
        if (!spring.is_active) continue;
        
        auto& rbA = registry.get<RigidBody3dComponent>(spring.rb3d_entityA);
        auto& rbB = registry.get<RigidBody3dComponent>(spring.rb3d_entityB);
       
        // Spring torque
        //
        // Difference between current relative rotation and initial
        // relative rotation:
        // D = (R_B * R_A^T) * (R_init)^T
        // TODO: Why not D = R_init * (R_B * R_A^T)^T, i.e. from "current" to "init"?
        const m3f D = rbB.R * transpose(rbA.R) * transpose(spring.R);
        //
        const float Dx = extract_Euler_angle_x(D);
        const float Dy = extract_Euler_angle_y(D);
        const float Dz = extract_Euler_angle_z(D);
        const v3f T_spring = v3f(Dx, Dy, Dz) * spring.K;

        // Damping torque
        const v3f T_damper = (rbB.W - rbA.W) * spring.D;

        // Apply resulting torque to bodies
        const v3f T_sum = T_spring + T_damper;
        RigidBody::apply_torque(rbA, T_sum);
        RigidBody::apply_torque(rbB, -T_sum);
    }
}

void AngularSpringDamper2dSystem::update(float dt, entt::registry& registry)
{
    auto view = registry.view<AngularSpringDamper2dComponent>();
    
    for(auto entity: view)
    {
        auto& spring = view.get<AngularSpringDamper2dComponent>(entity);
        if (!spring.is_active) continue;
        
        auto& rbA = registry.get<RigidBody2dComponent>(spring.rb2d_entityA);
        auto& rbB = registry.get<RigidBody2dComponent>(spring.rb2d_entityB);
       
        // Angular spring torque
        const float T_spring = (rbA.R - rbB.R + spring.R) * spring.K;
        
        // Angular damping torque
        const float T_damper = (rbA.W - rbB.W) * spring.D;
        
        // Apply resulting torque to bodies
        const float T_sum = T_spring + T_damper;
        RigidBody::apply_torque(rbA, -T_sum);
        RigidBody::apply_torque(rbB, T_sum);
    }
}
