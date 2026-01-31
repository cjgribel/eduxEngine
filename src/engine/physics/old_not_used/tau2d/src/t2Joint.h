
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

#pragma once
#ifndef T2JOINT_H
#define T2JOINT_H

#include "tau2d.h"
#include "t2Body.h"

enum JointType {    JOINT_DEFAULT,
                    JOINT_DISTANCE,
                    JOINT_REVOLUTE_A,
                    JOINT_ELEVATEDHINGE,
                    JOINT_ELEVATEDHINGEDUMMY,
                    JOINT_REVOLUTE_B,
                    JOINT_PRISMATIC_A,
                    JOINT_PRISMATIC_B,  // needed ?
                    JOINT_LINEARACTUATOR,
                    JOINT_ANGLE,
                    JOINT_ANGULARACTUATOR,
                    JOINT_ANGULARLIMITS,
                    JOINT_CONTACT
};

/*
	abstract base class for all joints
*/
class t2Joint
{

public:

	t2Body	*bodyA, *bodyB;
	vec2f	anchorA, anchorB;
    JointType type;

    t2Joint() {}
    
	t2Joint(	t2Body* bodyA,
				t2Body* bodyB,
                JointType type)
		:	bodyA(bodyA),
			bodyB(bodyB),
            type(type)
            
	{ }

	t2Joint(	t2Body* bodyA,
				t2Body* bodyB,
				vec2f anchorA,
				vec2f anchorB,
                JointType type)
		:	bodyA(bodyA),
			bodyB(bodyB),
			anchorA(anchorA),
			anchorB(anchorB),
            type(type)
	{ }

	virtual void init_solve(float dt, float idt) = 0;

	virtual void solve(float dt, float idt) = 0;
    
    virtual float joint_force() { return 0; }

	virtual void render() { }
	
	virtual ~t2Joint(void) { }
};


/*

*/
class t2DistanceJoint : public t2Joint
{
public:
	float L;

    t2DistanceJoint() : t2Joint(NULL, NULL, vec2f(), vec2f(), JOINT_DISTANCE), ERP(0.15f), L() {}
    
	t2DistanceJoint(	t2Body* bodyA,
						t2Body* bodyB,
						vec2f anchorA,
						vec2f anchorB,
						float L)
    :	t2Joint(bodyA, bodyB, anchorA, anchorB, JOINT_DISTANCE),
			L(L),
			ERP(0.15f)
	{ }

	void init_solve(float dt, float idt)
	{
		/* transformed anchor points */
		rA = mat2(bodyA->R) * anchorA;
		rB = mat2(bodyB->R) * anchorB;

		/* constraint direction */
		u = (bodyB->X + rB) - (bodyA->X + rA);
		u_len = u.norm();
		if(u_len > 0.0f)
			un = u / u_len;
		else
			un.set(0.0f, 0.0f);

		/* velocity bias term */
		C = u_len - L;
		bias = ERP * idt * C;

		/* effective mass */
		crAu = vec2f::cross(rA, un);
		crBu = vec2f::cross(rB, un);
		K = bodyA->imass + bodyA->iI * crAu * crAu + bodyB->imass + bodyB->iI * crBu * crBu;
		iK = 1.0f / K;

		lambdaAcc = 0.0f;
	}

	void solve(float dt, float idt)
	{
		/* anchor points velocities: rdot = v + w x r */
		rAdot = bodyA->V + vec2f::cross(bodyA->W, rA);
		rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
		/* anchor points relative velocity */
		rABdot = rBdot - rAdot;

		/* velocity constraint (is ideally = 0) */
		Cdot = vec2f::dot(un, rABdot);

		/* calculate and apply impulse */
		lambda = -iK * (Cdot + bias);
		P = un * lambda;
		bodyA->applyImpulse(-P, rA);
		bodyB->applyImpulse(P, rB);
	}

	void render()
	{
		vec2f	vA = bodyA->X + mat2(bodyA->R) * anchorA,
				vB = bodyB->X + mat2(bodyB->R) * anchorB;

		glBegin(GL_LINES);
		glVertex2d(vA.x, vA.y);
		glVertex2d(vB.x, vB.y);
		glEnd();
	}

	~t2DistanceJoint(void)	{ }

//private:

