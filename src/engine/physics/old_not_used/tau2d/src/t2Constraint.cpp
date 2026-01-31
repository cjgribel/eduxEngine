
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

#include "t2Constraint.h"

/*
	linear constraint
*/
t2LinearConstraint::t2LinearConstraint(	t2Body* bodyA,
										t2Body* bodyB,
										vec2f anchorA,
										vec2f anchorB)
	:	t2Joint(bodyA, bodyB, anchorA, anchorB, JOINT_DEFAULT)
{ }

/*
	angular constraint
*/
t2AngularConstraint::t2AngularConstraint(	t2Body* bodyA,
											t2Body* bodyB)
	:	t2Joint(bodyA, bodyB, JOINT_DEFAULT)
{ }

/*
	revolute constraint
*/
t2RevoluteConstraint::t2RevoluteConstraint(	t2Body* bodyA,
											t2Body* bodyB,
											vec2f anchorA,
											vec2f anchorB)
	:	t2Joint(bodyA, bodyB, anchorA, anchorB, JOINT_REVOLUTE_B), ERP_lin(0.2f)
{ }

void t2RevoluteConstraint::init_solve(float dt, float idt)
{
	/* transformed anchor points */
	rA = mat2(bodyA->R) * anchorA;
	rB = mat2(bodyB->R) * anchorB;

	/* constraint effective mass:

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
	mc_lin = K_lin.invert();

	/* velocity bias term */
	C_lin = (bodyB->X + rB) - (bodyA->X + rA);
	bias_lin = C_lin * ERP_lin * idt;
}

void t2RevoluteConstraint::solve( float dt, float idt )
{
	/* velocity of anchor points: rdot = v + w x r */
	rAdot = bodyA->V + vec2f::cross(bodyA->W, rA);
	rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
	/* anchor points relative velocity */
	rABdot = rBdot - rAdot;

	/* velocity constraint (is ideally = 0) */
	Cdot_lin = rABdot;
	/* calculate and apply impulse */
	lambda_lin = -mc_lin * (Cdot_lin + bias_lin);
	P_lin = lambda_lin;
	bodyA->applyImpulse(-P_lin, rA);
	bodyB->applyImpulse(P_lin, rB);
}

/*
	angular actuator constraint
*/

t2AngularActuatorConstraint::t2AngularActuatorConstraint(	t2Body* bodyA,
															t2Body* bodyB)
	:	t2AngularConstraint(bodyA, bodyB),
		actuatorEnabled(false)
{ }

void t2AngularActuatorConstraint::enableActuator( float maxAngularVelocity, float maxTorque )
{
	W_act_target = maxAngularVelocity;
	T_act_max = maxTorque;
	actuatorEnabled = true;
}

void t2AngularActuatorConstraint::disableActuator()
{
	actuatorEnabled = false;
}

void t2AngularActuatorConstraint::init_solve( float dt, float idt )
{
	if(actuatorEnabled)
	{
		/* constraint effective mass */
		K_act = bodyA->iI + bodyB->iI;
		mc_act = 1.0f / K_act;
		lambdaAcc_act = 0.0f;
	}
}

void t2AngularActuatorConstraint::solve( float dt, float idt )
{
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
}

/*
	distance constraint
	constrain bodies to a fixed relative distance
*/
#if 0
t2DistanceConstraint::t2DistanceConstraint(	t2Body* bodyA,
											t2Body* bodyB,
											vec2f anchorA,
											vec2f anchorB,
											float L)
	:	t2LinearConstraint(bodyA, bodyB, anchorA, anchorB),
		L(L),
		ERP(0.15f)
{ }

void t2DistanceConstraint::init_solve( float dt, float idt )
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
	mc = 1.0f / K;

	lambdaAcc = 0.0f;
}

void t2DistanceConstraint::solve( float dt, float idt )
{
	/* anchor points velocities: rdot = v + w x r */
	rAdot = bodyA->V + vec2f::cross(bodyA->W, rA);
	rBdot = bodyB->V + vec2f::cross(bodyB->W, rB);
	/* anchor points relative velocity */
	rABdot = rBdot - rAdot;

	/* velocity constraint (is ideally = 0) */
	Cdot = vec2f::dot(un, rABdot);

	/* calculate and apply impulse */
	lambda = -mc * (Cdot + bias);
	P = un * lambda;
	bodyA->applyImpulse(-P, rA);
	bodyB->applyImpulse(P, rB);
}

