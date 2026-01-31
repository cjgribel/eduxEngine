
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

// inclusion guard
#pragma once
#ifndef T2DFORCE_H
#define T2DFORCE_H

#include "tau2d.h"
#include "t2Body.h"

/*
	force base class
*/
class t2Force
{
public:
	t2Body	*bodyA, *bodyB;
	vec2f	anchorA, anchorB;

	t2Force(
		t2Body* bodyA,
		t2Body* bodyB,
		vec2f anchorA,
		vec2f anchorB)
		: bodyA(bodyA), bodyB(bodyB), anchorA(anchorA), anchorB(anchorB)
	{ }

	virtual ~t2Force(void) { }

	virtual void applyForce() = 0;

	virtual void render() = 0;
};

/*
	linear & angular springdamper force
*/
class t2dSpringDamperForce : public t2Force
{
public:

	/*
		initiate with linear coeff's only
	*/
	t2dSpringDamperForce(
		t2Body* bodyA,
		t2Body* bodyB,
		vec2f anchorA,
		vec2f anchorB,
		float K,
		float D,
		float L)
		: t2Force(bodyA, bodyB, anchorA, anchorB), K_lin(K), D_lin(D), L_lin(L), K_ang(0.0f), D_ang(0.0f), R_ang(0.0f)
	{ }

	/*
		initiate with linear & angular coeff's
	*/
	t2dSpringDamperForce(
		t2Body* bodyA,
		t2Body* bodyB,
		vec2f anchorA,
		vec2f anchorB,
		float K_lin,
		float D_lin,
		float L_lin,
		float K_ang,
		float D_ang,
		float R_ang)
		: t2Force(bodyA, bodyB, anchorA, anchorB), K_lin(K_lin), D_lin(D_lin), L_lin(L_lin), K_ang(K_ang), D_ang(D_ang), R_ang(R_ang)
	{ }

	virtual void applyForce()
	{
		if( K_lin > 0.0f || D_lin > 0.0f )
		{
			// linear spring force
			rA = mat2(bodyA->R) * anchorA;
			rB = mat2(bodyB->R) * anchorB;
			rAB = bodyA->X + rA - bodyB->X - rB;
			rAB_len = rAB.norm();
			if( rAB_len > 0.0f )
			{
				rABn = rAB / rAB_len;
				springForce = (rAB - rABn * L_lin) * K_lin;
			}
			else
			{
				rABn.set(0.0f, 0.0f);
				springForce.set(0.0f, 0.0f);
			}
		
			// linear damper force
			rAdot = bodyA->V + vec2f::cross(bodyA->W, rA);
			rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
			damperForce = rABn * vec2f::dot(rAdot - rBdot, rABn) * D_lin;

			// apply resulting force to bodies
			sumForce = springForce + damperForce;
			bodyA->applyForce(-sumForce, rA);
			bodyB->applyForce(sumForce, rB);
		}

		if( K_ang > 0.0f || D_ang > 0.0f )
		{
			// angular spring torque
			springTorque = (bodyA->R - bodyB->R + R_ang) * K_ang;

			// angular damping torque
			damperTorque = (bodyA->W - bodyB->W) * D_ang;

			// apply resulting torque to bodies
			sumTorque = springTorque + damperTorque;
			bodyA->T -= sumTorque;
			bodyB->T += sumTorque;
		}
	}

	virtual void render()
	{
		//vec2f
		//	vA = bodyA->X + mat2(bodyA->R) * anchorA,
		//	vB = bodyB->X + mat2(bodyB->R) * anchorB;
		//glBegin(GL_LINES);
		//glVertex2d(vA.x, vA.y);
		//glVertex2d(vB.x, vB.y);
		//glEnd();
	}

	float	K_lin,			// linear spring constant
			D_lin,			// damping constant (linear friction)
			L_lin,			// spring length (m)
			K_ang,			// torsion constant
			D_ang,			// rotational friction
			R_ang;			// angular spring length (rad)

//protected:
public:
	float	rAB_len,
			springTorque, damperTorque, sumTorque;
	vec2f	rA, rB, rAB, rABn, rAdot, rBdot,
			springForce, damperForce, sumForce;
};

/*
	springdamper rendered as deflected spring
*/
class t2CoilSpring : public t2dSpringDamperForce
{
public:
	t2CoilSpring(
		t2Body* bodyA,
		t2Body* bodyB,
		vec2f anchorA,
		vec2f anchorB,
		float K,
		float D,
		float L,
		int coilPeriods,
		float coilAmplitude)
		: t2dSpringDamperForce(bodyA, bodyB, anchorA, anchorB, K, D, L),
		periods(coilPeriods), amplitude(coilAmplitude)
	{ }