	vec2f	rA, rB,					/* transformed anchor points */
			rAdot, rBdot, rABdot,	/* anchor points velocities / relative velocity */
			u, un,					/* anchor points direction */
			P;						/* constraint impulse */
	float	C, Cdot,				/* position and velocity constraint */
			bias, ERP,				/* bias term (for position correction) / error reduction parameter (ERP) */
			lambda, lambdaAcc,		/* constraint impulse magnitude */
			K, iK;					/* constraint effective mass */
	float	u_len, crAu, crBu;		/* */
};


/*
	note: angular limits and actuation are not supposed to be enabled at the same time (duh)
*/
class t2RevoluteJoint : public t2Joint
{
public:

	t2RevoluteJoint(	t2Body* bodyA,
						t2Body* bodyB,
						vec2f anchorA,
						vec2f anchorB)
		:	t2Joint(bodyA, bodyB, anchorA, anchorB, JOINT_REVOLUTE_A),
			ERP_lin(0.2f), ERP_ang(0.2f),
			ang_tol(1.0f*DEG_TO_RAD),
			lambdaAcc_ang(0.0f),
			limitsEnabled(false), limitsActive(false), upperLimitActive(false),
			actuatorEnabled(false)		
	{ }

	void enableAngularLimits(float rad_min, float rad_max, float rad_init)
	{
		phi_min = rad_min;
		phi_max = rad_max;
		R_init = rad_init;
		limitsEnabled = true;
	}

	void enableAngularActuator(float maxAngularVelocity, float maxTorque)
	{
		W_act_target = maxAngularVelocity;
		T_act_max = maxTorque;
		actuatorEnabled = true;
	}

	void disableActuator() { actuatorEnabled = false; }

	void init_solve(float dt, float idt)
	{
		/* linear (point-to-point) constraint */

        lambdaAcc_lin.set(0, 0);
        
		/* transformed anchor points */
		rA = mat2(bodyA->R) * anchorA;
		rB = mat2(bodyB->R) * anchorB;

		/* constraint effective mass
			K	=	| 1/mA  0   | - 1/IA *skew(rA)*skew(rA) + | 1/mB  0   | - 1/IB *skew(rB)*skew(rB)
					|  0   1/mA |                             |  0   1/mB |

				=	| 1/mA  0   | - 1/IA * | -rA.y*rA.y  rA.x*rA.y | + | 1/mB  0   | - 1/IB * | -rB.y*rB.y  rB.x*rB.y |
					|  0   1/mA |          |  rA.x*rA.y -rA.x*rA.x |   |  0   1/mB |          |  rB.x*rB.y -rB.x*rB.x |
		*/
		K_lin = mat2(
			bodyA->imass + bodyA->iI * rA.y * rA.y + bodyB->imass + bodyB->iI * rB.y * rB.y,
			-bodyA->iI * rA.x * rA.y - bodyB->iI * rB.x * rB.y,
			-bodyA->iI * rA.x * rA.y - bodyB->iI * rB.x * rB.y,
			bodyA->imass + bodyA->iI * rA.x * rA.x + bodyB->imass + bodyB->iI * rB.x * rB.x);
		iK_lin = K_lin.invert();

		/* velocity bias term */
		C_lin = (bodyB->X + rB) - (bodyA->X + rA);
		bias_lin = C_lin * ERP_lin * idt;

		/* actuator constraint */

		if(actuatorEnabled)
		{
			/* constraint effective mass */
			K_act = bodyA->iI + bodyB->iI;
			mc_act = 1.0f / K_act;
			lambdaAcc_act = 0.0f;
		}

		/* angular constraint */

		if(limitsEnabled)
		{ 
			/* decide angle between bodies */
			phi = bodyB->R - bodyA->R - R_init;

			if(phi < phi_min || phi > phi_max)
			{
				/* decide what limit is being breached and by how much */
				if(phi < phi_min)
				{
					C_ang = -clampf(-(phi - phi_min + ang_tol), 0.0f, INF);//printf("%f\n", ang_tol);
					upperLimitActive = true;
				}
				else
				{
					C_ang = clampf(phi - phi_max - ang_tol, 0.0f, INF);//printf("%f\n", ang_tol);
					upperLimitActive = false;
				}
				/* velocity bias term */
				bias_ang = C_ang * ERP_ang * idt;
				/* constraint effective mass */
				iK_ang = 1.0f / (bodyA->iI + bodyB->iI);

				lambdaAcc_ang = 0.0f;
				limitsActive = true;
			}
			else
				limitsActive = false;
		}
	}