void t2DistanceConstraint::render()
{
	vec2f	vA = bodyA->X + mat2(bodyA->R) * anchorA,
			vB = bodyB->X + mat2(bodyB->R) * anchorB;

	glBegin(GL_LINES);
	glVertex2d(vA.x, vA.y);
	glVertex2d(vB.x, vB.y);
	glEnd();
}
#endif

/*
	angle constraint
	constrain bodies to a fixed relative distance
*/
t2AngleConstraint::t2AngleConstraint(	t2Body* bodyA,
										t2Body* bodyB,
										float R_rel)
	:	t2AngularConstraint(bodyA, bodyB),
		R_rel(R_rel),
		ERP_ang(0.2f),
		maxCorr_ang(10.0f)
{ }

void t2AngleConstraint::init_solve( float dt, float idt )
{
	/* angular bias */
	C_ang = bodyB->R - bodyA->R + R_rel;
	C_ang = clampf(C_ang, -maxCorr_ang, maxCorr_ang);
	bias_ang = C_ang * ERP_ang * idt;

	/* angular constraint effective mass */
	mc_ang = 1.0f / (bodyA->iI + bodyB->iI);
}

void t2AngleConstraint::solve( float dt, float idt )
{
	/* angular velocity constraint (is ideally = 0) */
	Cdot_ang = bodyB->W - bodyA->W;

	/* calculate and apply angular impulse */
	P_ang = -mc_ang * (Cdot_ang + bias_ang);
	bodyA->W -= bodyA->iI * P_ang;
	bodyB->W += bodyB->iI * P_ang;
}

/*
	prismatic constraint
*/
t2PrismaticConstraint::t2PrismaticConstraint(	t2Body* bodyA,
												t2Body* bodyB,
												vec2f anchorA,
												vec2f anchorB,
												float R_rel,
												float R_joint)
	:	t2LinearConstraint(bodyA, bodyB, anchorA, anchorB),
		R_joint(R_joint),
		l_init(1.0f, 0.0f),
		ERP_lin(0.2f),
		maxCorr_lin(0.2f)
{ }

void t2PrismaticConstraint::init_solve( float dt, float idt )
{
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
	K = bodyA->imass + bodyB->imass + bodyA->iI * crAl * crAl + bodyB->iI * crBl * crBl;
	mc_lin = 1.0f / K;
}

void t2PrismaticConstraint::solve( float dt, float idt )
{
	/* linear velocity constraint (is ideally = 0) */
	Cdot_lin =
		-vec2f::dot(l, bodyA->V) -
		bodyA->W * vec2f::cross(d+rA, l) +
		vec2f::dot(l, bodyB->V) +
		bodyB->W * vec2f::cross(rB, l);

	/* calculate and apply linear impulse */
	lambda_lin = -mc_lin * (Cdot_lin + bias_lin);
	P_lin = l * lambda_lin;
	bodyA->applyImpulse(-P_lin, d+rA);
	bodyB->applyImpulse(P_lin, rB);
}

t2AngularLimitsConstraint::t2AngularLimitsConstraint(	t2Body* bodyA,
														t2Body* bodyB,
														float rad_min,
														float rad_max,
														float rad_init)
	:	t2AngularConstraint(bodyA, bodyB),
		phi_min(rad_min),
		phi_max(rad_max),
		R_init(rad_init),
		ERP_ang(0.2f),
		lambdaAcc_ang(0.0f),
		limitsActive(false), upperLimitActive(false)		
{ }
/*
	angular limits constraint
*/
void t2AngularLimitsConstraint::init_solve( float dt, float idt )
{
	/* decide angle between bodies */
	phi = bodyB->R - bodyA->R - R_init;

	if(phi < phi_min || phi > phi_max)
	{
		/* decide what limit is being breached and by how much */
		if(phi < phi_min)
		{
			C_ang = -clampf(-(phi - phi_min + ang_tol), 0.0f, INF);
			upperLimitActive = true;
		}
		else
		{
			C_ang = clampf(phi - phi_max - ang_tol, 0.0f, INF);
			upperLimitActive = false;
		}
		/* velocity bias term */
		bias_ang = C_ang * ERP_ang * idt;
		/* constraint effective mass */
		mc_ang = 1.0f / (bodyA->iI + bodyB->iI);

		lambdaAcc_ang = 0.0f;
		limitsActive = true;
	}
	else
		limitsActive = false;
}

void t2AngularLimitsConstraint::solve( float dt, float idt )
{
	if(limitsActive)
	{
		/* angular velocity constraint (is ideally = 0) */
		Cdot_ang = bodyB->W - bodyA->W;

		/* calculate & clamp lambda, add to accumulated */
		lambda_ang = -mc_ang * (Cdot_ang + bias_ang);
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