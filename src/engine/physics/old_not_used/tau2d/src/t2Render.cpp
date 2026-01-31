
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2009, cjgribel@gmail.com
*/

#include "t2Render.h"
#include "t2Geometry.h"

void renderHUD(float velocity)
{
	// velocity meter needle
	glColor4f(0.3f, 0.3f, 0.3f, 1.0f);
	vec2f needle = mat2(-velocity/450.0f*3.6f*2.0f*PI) * (vec2f(0.0f, -0.13f));
	//glLineWidth(3.0f);
	drawLineGL(vec2f(0.8f, 0.2f), needle);
	//glLineWidth(2.0f);
	// velocity meter case
	int steps = 18;
	float rad = 0.0f, drad = 2.0f * PI / steps;
	glBegin(GL_LINE_LOOP);
	for(int k = 0; k < steps; k++)
	{
		glVertex2d(0.8f + 0.15f * cos(rad), 0.2f + 0.15f * sin(rad));
		rad += drad;
	}
	glEnd();
}

void render(t2World *world, const unsigned int &render_mask)
{
	t2Body *body;
	t2Geometry *geom;
	t2PolygonGeometry *pgeom;
	t2CircleGeometry *cgeom;

	for(int i = 0; i < world->nbrBodies; i++){

		body = world->bodies[i];
		bool isSleeping =	(render_mask & T2_RENDER_SLEEP_STATUS) &&
							body->framesAtRest > T2_FRAMES_AT_REST_REQUIRED;

		for(int j = 0; j < body->nbrGeometries; j++)
		{
			geom = body->geometries[j];

			// AABB

			if(render_mask & T2_RENDER_AABB)
			{
				glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
				glBegin(GL_LINE_LOOP);
					glVertex2d(geom->aabb.x_min, geom->aabb.y_min);
					glVertex2d(geom->aabb.x_max, geom->aabb.y_min);
					glVertex2d(geom->aabb.x_max, geom->aabb.y_max);
					glVertex2d(geom->aabb.x_min, geom->aabb.y_max);
				glEnd();
			}

			// render body

			if(geom->type == GEOMTYPE_POLY)
			{
				pgeom = static_cast<t2PolygonGeometry*> (geom);

				// polygon interior

				if(render_mask & T2_RENDER_FILL && !isSleeping)
				{
					glEnable(GL_BLEND);
					glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glColor4f(body->colorFillR, body->colorFillG, body->colorFillB, 0.5f);

					glBegin(GL_POLYGON);
					for(int k = 0; k < pgeom->nbrVertices; k++)
						glVertex2d(pgeom->vertices[k].x, pgeom->vertices[k].y);
					glEnd();
					glDisable(GL_BLEND);
				}

				// polygon outline
#if 1
				if (!body->hasOutline)
				{
//					if((render_mask & T2_RENDER_SLEEP_STATUS) && body->framesAtRest > T2_FRAMES_AT_REST_REQUIRED)
//						glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
//					else
						glColor4f(body->colorBorderR, body->colorBorderG, body->colorBorderB, 1.0f);

					glBegin(GL_LINE_LOOP);
					for(int k = 0; k < pgeom->nbrVertices; k++)
						glVertex2d(pgeom->vertices[k].x, pgeom->vertices[k].y);
					glEnd();
				}
#endif
				// normals

				if(render_mask & T2_RENDER_NORMALS)
				{
					glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
					for(int k = 0; k < pgeom->nbrVertices; k++)
						drawLineGL(pgeom->vertices[k], pgeom->normals[k] * 0.4f);
				}

			}
			else if(geom->type == GEOMTYPE_CIRCLE)
			{
				cgeom = static_cast<t2CircleGeometry*> (geom);
				int steps = 18;
				float rad = 0.0f, drad = 2.0f * PI / steps;

				// circle interior

				if(render_mask & T2_RENDER_FILL && !isSleeping)
				{
					glEnable(GL_BLEND);
					glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glColor4f(body->colorFillR, body->colorFillG, body->colorFillB, 0.5f);

					glBegin(GL_TRIANGLE_FAN);
					for(int k = 0; k < steps; k++)
					{
						glVertex2d(cgeom->X.x + cgeom->radius * cos(rad), cgeom->X.y + cgeom->radius * sin(rad));
						rad += drad;
					}
					glEnd();
					glDisable(GL_BLEND);
				}

				// circle outline
#if 1
				rad = 0.0f;
				//if((render_mask & T2D_RENDER_SLEEP_STATUS) && body->framesAtRest > world->framesAtRest_req)
				//	glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
				//else
					glColor4f(body->colorBorderR, body->colorBorderG, body->colorBorderB, 1.0f);

				glBegin(GL_LINE_LOOP);
				for(int k = 0; k < steps; k++)
				{
					glVertex2d(cgeom->X.x + cgeom->radius * cos(rad), cgeom->X.y + cgeom->radius * sin(rad));
					rad += drad;
				}
				glEnd();
#endif
#if 1
				// circle radial lines

				drawLineGL(cgeom->X - cgeom->vR1 * 0.5f, cgeom->vR1);
				drawLineGL(cgeom->X - cgeom->vR2 * 0.5f, cgeom->vR2);
#endif
			}

			// local axes: rotation

			if(render_mask & T2_RENDER_LOCAL_SPACE)
			{
				mat2 rot(body->R);
				vec2f
					xAxis = rot * vec2f(0.5f, 0.0f),
					yAxis = rot * vec2f(0.0f, 0.5f);
				glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
				drawLineGL(body->X, xAxis);
				glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
				drawLineGL(body->X, yAxis);
			}

		}	/* for geoms */

		/* body outline (if present explicitly) */

		if (body->hasOutline)
		{
			glColor4f(body->colorBorderR, body->colorBorderG, body->colorBorderB, 1.0f);

			glBegin(GL_LINE_LOOP);
			mat2 rot(body->R);
			for( int k = 0; k < body->vc_outline; k++ )
			{
				vec2f v = body->X + rot * body->vec_outline[k];
				glVertex2d( v.x, v.y );
			}
			glEnd();
		}

	}	/* for bodies */

	// forces

	if(render_mask & T2_RENDER_FORCES)
	{
		for(std::list<t2Force*>::iterator f_it = world->forces.begin(); f_it != world->forces.end(); f_it++)
			(*f_it)->render();
	}
	// joints

	if(render_mask & T2_RENDER_JOINTS)
	{
		for(std::list<t2Joint*>::iterator c_it = world->joints.begin(); c_it != world->joints.end(); c_it++)
			(*c_it)->render();
	}
	// contact points

	if(render_mask & T2_RENDER_CONTACTS)
	{
		for(std::vector<t2ContactJoint>::iterator c_it = world->contacts->begin(); c_it != world->contacts->end(); c_it++)
			c_it->render();
	}
}


void drawLineGL(vec2f p, vec2f v)
{
	glBegin(GL_LINES);
	glVertex2d(p.x, p.y);
	glVertex2d(p.x + v.x, p.y + v.y);
	glEnd();
}
