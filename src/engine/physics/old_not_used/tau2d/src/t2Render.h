
/*
 * Tau2D Rigid Body Dynamics 
 * CJ Gribel (c) 2008-2009, cjgribel@gmail.com
 *
 */

// inclusion guard
#pragma once
#ifndef T2RENDER_H
#define T2RENDER_H

#include "tau2d.h"
#include "t2World.h"
#include "config.h"

/*	*/
void renderHUD(float velocity);

/*	*/
void render(t2World *world, const ui32 &render_mask);

/*	*/
//void render_GLtfm(t2World *world);

/*	*/
void drawLineGL(vec2f p1, vec2f p2);

#endif /* T2DRENDER_H */