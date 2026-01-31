
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

#include "t2Ragdoll.h"

t2Ragdoll::t2Ragdoll(vec2f pos, float scale, float density, unsigned short collisionGroupId, t2World *world)
	: bodypart_count(0), joint_count(0), world(world)
{
	float
		mass_scale = density * pow(scale, 2),	// mass scaling factor
		I_scale = density * pow(scale, 4);		// moment of inertia scaling factor
	
	mat2 scaleMx(scale, scale);

	// feet

	vec2f feet_v[5] = { vec2f(-0.0933f, 0.0632f), vec2f(-0.1033f, -0.0468f), vec2f(0.1407f, -0.0448f), vec2f(0.1387f, -0.0148f), vec2f(-0.0093f, 0.0632f) };

	t2PolygonGeometry *rfoot_geom = new t2PolygonGeometry(feet_v, 5, vec2f());
	rfoot_geom->transformLocal(scaleMx);
	rfoot = new t2Body(pos + vec2f(0.043f, -0.8212f) * scale, false);
	rfoot->addGeometry(rfoot_geom);
	rfoot->setMass(mass_scale * 0.0201f, I_scale * 9.6090e-5f);
	bodyparts[bodypart_count++] = rfoot;

	t2PolygonGeometry *lfoot_geom = new t2PolygonGeometry(feet_v, 5, vec2f());
	lfoot_geom->transformLocal(scaleMx);
	lfoot = new t2Body(pos + vec2f(0.043f, -0.8212f) * scale, false);
	lfoot->addGeometry(lfoot_geom);
	lfoot->setMass(mass_scale * 0.0201f, I_scale * 9.6090e-5f);
	bodyparts[bodypart_count++] = lfoot;

	//rfoot = new t2Body(pos + vec2f(0.043f, -0.8212f) * scale, false);
	//t2PolygonGeometry *rfoot_geom = new t2PolygonGeometry();
	//rfoot_geom->addVertex(vec2f(	-0.0933f,	0.0632f)	* scale);
	//rfoot_geom->addVertex(vec2f(	-0.1033f,	-0.0468f)	* scale);
	//rfoot_geom->addVertex(vec2f(	0.1407f,	-0.0448f)	* scale);
	//rfoot_geom->addVertex(vec2f(	0.1387f,	-0.0148f)	* scale);
	//rfoot_geom->addVertex(vec2f(	-0.0093f,	0.0632f)	* scale);
	//rfoot->addGeometry(rfoot_geom);
	//rfoot->setMass(mass_scale * 0.0201f, I_scale * 9.6090e-5f);
	//bodyparts[bodypart_count++] = rfoot;

	//lfoot = new t2Body(pos + vec2f(0.043f, -0.8212f) * scale, false);
	//t2PolygonGeometry *lfoot_geom = new t2PolygonGeometry();
	//lfoot_geom->addVertex(vec2f(	-0.0933f,	0.0632f)	* scale);
	//lfoot_geom->addVertex(vec2f(	-0.1033f,	-0.0468f)	* scale);
	//lfoot_geom->addVertex(vec2f(	0.1407f,	-0.0448f)	* scale);
	//lfoot_geom->addVertex(vec2f(	0.1387f,	-0.0148f)	* scale);
	//lfoot_geom->addVertex(vec2f(	-0.0093f,	0.0632f)	* scale);
	//lfoot->addGeometry(lfoot_geom);
	//lfoot->setMass(mass_scale * 0.0201f, I_scale * 9.6090e-5f);
	//bodyparts[bodypart_count++] = lfoot;

	// calves

	rcalf = new t2Body(pos + vec2f(-0.0350f, -0.5474f) * scale, false);
	t2PolygonGeometry *rcalf_geom = new t2PolygonGeometry();
	rcalf_geom->addVertex(vec2f(	-0.0530f,	-0.2076f)	* scale);
	rcalf_geom->addVertex(vec2f(	0.0360f,	-0.2076f)	* scale);
	rcalf_geom->addVertex(vec2f(	0.0630f,	0.1934f)	* scale);
	rcalf_geom->addVertex(vec2f(	-0.0470f,	0.1934f)	* scale);
	rcalf->addGeometry(rcalf_geom);
	rcalf->setMass(mass_scale * 0.0399f, I_scale * 5.6686e-4f);
	bodyparts[bodypart_count++] = rcalf;

	lcalf = new t2Body(pos + vec2f(-0.0350f, -0.5474f) * scale, false);
	t2PolygonGeometry *lcalf_geom = new t2PolygonGeometry();
	lcalf_geom->addVertex(vec2f(	-0.0530f,	-0.2076f)	* scale);
	lcalf_geom->addVertex(vec2f(	0.0360f,	-0.2076f)	* scale);
	lcalf_geom->addVertex(vec2f(	0.0630f,	0.1934f)	* scale);
	lcalf_geom->addVertex(vec2f(	-0.0470f,	0.1934f)	* scale);
	lcalf->addGeometry(lcalf_geom);
	lcalf->setMass(mass_scale * 0.0399f, I_scale * 5.6686e-4f);
	bodyparts[bodypart_count++] = lcalf;

	// ankles

	rankle = new t2RevoluteJoint(rfoot, rcalf, vec2f(-0.0493f, 0.0712f) * scale, vec2f(-0.0100f, -0.2026f) * scale);
	rankle->enableAngularLimits(-0.523598776f, 0.523598776f, 0.0f);
	joints[joint_count++] = rankle;

	lankle = new t2RevoluteJoint(lfoot, lcalf, vec2f(-0.0493f, 0.0712f) * scale, vec2f(-0.0100f, -0.2026f) * scale);
	lankle->enableAngularLimits(-0.523598776f, 0.523598776f, 0.0f);
	joints[joint_count++] = lankle;

	// thighs

	rthigh = new t2Body(pos + vec2f(-0.0053f, -0.1906f) * scale, false);
	t2PolygonGeometry *rthigh_geom = new t2PolygonGeometry();
	rthigh_geom->addVertex(vec2f(	-0.0747f,	-0.1684f)	* scale);
	rthigh_geom->addVertex(vec2f(	0.0313f,	-0.1684f)	* scale);
	rthigh_geom->addVertex(vec2f(	0.1063f,	0.1426f)	* scale);
	rthigh_geom->addVertex(vec2f(	-0.0697f,	0.1426f)	* scale);
	rthigh->addGeometry(rthigh_geom);
	rthigh->setMass(mass_scale * 0.0439f, I_scale * 4.2904e-4f);
	bodyparts[bodypart_count++] = rthigh;

	lthigh = new t2Body(pos + vec2f(-0.0053f, -0.1906f) * scale, false);
	t2PolygonGeometry *lthigh_geom = new t2PolygonGeometry();
	lthigh_geom->addVertex(vec2f(	-0.0747f,	-0.1684f)	* scale);
	lthigh_geom->addVertex(vec2f(	0.0313f,	-0.1684f)	* scale);
	lthigh_geom->addVertex(vec2f(	0.1063f,	0.1426f)	* scale);
	lthigh_geom->addVertex(vec2f(	-0.0697f,	0.1426f)	* scale);
	lthigh->addGeometry(lthigh_geom);
	lthigh->setMass(mass_scale * 0.0439f, I_scale * 4.2904e-4f);
	bodyparts[bodypart_count++] = lthigh;

	// knees

	rknee = new t2RevoluteJoint(rcalf, rthigh, vec2f(0.0050f, 0.1924f) * scale, vec2f(-0.0247f, -0.1644f) * scale);
	rknee->enableAngularLimits(0.0f, 2.61799388f, 0.0f);
	joints[joint_count++] = rknee;

	lknee = new t2RevoluteJoint(lcalf, lthigh, vec2f(0.0050f, 0.1924f) * scale, vec2f(-0.0247f, -0.1644f) * scale);
	lknee->enableAngularLimits(0.0f, 2.61799388f, 0.0f);
	joints[joint_count++] = lknee;

	// pelvis

	pelvis = new t2Body(pos + vec2f(0.0158f, 0.0444f) * scale, false);
	t2PolygonGeometry *pelvis_geom = new t2PolygonGeometry();
	pelvis_geom->addVertex(vec2f(	-0.0878f,	-0.0944f)	* scale);
	pelvis_geom->addVertex(vec2f(	0.0842f,	-0.0924f)	* scale);
	pelvis_geom->addVertex(vec2f(	0.1272f,	0.0876f)	* scale);
	pelvis_geom->addVertex(vec2f(	-0.0808f,	0.0876f)	* scale);
	pelvis_geom->addVertex(vec2f(	-0.1258f,	0.0066f)	* scale);
	pelvis->addGeometry(pelvis_geom);
	pelvis->setMass(mass_scale * 0.0382f, I_scale * 2.4608e-4f);
	bodyparts[bodypart_count++] = pelvis;

	// hips

	rhip = new t2RevoluteJoint(rthigh, pelvis, vec2f(0.0103f, 0.1456f) * scale, vec2f(-0.0108f, -0.0894f) * scale);
	rhip->enableAngularLimits(-1.8f, 0.0f, 0.0f); //-2.26892803f
	joints[joint_count++] = rhip;
	
	lhip = new t2RevoluteJoint(lthigh, pelvis, vec2f(0.0103f, 0.1456f) * scale, vec2f(-0.0108f, -0.0894f) * scale);
	lhip->enableAngularLimits(-2.0f, 0.0f, 0.0f);
	joints[joint_count++] = lhip;

	// stomach

	stomach = new t2Body(pos + vec2f(0.0497, 0.2249f) * scale, false);
	t2PolygonGeometry *stomach_geom = new t2PolygonGeometry();
	stomach_geom->addVertex(vec2f(	-0.1037f,	-0.0939f)	* scale);
	stomach_geom->addVertex(vec2f(	0.0923f,	-0.0939f)	* scale);
	stomach_geom->addVertex(vec2f(	0.1033f,	0.0941f)	* scale);
	stomach_geom->addVertex(vec2f(	-0.0917f,	0.0941f)	* scale);
	stomach->addGeometry(stomach_geom);
	stomach->setMass(mass_scale * 0.0368f, I_scale * 2.2572e-4f);
	bodyparts[bodypart_count++] = stomach;

	// lower ab

	lowerab = new t2RevoluteJoint(pelvis, stomach, vec2f(0.0192f, 0.0906f) * scale, vec2f(-0.0147f, -0.0899f) * scale);
	lowerab->enableAngularLimits(-0.523598776f, 0.523598776f, 0.0f);
	joints[joint_count++] = lowerab;

	// chest

	chest = new t2Body(pos + vec2f(0.0249f, 0.4367f) * scale, false);
	t2PolygonGeometry *chest_geom = new t2PolygonGeometry();
	chest_geom->addVertex(vec2f(	-0.0659f,	-0.1127f)	* scale);
	chest_geom->addVertex(vec2f(	0.1261f,	-0.1127f)	* scale);
	chest_geom->addVertex(vec2f(	0.0201f,	0.1343f)	* scale);
	chest_geom->addVertex(vec2f(	-0.0919f,	0.1343f)	* scale);
	chest->addGeometry(chest_geom);
	chest->setMass(mass_scale * 0.0375f, I_scale * 2.7708e-4f);
	bodyparts[bodypart_count++] = chest;

	// upper ab

	upperab = new t2RevoluteJoint(stomach, chest, vec2f(-0.0047f, 0.0951f) * scale, vec2f(0.0201f, -0.1167f) * scale);
	upperab->enableAngularLimits(-0.523598776f, 0.174532925f, 0.0f);
	joints[joint_count++] = upperab;

	// neck

	//neck = new t2Body(pos + vec2f(-0.0052f, 0.5985f) * scale, false);
	//t2PolygonGeometry *neck_geom = new t2PolygonGeometry();
	//neck_geom->addVertex(vec2f(	-0.0588f,	-0.0265f)	* scale);
	//neck_geom->addVertex(vec2f(	0.0522f,	-0.0265f)	* scale);
	//neck_geom->addVertex(vec2f(	0.0572f,	0.0215f)	* scale);
	//neck_geom->addVertex(vec2f(	-0.0428f,	0.0325f)	* scale);
	//neck->addGeometry(neck_geom);
	//neck->setMass(mass_scale * 0.0057f, I_scale * 6.7221e-6f);
	//bodyparts[bodypart_count++] = neck;

	// lower neck

	//lowerneck = new t2RevoluteJoint(chest, neck, vec2f(-0.0399f, 0.1383f) * scale, vec2f(-0.0098f, -0.0235f) * scale);
	//lowerneck->enableAngularLimits(-0.174532925f, 0.174532925f, 0.0f);
	//joints[joint_count++] = lowerneck;

	// head

	head = new t2Circle(pos + vec2f(0.0220f, 0.7380f) * scale, 0.115f * scale, density/2.0f);
	bodyparts[bodypart_count++] = head;

	// lower neck

	lowerneck = new t2RevoluteJoint(chest, head, vec2f(-0.0399f, 0.1383f) * scale, vec2f(-0.0270f, -0.1080f) * scale);
	lowerneck->enableAngularLimits(-0.610865238f, 0.785398163f, 0.0f);
	joints[joint_count++] = lowerneck;

	 //upper neck

	//upperneck = new t2RevoluteJoint(neck, head, vec2f(0.0002f, 0.0315f) * scale, vec2f(-0.0270f, -0.1080f) * scale);
	//upperneck->enableAngularLimits(-0.610865238f, 0.785398163f, 0.0f);
	//joints[joint_count++] = upperneck;

	// upper arms

	rupperarm = new t2Body(pos + vec2f(-0.0074f, 0.4330f) * scale, false);
	t2PolygonGeometry *rupperarm_geom = new t2PolygonGeometry();
	rupperarm_geom->addVertex(vec2f(	-0.0276f,	-0.1460f)	* scale);
	rupperarm_geom->addVertex(vec2f(	0.0394f,	-0.1460f)	* scale);
	rupperarm_geom->addVertex(vec2f(	0.0494f,	0.1300f)	* scale);
	rupperarm_geom->addVertex(vec2f(	-0.0416f,	0.1300f)	* scale);
	rupperarm_geom->addVertex(vec2f(	-0.0556f,	0.0430f)	* scale);
	rupperarm->addGeometry(rupperarm_geom);
	rupperarm->setMass(mass_scale * 0.0243f, I_scale * 1.6261e-4f);
	bodyparts[bodypart_count++] = rupperarm;

	lupperarm = new t2Body(pos + vec2f(-0.0074f, 0.4330f) * scale, false);
	t2PolygonGeometry *lupperarm_geom = new t2PolygonGeometry();
	lupperarm_geom->addVertex(vec2f(	-0.0276f,	-0.1460f)	* scale);
	lupperarm_geom->addVertex(vec2f(	0.0394f,	-0.1460f)	* scale);
	lupperarm_geom->addVertex(vec2f(	0.0494f,	0.1300f)	* scale);
	lupperarm_geom->addVertex(vec2f(	-0.0416f,	0.1300f)	* scale);
	lupperarm_geom->addVertex(vec2f(	-0.0556f,	0.0430f)	* scale);
	lupperarm->addGeometry(lupperarm_geom);
	lupperarm->setMass(mass_scale * 0.0243f, I_scale * 1.6261e-4f);
	bodyparts[bodypart_count++] = lupperarm;

	// shoulders

	rshoulder = new t2RevoluteJoint(chest, rupperarm, vec2f(-0.0399f, 0.1083f) * scale, vec2f(-0.0076f, 0.1120f) * scale);
	rshoulder->enableAngularLimits(-1.04719755f, 3.14159265f, 0.0f);
	joints[joint_count++] = rshoulder;

	lshoulder = new t2RevoluteJoint(chest, lupperarm, vec2f(-0.0399f, 0.1083f) * scale, vec2f(-0.0076f, 0.1120f) * scale);
	lshoulder->enableAngularLimits(-1.04719755f, 3.14159265f, 0.0f); // edit
	joints[joint_count++] = lshoulder;

	// forearms

	rforearm = new t2Body(pos + vec2f(-0.0027f, 0.1657f) * scale, false);
	t2PolygonGeometry *rforearm_geom = new t2PolygonGeometry();
	rforearm_geom->addVertex(vec2f(	-0.0323f,	-0.1227f)	* scale);
	rforearm_geom->addVertex(vec2f(	0.0237f,	-0.1227f)	* scale);
	rforearm_geom->addVertex(vec2f(	0.0347f,	0.1193f)	* scale);
	rforearm_geom->addVertex(vec2f(	-0.0263f,	0.1193f)	* scale);
	rforearm->addGeometry(rforearm_geom);
	rforearm->setMass(mass_scale * 0.0142f, I_scale * 7.3179e-5f);
	bodyparts[bodypart_count++] = rforearm;

	lforearm = new t2Body(pos + vec2f(-0.0027f, 0.1657f) * scale, false);
	t2PolygonGeometry *lforearm_geom = new t2PolygonGeometry();
	lforearm_geom->addVertex(vec2f(	-0.0323f,	-0.1227f)	* scale);
	lforearm_geom->addVertex(vec2f(	0.0237f,	-0.1227f)	* scale);
	lforearm_geom->addVertex(vec2f(	0.0347f,	0.1193f)	* scale);
	lforearm_geom->addVertex(vec2f(	-0.0263f,	0.1193f)	* scale);
	lforearm->addGeometry(lforearm_geom);
	lforearm->setMass(mass_scale * 0.0142f, I_scale * 7.3179e-5f);
	bodyparts[bodypart_count++] = lforearm;

	// elbows

	relbow = new t2RevoluteJoint(rupperarm, rforearm, vec2f(0.0024f, -0.1430f) * scale, vec2f(-0.0023f, 0.1243f) * scale);
	relbow->enableAngularLimits(0.0f, 2.7925268f, 0.0f);
	joints[joint_count++] = relbow;

	lelbow = new t2RevoluteJoint(lupperarm, lforearm, vec2f(0.0024f, -0.1430f) * scale, vec2f(-0.0023f, 0.1243f) * scale);
	lelbow->enableAngularLimits(0.0f, 2.7925268f, 0.0f);
	joints[joint_count++] = lelbow;

	// hands

	rhand = new t2Body(pos + vec2f(-0.0048f, -0.0334f) * scale, false);
	t2PolygonGeometry *rhand_geom = new t2PolygonGeometry();
	rhand_geom->addVertex(vec2f(	-0.0412f,	-0.0716f)	* scale);
	rhand_geom->addVertex(vec2f(	0.0158f,	-0.0826f)	* scale);
	rhand_geom->addVertex(vec2f(	0.0528f,	0.0244f)	* scale);
	rhand_geom->addVertex(vec2f(	0.0198f,	0.0754f)	* scale);
	rhand_geom->addVertex(vec2f(	-0.0332f,	0.0754f)	* scale);
	rhand->addGeometry(rhand_geom);
	rhand->setMass(mass_scale * 0.0111f, I_scale * 2.4844e-5f);
	bodyparts[bodypart_count++] = rhand;

	lhand = new t2Body(pos + vec2f(-0.0048f, -0.0334f) * scale, false);
	t2PolygonGeometry *lhand_geom = new t2PolygonGeometry();
	lhand_geom->addVertex(vec2f(	-0.0412f,	-0.0716f)	* scale);
	lhand_geom->addVertex(vec2f(	0.0158f,	-0.0826f)	* scale);
	lhand_geom->addVertex(vec2f(	0.0528f,	0.0244f)	* scale);
	lhand_geom->addVertex(vec2f(	0.0198f,	0.0754f)	* scale);
	lhand_geom->addVertex(vec2f(	-0.0332f,	0.0754f)	* scale);
	lhand->addGeometry(lhand_geom);
	lhand->setMass(mass_scale * 0.0111f, I_scale * 2.4844e-5f);
	bodyparts[bodypart_count++] = lhand;

	// wrists

	rwrist = new t2RevoluteJoint(rforearm, rhand, vec2f(-0.0073f, -0.1207f) * scale, vec2f(-0.0052f, 0.0784f) * scale);
	rwrist->enableAngularLimits(-0.174532925f, 0.174532925f, 0.0f);
	joints[joint_count++] = rwrist;

	lwrist = new t2RevoluteJoint(lforearm, lhand, vec2f(-0.0073f, -0.1207f) * scale, vec2f(-0.0052f, 0.0784f) * scale);
	lwrist->enableAngularLimits(-0.174532925f, 0.174532925f, 0.0f);
	joints[joint_count++] = lwrist;

	
	// add bodies to world, calc mass, set common properties
	float tot_mass = 0.0f;
	for(int i = 0; i < bodypart_count; i++)
	{
//        bodyparts[i]->restitution = 1;
		bodyparts[i]->collisionGroupId = collisionGroupId;
		bodyparts[i]->collisionFilter = T2_COLLISION_GROUP_ALL ^ collisionGroupId;
		world->addBody(bodyparts[i]);
		tot_mass += bodyparts[i]->mass;
	}
	//printf("ragdoll total mass: %f kg\n", tot_mass);

	// set colors
	rfoot->setColor(0.0f, 0.0f, 0.0f); lfoot->setColor(0.0f, 0.0f, 0.0f);
	rcalf->setColor(0.0f, 0.0f, 0.7f); lcalf->setColor(0.0f, 0.0f, 0.7f);
	rthigh->setColor(0.0f, 0.0f, 0.7f); lthigh->setColor(0.0f, 0.0f, 0.7f);
	pelvis->setColor(0.0f, 0.0f, 0.7f);
	stomach->setColor(0.7f, 0.0f, 0.0f);
	chest->setColor(0.7f, 0.0f, 0.0f);
	rupperarm->setColor(0.7f, 0.0f, 0.0f); lupperarm->setColor(0.7f, 0.0f, 0.0f);
	rforearm->setColor(0.7f, 0.0f, 0.0f); lforearm->setColor(0.7f, 0.0f, 0.0f);
	rhand->setColor(0.87f, 0.72f, 0.46f); lhand->setColor(0.87f, 0.72f, 0.46f);
	//neck->setColor(0.87f, 0.72f, 0.46f);
	head->setColor(0.87f, 0.72f, 0.46f);


	// add joints to world
	for(int i = 0; i < joint_count; i++)
	{
		world->joints.push_back(joints[i]);
	}

	
}

void t2Ragdoll::disassemble()
{
	for (int i = 0; i < joint_count; i++) world->joints.remove(joints[i]);
}