	void solve(float dt, float idt)
	{
		/* linear (point-to-point) constraint */

		/* velocity of anchor points: rdot = v + w x r */
		rAdot = bodyA->V + vec2f::cross(bodyA->W, rA);
		rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
		/* anchor points relative velocity */
		rABdot = rBdot - rAdot;

		/* velocity constraint (is ideally = 0) */
		Cdot_lin = rABdot;
		/* calculate and apply impulse */
		lambda_lin = -iK_lin * (Cdot_lin + bias_lin);
		P_lin = lambda_lin;
        lambdaAcc_lin += lambda_lin;
        
		bodyA->applyImpulse(-P_lin, rA);
		bodyB->applyImpulse(P_lin, rB);

		/* actuator constraint */

		if(actuatorEnabled)
		{
			/* actuator velocity constraint */
			Cdot_act = bodyB->W - bodyA->W + W_act_target;
			/* calculate & clamp lambda, add to accumulated	*/
			lambda_act = -mc_act * idt * Cdot_act;
			lambda_act = clampf(lambdaAcc_act + lambda_act, -T_act_max, T_act_max) - lambdaAcc_act;
			lambdaAcc_act += lambda_act;
			/* calculate and apply actuator impulse */
			P_act = dt * lambda_act;
			bodyA->W -= bodyA->iI * P_act;
			bodyB->W += bodyB->iI * P_act;
		}

		/* angular constraint */

		if(limitsActive)
		{
			/* angular velocity constraint (is ideally = 0) */
			Cdot_ang = bodyB->W - bodyA->W;

			/* calculate & clamp lambda, add to accumulated */
			lambda_ang = -iK_ang * (Cdot_ang + bias_ang);
			if(upperLimitActive)
				lambda_ang = clampf(lambdaAcc_ang + lambda_ang, 0.0f, INF) - lambdaAcc_ang;
			else
				lambda_ang = clampf(lambdaAcc_ang + lambda_ang, -INF, 0.0f) - lambdaAcc_ang;
			lambdaAcc_ang += lambda_ang;

			/* calculate and apply angular impulse */
			P_ang = lambda_ang;
			bodyA->W -= bodyA->iI * P_ang;
			bodyB->W += bodyB->iI * P_ang;
		}
	}
    
    virtual float joint_force() { return lambdaAcc_lin.norm(); }

	~t2RevoluteJoint(void) { }

protected:

	/* linear entities */
	vec2f	rA, rB,					/* transformed anchor points */
			rAdot, rBdot, rABdot,	/* anchor points velocities / relative velocity */
			P_lin, lambda_lin, lambdaAcc_lin,   /* impulse / magnitude of impulse */
			bias_lin,				/* bias term (for position correction) */
			C_lin, Cdot_lin;		/* position and velocity constraints */
	mat2	iK_lin,					/* constraint effective mass */
			K_lin;					/* */ 
	float	ERP_lin;				/* error reduction parameter (ERP) */

	/* angular limits entities */
	float	C_ang, Cdot_ang,
			ERP_ang, bias_ang, ang_tol,
			iK_ang,
			P_ang, lambda_ang, lambdaAcc_ang,
			phi, phi_min, phi_max, R_init;
	bool	limitsEnabled, limitsActive, upperLimitActive;

	/* actuator entities */
	float	K_act, mc_act, Cdot_act,
			lambda_act, lambdaAcc_act,
			P_act, W_act_target, T_act_max;
	bool	actuatorEnabled;
};

/*
	prismatic joint
*/
class t2PrismaticJoint : public t2Joint
{

public:

	t2PrismaticJoint(	t2Body* bodyA,
						t2Body* bodyB,
						vec2f anchorA,
						vec2f anchorB,
						float R_rel,
						float R_joint)
		:	t2Joint(bodyA, bodyB, anchorA, anchorB, JOINT_PRISMATIC_A),
			R_rel(R_rel), R_joint(R_joint),
			l_init(1.0f, 0.0f),
			ERP_lin(0.2f), maxCorr_lin(0.2f),
			ERP_ang(0.2f), maxCorr_ang(10.0f)
	{ }

