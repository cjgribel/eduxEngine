
/*
 * Tau3D Dynamics 
 * Carl Johan Gribel (c) 2011, cjgribel@gmail.com
 *
 */

#include "force.h"
#include "world.h"

force_t::force_t(	body_t *bodyA, body_t *bodyB,
					vec3f rA, vec3f rB,
					f32 K, f32 D, f32 L)
					:	bodyA(bodyA), bodyB(bodyB),
						rA(rA), rB(rB),
						K(K), D(D), L(L)
{}

void force_t::applyForce()
{
	/* anchors in world space */
	vec3f rAw = bodyA->R * rA;
	vec3f rBw = bodyB->R * rB;
	vec3f rABw = bodyA->X + rAw - bodyB->X - rBw;
	f32 rABw_nrm = rABw.norm2();

	/* spring (distance) */
	vec3f rABw_nrmd, f_spring;
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
	vec3f rAw_dot = bodyA->V + bodyA->W % rAw;
	vec3f rBw_dot = bodyB->V + bodyB->W % rBw;
	vec3f f_damper = rABw_nrmd * rABw_nrmd.dot(rAw_dot - rBw_dot) * D;

	// apply resulting force to bodies
	vec3f f_sum = f_spring + f_damper;
	bodyA->apply_force(-f_sum, rAw);
	bodyB->apply_force(f_sum, rBw);
}

void force_t::render()
{
	vec3f	vAw = bodyA->X + bodyA->R * rA,
			vBw = bodyB->X + bodyB->R * rB;
	
	glColor3f(0.3f, 0.3f, 0.3f);
	glLineWidth(2);
	glBegin(GL_LINES);
	glVertex3fv(vAw.vec);
	glVertex3fv(vBw.vec);
	glEnd();

    render_helix(vAw, vBw);
}