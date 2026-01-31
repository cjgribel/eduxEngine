
/*
	Tau2D Dynamics Engine
	CJ Gribel (c) 2008-2011, cjgribel@gmail.com
*/


// inclusion guard
#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include "t2Math.h"

typedef float			f32;
typedef	double			f64;
typedef short			i16;
typedef unsigned short	ui16;
typedef	int				i32;
typedef unsigned int	ui32;

static float            T2_G_x = 0;
static float            T2_G_y = -9.82;

//#define CD_SAP
static ui16              T2_SOLVER_ITERATIONS = 30;

const ui16              T2_MAX_BODIES			= 512;	// 

const ui16              T2_MAX_GEOMETRIES = 512;

const ui16              T2_COLLISION_GROUP_DEFAULT	= 1;		// default collision group
const ui16              T2_COLLISION_GROUP_ALL		= 0xFFFF;	// all collision groups

const f32				T2_RESTING_LIN_VEL_TOL_SQUARED	= 0.08f*0.08f;      // threshold velocities for when to consider a body to be at rest
const f32				T2_RESTING_ANG_VEL_TOL			= deg_to_rad(5.0f);	// 
const i32				T2_FRAMES_AT_REST_REQUIRED		= 15;				// number of frames at rest required to put body to pseudo-sleep

const ui16              T2_MAX_SAP_ELEMENTS = 1024;

const f32               T2_DEFAULT_BODY_RESTITUTION = 0.0f;
const f32               T2_DEFAULT_BODY_KIN_FRICTION = 0.2f;
const f32               T2_DEFAULT_BODY_STAT_FRICTION = 0.5f;

const f32               T2_CONTACT_ERPn = 0.2f;
const f32               T2_CONTACT_DEPTH_TOL = 0.02f;
const f32               T2_CONTACT_VEL_TOL = 5;          // 1

const unsigned int		T2_RENDER_AABB			= 0x1;
const unsigned int		T2_RENDER_FILL			= 0x2;
const unsigned int		T2_RENDER_JOINTS		= 0x4;
const unsigned int		T2_RENDER_CONTACTS		= 0x8;
const unsigned int		T2_RENDER_FORCES		= 0x10;
const unsigned int		T2_RENDER_NORMALS		= 0x20;
const unsigned int		T2_RENDER_LOCAL_SPACE	= 0x40;
const unsigned int		T2_RENDER_SLEEP_STATUS	= 0x80;



#endif /* CONFIG_H */