	void init_solve(float dt, float idt)
	{
		/* linear constraint */

		/* transformed anchor points */
		rA = mat2(bodyA->R) * anchorA;
		rB = mat2(bodyB->R) * anchorB;

		d = bodyB->X + rB - bodyA->X - rA;
		l = mat2(bodyA->R + R_joint) * l_init;

		/* linear bias */
		C_lin = vec2f::dot(l, d);
		C_lin = clampf(C_lin, -maxCorr_lin, maxCorr_lin);
		bias_lin = C_lin * ERP_lin * idt;

		/* linear constraint effective mass */
		crAl = vec2f::cross(d+rA, l);
		crBl = vec2f::cross(rB, l);
		K_lin = bodyA->imass + bodyB->imass + bodyA->iI * crAl * crAl + bodyB->iI * crBl * crBl;
		iK_lin = 1.0f / K_lin;

		/* angular constraint */

		// angular bias
		C_ang = bodyB->R - bodyA->R + R_rel;
		C_ang = clampf(C_ang, -maxCorr_ang, maxCorr_ang);
		bias_ang = C_ang * ERP_ang * idt;

		/* angular constraint effective mass */
		iK_ang = 1.0f / (bodyA->iI + bodyB->iI);
	}

	void solve(float dt, float idt)
	{
		/* linear constraint */

		/* linear velocity constraint (is ideally = 0) */
		Cdot_lin =
			-vec2f::dot(l, bodyA->V) -
			bodyA->W * vec2f::cross(d+rA, l) +
			vec2f::dot(l, bodyB->V) +
			bodyB->W * vec2f::cross(rB, l);

		/* calculate and apply linear impulse */
		lambda_lin = -iK_lin * (Cdot_lin + bias_lin);
		P_lin = l * lambda_lin;
		bodyA->applyImpulse(-P_lin, d+rA);
		bodyB->applyImpulse(P_lin, rB);

		/* angular constraint */

		/* angular velocity constraint (is ideally = 0) */
		Cdot_ang = bodyB->W - bodyA->W;

		/* calculate and apply angular impulse */
		P_ang = -iK_ang * (Cdot_ang + bias_ang);
		bodyA->W -= bodyA->iI * P_ang;
		bodyB->W += bodyB->iI * P_ang;
	}

	~t2PrismaticJoint(void) { }

private:

	vec2f	rA, rB, d,				/* transformed anchor points; anchor points direction */
			l, l_init,				/* constraint reference direction; inital constraint direction */
			P_lin;					/* linear impulse */
	float	R_rel, R_joint;			/* relative body angle; joint angle */
	float	C_lin, Cdot_lin,		/* position and velocity constraint */
			bias_lin, ERP_lin,		/* bias term (for position correction); error reduction parameter (ERP) */
			iK_lin,					/* effective mass of linear constraint */
			lambda_lin,				/* magnitude of linear impulse */
			maxCorr_lin,			/* max correction (for stability) */
			crAl, crBl,				/* precalculated cross products */
			K_lin;					/* */

	float	C_ang, Cdot_ang,		/* */
			bias_ang, ERP_ang,		/* */
			P_ang,					/* */
			iK_ang,					/* */
			maxCorr_ang;			/* */
};

/*
	contact joint
	special joint handling contact points (non-penetration, friction, restitution)
	note: does not utilize the base class variables anchorA and anchorB
*/
class t2ContactJoint : public t2Joint
{
public:

	t2ContactJoint(	t2Body* bodyA,
					t2Body* bodyB,
					vec2f cp,
					vec2f cn,
					float depth,
                    int id_geomA, int id_geomB, int id_vertex, int id_edge)
		:	t2Joint(bodyA, bodyB, JOINT_CONTACT),
			cp(cp),
			cn(cn),
			depth(depth),
            id_geomA(id_geomA), id_geomB(id_geomB), id_vertex(id_vertex), id_edge(id_edge),
			ERP_n(T2_CONTACT_ERPn),
			depth_tol(T2_CONTACT_DEPTH_TOL), vel_tol(T2_CONTACT_VEL_TOL)
	{ }

	void init_solve(float dt, float idt) {}
    
    //f32 warm_lambdaAcc_n;
    
