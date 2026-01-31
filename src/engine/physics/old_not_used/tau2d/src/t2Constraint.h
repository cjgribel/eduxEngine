
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

#pragma once
#ifndef T2CONSTRAINT_H
#define T2CONSTRAINT_H

#include "tau2d.h"
#include "t2Body.h"
#include "t2Joint.h"

/*
Constraint
	Linear (anchor points)
		Distance (spec of lin limits)
		Revolute
		Prismatic
		*Linear Actuator
		*Linear Limits (in hydraulic)
		*Contact
	Angular (eff. mass)
		Angle (spec of ang limits)
		Angular Limits
		Angular Actuator

todo: hydraulic; lin limits (var & min/max, unilateral) + lin act, switch
todo: keep the 'lin' and 'and' postfix?

 hydraulic: linear act, linear limits (variable + max/min) (+switch)
*/

/*
	linear constraint base class
*/
class t2LinearConstraint : public t2Joint
{

public:

	t2LinearConstraint(	t2Body* bodyA,
						t2Body* bodyB,
						vec2f anchorA,
						vec2f anchorB);
};

/*
	angular constraint base class
*/
class t2AngularConstraint : public t2Joint
{

public:

	t2AngularConstraint(	t2Body* bodyA,
							t2Body* bodyB);
};

/*
	distance constraint
	constrain bodies to a fixed relative distance
*/
#if 0
class t2DistanceConstraint : public t2LinearConstraint
{

public:

	float L;

	t2DistanceConstraint(	t2Body* bodyA,
							t2Body* bodyB,
							vec2f anchorA,
							vec2f anchorB,
							float L);

	void init_solve(float dt, float idt);

	void solve(float dt, float idt);

	void render();

//private:

	vec2f	rA, rB,					/* transformed anchor points */
			rAdot, rBdot, rABdot,	/* anchor points velocities; relative velocity */
			u, un,					/* anchor points direction */
			P;						/* constraint impulse */
	float	C, Cdot,				/* position and velocity constraint */
			bias, ERP,				/* bias term (for position correction); error reduction parameter */
			lambda, lambdaAcc,		/* constraint impulse magnitude */
			mc,						/* constraint effective mass */
			K;						/*  */
	float	u_len, crAu, crBu;		/* */
};
#endif

/*
	angle constraint
	constrain bodies to a fixed relative angle
*/
class t2AngleConstraint : public t2AngularConstraint
{

public:

	t2AngleConstraint(	t2Body* bodyA,
						t2Body* bodyB,
						float R_rel);

	void init_solve(float dt, float idt);

	void solve(float dt, float idt);

private:

	float	R_rel,
			C_ang, Cdot_ang,
			bias_ang,
			ERP_ang,
			mc_ang,
			maxCorr_ang,
			P_ang;
};

/*
	revolute constraint
*/
class t2RevoluteConstraint : public t2Joint
{

public:

	t2RevoluteConstraint(	t2Body* bodyA,
							t2Body* bodyB,
							vec2f anchorA,
							vec2f anchorB);

	void init_solve(float dt, float idt);

	void solve(float dt, float idt);

protected:

	vec2f	rA, rB,					/* transformed anchor points */
			rAdot, rBdot, rABdot,	/* anchor points velocities; relative velocity */
			P_lin, lambda_lin,		/* impulse; magnitude of impulse */
			bias_lin,				/* bias term (for position correction) */
			C_lin, Cdot_lin;		/* position and velocity constraints */
			mat2 mc_lin,			/* constraint effective mass */
			K_lin;					/* */
	float	ERP_lin;				/* error reduction parameter (ERP) */
};

/*
	prismatic constraint
*/
class t2PrismaticConstraint : public t2LinearConstraint
{
public:

	t2PrismaticConstraint(	t2Body* bodyA,
							t2Body* bodyB,
							vec2f anchorA,
							vec2f anchorB,
							float R_rel,
							float R_joint);

	void init_solve(float dt, float idt);

	void solve(float dt, float idt);

private:

	vec2f	rA, rB, d,				/* transformed anchor points; anchor points direction */
			l, l_init,				/* constraint reference direction; inital constraint direction */
			P_lin;					/* linear impulse */
	float	R_joint,				/* joint angle */
			C_lin, Cdot_lin,		/* position and velocity constraint */
			bias_lin, ERP_lin,		/* bias term (for position correction); error reduction parameter */
			mc_lin,					/* effective mass of linear constraint */
			lambda_lin,				/* magnitude of linear impulse */
			maxCorr_lin,			/* max correction (for stability) */
			crAl, crBl,				/* precalculated cross products */
			K;						/* */ 
};

/*
	angular actuator constraint
*/
class t2AngularActuatorConstraint : public t2AngularConstraint
{
public:

	t2AngularActuatorConstraint(	t2Body* bodyA,
									t2Body* bodyB);

	void enableActuator(float maxAngularVelocity, float maxTorque);

	void disableActuator();

	void init_solve(float dt, float idt);

	void solve(float dt, float idt);

protected:

	float	K_act, mc_act,
			Cdot_act,
			lambda_act, lambdaAcc_act,
			P_act,
			W_act_target,
			T_act_max;
	bool	actuatorEnabled;
};

/*
	angular limits constraint
*/
class t2AngularLimitsConstraint : public t2AngularConstraint
{

public:

	t2AngularLimitsConstraint(	t2Body* bodyA,
								t2Body* bodyB,
								float rad_min,
								float rad_max,
								float rad_init);

	void init_solve(float dt, float idt);

	void solve(float dt, float idt);

protected:

	float	C_ang, Cdot_ang,
			ERP_ang,
			bias_ang,
			ang_tol,
			mc_ang,
			lambda_ang, lambdaAcc_ang,
			P_ang,
			phi, phi_min, phi_max,
			R_init;
	bool	limitsActive, upperLimitActive;
};

#endif /* T2CONSTRAINT_H */