	void render()
	{
		vec2f	vA = bodyA->X + mat2(bodyA->R) * anchorA,		// global anchor point of A
				vB = bodyB->X + mat2(bodyB->R) * anchorB,		// global anchor point of B
				vAB = vB - vA;									// joint vector
		float	vAB_len = vAB.norm(),							// 
				period_len = vAB_len / periods;					// absolute length of each coil 'period'
		vec2f	vABn = vAB / vAB_len,							// 
				v_inc = vABn * period_len,						// increment along the joint
				v_subinc = v_inc * (1.0f / 4.0f),				// 1/4 of a period increment
				vn = vec2f::getNormal(vA, vB).normalize() * amplitude,	// joint normal vector (amplitude direction)																
				v = vA,											// initial point along joint vector
				v1, v2;											// temps for 1/4 and 3/4 of each period

		if( vAB_len < 0.001f )
			return;

		glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
		glBegin(GL_LINE_STRIP);
		glVertex2d(vA.x, vA.y);
		for( int i = 0; i < periods; i++ )
		{
			v1 = v + v_subinc + vn;
			v2 = v + v_subinc * 3.0f - vn;
			glVertex2d(v1.x, v1.y);
			glVertex2d(v2.x, v2.y);
			v += v_inc;
		}
		glVertex2d(vB.x, vB.y);
		glEnd();
	}

//private:
	int periods;
	float amplitude;
};

/*
	model for the force acting on the camera in order for it to track its target
	note:	velocities will be damped regardless of direction
	note:	bodyA is supposed to always point to the body repereseting the camera
			anchorA is supposed to be (0,0). forces will act only on bodyA.
			bodyB/anchorB may be any target body/anchor point.
*/
class t2dCameraTrackingForce : public t2dSpringDamperForce
{
public:

	t2dCameraTrackingForce(
		t2Body* bodyA,
		t2Body* bodyB,
		vec2f anchorA,
		vec2f anchorB,
		float K,
		float D,
		float L)
		: t2dSpringDamperForce(bodyA, bodyB, anchorA, anchorB, K, D, L)
	{ }

	void applyForce()
	{
		// spring force
		//rA = mat2(bodyA->R) * anchorA; should be (0,0)
		rB = mat2(bodyB->R) * anchorB;
		rAB = bodyA->X - bodyB->X - rB;
		rAB_len = rAB.norm();
		if( rAB_len > 0.0f )
		{
			rABn = rAB / rAB_len;
			springForce = (rAB - rABn * L_lin) * K_lin;
		}
		else
		{
			rABn.set(0.0f, 0.0f);
			springForce.set(0.0f, 0.0f);
		}
	
		// damping force
		rAdot = bodyA->V; // + vec2f::cross(bodyA->W, rA); ignore camera angular velocity (should be 0)
		rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
		damperForce = (rAdot - rBdot) * D_lin;

		// apply resulting force to body representing camera
		sumForce = springForce + damperForce;
		bodyA->applyForce(-sumForce, rA);
	}

	void render() { }
};

/*
	model for the force acting on the body attached to the mouse pointer
*/
class t2dMouseTrackingForce : public t2CoilSpring
{
private:
	bool engaged;
    
public:
    t2Body *last_body;

	t2dMouseTrackingForce(
		t2Body* background,
		float K,
		float D)
		: t2CoilSpring(background, NULL, vec2f(), vec2f(), K, D, 0.0f, 7, 0.2f),
        engaged(false),
        last_body(0)
	{ }

	void engage(t2Body* body, vec2f anchor)
	{
		bodyB = body;
        last_body = body;
		anchorB = anchor;
		engaged = true;

		/* adapt coefficients to engaged body */
		//K_lin = body->mass*30.0f;
		//D_lin = K_lin*0.17f;
	}

	void disengage()
	{
		bodyB = NULL;
		engaged = false;
	}

	bool isEngaged() { return engaged; }

	void setPointer(vec2f pointer_pos)
	{
		anchorA = pointer_pos;
	}

	void applyForce()
	{
		if(!engaged)
			return;

		// spring force
		rB = mat2(bodyB->R) * anchorB;
		rAB = anchorA - bodyB->X - rB;
		rAB_len = rAB.norm();
		if( rAB_len > 0.0f )
		{
			rABn = rAB / rAB_len;
			springForce = (rAB - rABn * L_lin) * K_lin;
		}
		else
		{
			rABn.set(0.0f, 0.0f);
			springForce.set(0.0f, 0.0f);
		}

		// damping force
		rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
		damperForce = -rBdot * D_lin;

		// apply resulting force to engaged body
		sumForce = springForce + damperForce;
		bodyB->applyForce(sumForce, rB);
	}

	void render()
	{
		if(engaged) t2CoilSpring::render();
	}
};

#endif /* T2DFORCE_H */