    void pre_solve(float dt, float idt, t2ContactJoint *c_warm)
	{
		/* anchor points */
		rA = cp - bodyA->X;
		rB = cp - bodyB->X;
        
        
		/* anchor points velocities: rdot = v + w x r */
		rAdot = bodyA->V + vec2f::cross(bodyA->W, rA);
		rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
		/* anchor points relative velocity */
		rABdot = rBdot - rAdot;
        
		/* normal constraint (non-penetration) */

		/* normal effective mass */
		crAcn = vec2f::cross(rA, cn);
		crBcn = vec2f::cross(rB, cn);
		K_n = bodyA->imass + bodyA->iI * crAcn * crAcn + bodyB->imass + bodyB->iI * crBcn * crBcn;
		iK_n = 1.0f / K_n;

		/* normal bias (position correction and restitution) */
        C_n = clampf(depth - depth_tol, 0.0f, INF);

		rABdot_n = vec2f::dot(rABdot, cn);
		rABdot_n_cl = clampf(rABdot_n - vel_tol, 0.0f, INF);
		bias_n = ERP_n * idt * C_n + bodyA->restitution * bodyB->restitution * rABdot_n_cl;

		/* tangential constraint (friction) */

		/* tangential effective mass */
		ct = vec2f::normalize(rABdot - cn * rABdot_n);
		crAct = vec2f::cross(rA, ct);
		crBct = vec2f::cross(rB, ct);
		K_t = bodyA->imass + bodyA->iI * crAct * crAct + bodyB->imass + bodyB->iI * crBct * crBct;
		iK_t = 1.0f / K_t;

		/* tangential bias */
		bias_t = 0;
		

        lambdaAcc_n = 0.0f;
        lambdaAcc_t = 0.0f;
        
        if (c_warm != NULL)
        {   
            /* 
             warm starting: apply accumulated impulse from last iteration ('solution')
             */
            
            f32 ln = c_warm->lambdaAcc_n;
            f32 lt = c_warm->lambdaAcc_t;   
            lambdaAcc_n = ln;
            lambdaAcc_t = lt;
            
            P_n = cn * ln;
            bodyA->applyImpulse(P_n, c_warm->rA);
            bodyB->applyImpulse(-P_n, c_warm->rB);
            P_t = ct * lt;
            bodyA->applyImpulse(P_t, c_warm->rA);
            bodyB->applyImpulse(-P_t, c_warm->rB);
        }

	}

	void solve(float dt, float idt)
	{
		/* anchor points velocities: rdot = v + w x r */
		rAdot = bodyA->V + vec2f::cross(bodyA->W, rA);
		rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
		/* anchor points relative velocity */
		rABdot = rBdot - rAdot;
		/* anchor points relative normal and tangential velocities */
		rABdot_n = vec2f::dot(rABdot, cn);
		rABdot_t = vec2f::dot(rABdot, ct);

		/* normal constraint (restitution) */

		/* normal velocity constraint */
		Cdot_n = rABdot_n;
		lambda_n = iK_n * (Cdot_n + bias_n);
		// clamp lambda, add to accumulated */
		lambda_n = clampf(lambdaAcc_n + lambda_n, 0, INF) - lambdaAcc_n;
		lambdaAcc_n += lambda_n;
		/* calculate and apply normal impulse */
		P_n = cn * lambda_n;
		bodyA->applyImpulse(P_n, rA);
		bodyB->applyImpulse(-P_n, rB);

		/* tangential constraint (friction) */

		/* tangential velocity constraint */
		Cdot_t = rABdot_t;
		/* calculate lambda, identify type of friction, add to accumulated */
		lambda_t = iK_t * (Cdot_t + bias_t);
		maxFriction_d = (bodyA->kinetic_friction + bodyB->kinetic_friction)*0.5f * lambdaAcc_n;
		maxFriction_s = (bodyA->static_friction + bodyB->static_friction)*0.5f * lambdaAcc_n;
		/*	if tangential impule larger than threshold for static friction -- clamp to bounds for dynamic friction,
			otherwise, keep lambda to eliminate all relative tangential velocity */
		if(fabs(lambda_t) > fabs(maxFriction_s))
			lambda_t = clampf(lambdaAcc_t + lambda_t, -maxFriction_d, maxFriction_d) - lambdaAcc_t;
		lambdaAcc_t += lambda_t;
		/* calculate and apply tangential impulse */
		P_t = ct * lambda_t;
		bodyA->applyImpulse(P_t, rA);
		bodyB->applyImpulse(-P_t, rB);
	}
    
    void post_solve()
    {

    }

	void render()
	{
        glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
        glLineWidth(1);
        
		/* contact point */
		float dx = 0.075f;
		glBegin(GL_QUADS);
		glVertex2d(cp.x + dx, cp.y - dx);
		glVertex2d(cp.x + dx, cp.y + dx);
		glVertex2d(cp.x - dx, cp.y + dx);
		glVertex2d(cp.x - dx, cp.y - dx);
		glEnd();

		/* render penetration */
		vec2f v_depth = cn * depth;
		glBegin(GL_LINES);
		glVertex2d(cp.x, cp.y);
		glVertex2d(cp.x + v_depth.x, cp.y + v_depth.y);
		glEnd();
        
        /* line between bodies */
        glBegin(GL_LINES);
		glVertex2d(bodyA->X.x, bodyA->X.y);
		glVertex2d(bodyB->X.x, bodyB->X.y);
		glEnd();
        
        glLineWidth(2);
	}

	~t2ContactJoint(void) { }

//private:

	vec2f	cp, cn, ct;
	float	C_n, Cdot_n,
			rABdot_n, rABdot_n_cl,
			bias_n, ERP_n,
			K_n, iK_n,
			crAcn, crBcn,
			lambda_n, lambdaAcc_n;

	float	Cdot_t,
			rABdot_t,
			bias_t, ERP_t,
			K_t, iK_t,
			crAct, crBct, 
			lambda_t, lambdaAcc_t;

	float	depth,
			maxFriction_d, maxFriction_s,
			depth_tol, vel_tol;

	vec2f	rA, rB, rAdot, rBdot, rABdot;
	vec2f	P_n, P_t;
    
    int     id_geomA, id_geomB, id_vertex, id_edge;       // id's for contact cache
};

/*

*/
class t2ElevatedHinge : public t2RevoluteJoint
{

public:

	t2ElevatedHinge(	t2Body* bodyA,
						t2Body* bodyB,
						vec2f anchorA,
						vec2f anchorB,
						float rotA_init,
						float rotB_init,
						vec2f lstrutA,
						vec2f rstrutA,
						vec2f lstrutB,
						vec2f rstrutB)
		:	t2RevoluteJoint(bodyA, bodyB, anchorA, anchorB),
			rotA_init(rotA_init), rotB_init(rotB_init),
			lstrutA(lstrutA), rstrutA(rstrutA),
			lstrutB(lstrutB), rstrutB(rstrutB)
	{
        type = JOINT_ELEVATEDHINGE;
    }

	void render()
	{

		mat2
			rotA(bodyA->R + rotA_init),						/* reference rotation of support for A */
			rotB(bodyB->R + rotB_init);						/* reference rotation of support for B */
		vec2f
			vA = bodyA->X + mat2(bodyA->R) * anchorA,		/* global anchor point of A */
			vAl = rotA * lstrutA,							/* left support strut of A */
			vAr = rotA * rstrutA,							/* right support strut of A */
			vB = bodyB->X + mat2(bodyB->R) * anchorB,		/* global anchor point of B */
			vBl = rotB * lstrutB,							/* left support strut of B */
			vBr = rotB * rstrutB;							/* right support strut of B */

		/* render */
		glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
		glBegin(GL_LINE_STRIP);
		glVertex2d(vA.x + vAl.x, vA.y + vAl.y);
		glVertex2d(vA.x, vA.y);
		glVertex2d(vA.x + vAr.x, vA.y + vAr.y);
		glEnd();
		glBegin(GL_LINE_STRIP);
		glVertex2d(vB.x + vBl.x, vB.y + vBl.y);
		glVertex2d(vB.x, vB.y);		
		glVertex2d(vB.x + vBr.x, vB.y + vBr.y);
		glEnd();
	}

private:

	float	rotA_init, rotB_init;					/* reference roations of joint */
	vec2f	lstrutA, rstrutA, lstrutB, rstrutB;		/* left & right strut of body A & B */
};

/*

*/
class t2ElevatedHingeDummy : public t2ElevatedHinge
{

public:

	t2ElevatedHingeDummy(	t2Body* bodyA,
							t2Body* bodyB,
							vec2f anchorA,
							vec2f anchorB,
							float rotA_init,
							float rotB_init,
							vec2f lstrutA,
							vec2f rstrutA,
							vec2f lstrutB,
							vec2f rstrutB)
		:	t2ElevatedHinge(bodyA, bodyB, anchorA, anchorB, rotA_init, rotB_init, lstrutA, rstrutA, lstrutB, rstrutB)
	{
        type = JOINT_ELEVATEDHINGEDUMMY;
    }

	void init_solve(float dt, float idt) { }

	void solve(float dt, float idt) { }
};

/*
	hydraulic actuator
*/
enum t2LinearActuatorJointMode { LINACTMODE_HOLD, LINACTMODE_EXTEND, LINACTMODE_CONTRACT };

class t2LinearActuatorJoint : public t2Joint
{

public:

	t2LinearActuatorJoint(	t2Body* bodyA,
								t2Body* bodyB,
								vec2f anchorA,
								vec2f anchorB,
								float L_min,
								float L_max,
								float L_init,
								float V_max,
								float F_max,
								float stroke_width,
								float stroke_height)
		:	t2Joint(bodyA, bodyB, anchorA, anchorB, JOINT_LINEARACTUATOR),
			L_min(L_min), L_max(L_max), L_target(L_init),
			V_max(V_max),
			F_max(F_max),
			act_mode(LINACTMODE_HOLD),
			stroke_width(stroke_width),
			stroke_height(stroke_height),
			stroke_dwidth(-0.15f),
			ERP(0.15f)
	{ }

	void setMode(t2LinearActuatorJointMode mode)
	{
		if(mode == LINACTMODE_HOLD)
		{
			vec2f u = (bodyA->X + mat2(bodyA->R) * anchorA) - (bodyB->X + mat2(bodyB->R) * anchorB);
			L_target = clampf(u.norm(), L_min, L_max);
		}
		act_mode = mode;
	}

	void init_solve(float dt, float idt)
	{
		/* transformed anchor points */
		rA = mat2(bodyA->R) * anchorA;
		rB = mat2(bodyB->R) * anchorB;

		/* constraint direction */
		u = (bodyB->X + rB) - (bodyA->X + rA);
		u_len = u.norm();
		if(u_len > 0.0f)
			un = u / u_len;
		else
			un.set(0.0f, 0.0f);

		if(act_mode == LINACTMODE_HOLD)
		{
			V_target = 0.0f;
			F_hasLimits = false;
			C = u_len - L_target;
		}
		else if(act_mode == LINACTMODE_EXTEND)
		{
			if(u_len < L_min)
			{
				C = u_len - L_min;
				V_target = V_max;
				F_hasLimits = false;
			}
			else if(u_len <= L_target)
			{
				C = u_len - L_target;
				V_target = V_max;
				F_hasLimits = true; F_low_limit = -F_max; F_high_limit = F_max;
			}
			else if(u_len >= L_max)
			{
				C = u_len - L_max;
				V_target = 0.0f;
				F_hasLimits = false;
			}
			else
			{
				C = 0.0f;
				V_target = V_max;
				F_hasLimits = true;	F_low_limit = NEG_INF; F_high_limit = F_max;
				L_target = clampf(u_len, L_min, L_max);
			}
		}
		else if(act_mode == LINACTMODE_CONTRACT)
		{
			if(u_len > L_max)
			{
				C = u_len - L_max;
				V_target = -V_max;
				F_hasLimits = false;
			}
			else if(u_len >= L_target)
			{
				C = u_len - L_target;
				V_target = -V_max;
				F_hasLimits = true; F_low_limit = -F_max; F_high_limit = F_max;
			}
			else if(u_len <= L_min)
			{
				C = u_len - L_min;
				V_target = 0.0f;
				F_hasLimits = false;
			}
			else
			{
				C = 0.0f;
				V_target = -V_max;
				F_hasLimits = true; F_low_limit = -F_max; F_high_limit = INF;	
				L_target = clampf(u_len, L_min, L_max);
			}
		}


		//float Cc;
		//if(C > 0.0f)
		//	Cc = clampf(C - 0.005f, 0.0f, INF);
		//else
		//	Cc = -clampf(-C - 0.005f, 0.0f, INF);
		bias = ERP * idt * C;

		/* effective mass */
		crAu = vec2f::cross(rA, un);
		crBu = vec2f::cross(rB, un);
		K = bodyA->imass + bodyA->iI * crAu * crAu + bodyB->imass + bodyB->iI * crBu * crBu;
		iK = 1.0f / K;

		lambdaAcc_vel = 0.0f;
	}

	void solve(float dt, float idt)
	{
		/* velocity correcting impulse (may have limits) */

		/* anchor points velocities: rdot = v + w x r */
		rAdot = bodyA->V + vec2f::cross(bodyA->W, rA);
		rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
		/* anchor points relative velocity */
		rABdot = rBdot - rAdot;

		/* velocity constraint (is ideally = 0) */
		Cdot = vec2f::dot(un, rABdot) - V_target;
		/* calculate & clamp lambda, add to accumulated */
		lambda_vel = -iK * idt * Cdot;
		if(F_hasLimits)
		{
			lambda_vel = clampf(lambdaAcc_vel + lambda_vel, F_low_limit, F_high_limit) - lambdaAcc_vel;
			lambdaAcc_vel += lambda_vel;
		}
		/* calculate and apply velocity correcting impulse */
		P_vel = un * dt * lambda_vel;
		bodyA->applyImpulse(-P_vel, rA);
		bodyB->applyImpulse(P_vel, rB);

		/* position correcting impulse (unlimited) */

		/* calculate and apply impulse */
		lambda_pos = -iK * (bias);
		P_pos = un * lambda_pos;
		bodyA->applyImpulse(-P_pos, rA);
		bodyB->applyImpulse(P_pos, rB);
	}

	void render()
	{
		vec2f	vA = bodyA->X + mat2(bodyA->R) * anchorA,			/* global anchor point for A */
				vB = bodyB->X + mat2(bodyB->R) * anchorB,			/* global anchor point for B */
				vAB = vB - vA;										/* joint vector */
		float	vAB_len = vAB.norm();								/* */
		vec2f	vABn = vAB /vAB_len,								/* normalized joint vector */
				v = vA,												/* initial point along joint vector */
				v_inc = vABn * stroke_height,						/* increment per stroke along joint vector */
				vn = vec2f::getNormal(vA, vB).normalize() * (stroke_width/2.0f),	/* joint vector normal */
				vn_inc = vn * stroke_dwidth;						/* increment of normal width (stroke width) per stroke */
		int		nbrFullStrokes = vAB_len / stroke_height;			/* */ 
		
		glEnable(GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(1.0f, 1.0f, 0.0f, 0.5f);
		vn += vn_inc;
		glBegin(GL_QUADS);
		glVertex2d(v.x - vn.x, v.y - vn.y);
		glVertex2d(v.x + vn.x, v.y + vn.y);
		v += v_inc;
		glVertex2d(v.x + vn.x, v.y + vn.y);
		glVertex2d(v.x - vn.x, v.y - vn.y);
		glEnd();
		glDisable(GL_BLEND);

		vn -= vn_inc;
		v -= v_inc;

		/* render; a square per full stroke */
		glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
		for(int i = 0; i < nbrFullStrokes; i++)
		{
			vn += vn_inc;
			glBegin(GL_LINE_LOOP);
			glVertex2d(v.x - vn.x, v.y - vn.y);
			glVertex2d(v.x + vn.x, v.y + vn.y);
			v += v_inc;
			glVertex2d(v.x + vn.x, v.y + vn.y);
			glVertex2d(v.x - vn.x, v.y - vn.y);
			glEnd();
		}
		/* render the final, partial stroke */
		vn += vn_inc;
		glBegin(GL_LINE_LOOP);
		glVertex2d(v.x - vn.x, v.y - vn.y);
		glVertex2d(v.x + vn.x, v.y + vn.y);
		v = vB;
		glVertex2d(v.x + vn.x, v.y + vn.y);
		glVertex2d(v.x - vn.x, v.y - vn.y);
		glEnd();
	}

private:

	vec2f	rA, rB,					/* transformed anchor points */
			rAdot, rBdot, rABdot,	/* anchor points velocities / relative velocity */
			u, un,					/* anchor points direction */
			P_vel, P_pos;			/* constraint impulse */
	float	C, Cdot,				/* position and velocity constraint */
			bias, ERP,				/* bias term (for position correction) / error reduction parameter (ERP) */
			lambda_vel, lambda_pos,	/* constraint impulse magnitude */
			lambdaAcc_vel,			/* */
			K, iK;					/* constraint effective mass */
	float	u_len, crAu, crBu;		/* */

	float	L_min, L_max, L_target,
			V_max, F_max, V_target,
			F_low_limit, F_high_limit,
			stroke_width, stroke_dwidth, stroke_height;

	bool	F_hasLimits;
	t2LinearActuatorJointMode act_mode;
};

#endif /* T2DJOINT_H */