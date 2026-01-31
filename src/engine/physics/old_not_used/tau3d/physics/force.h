
/*
 * Tau3D Dynamics 
 * Carl Johan Gribel (c) 2011, cjgribel@gmail.com
 *
 */

#pragma once
#ifndef force_t_H
#define force_t_H

//#include "tau3d.h"
#include "body.h"
#include "vec.h"
#include <vector>

using linalg::vec3f;
class body_t;

// Linear spring damper
//
class force_t
{
public:
	body_t	*bodyA, *bodyB;
	vec3f	rA, rB;
	f32		K, D, L;

	force_t(	body_t *bodyA, body_t *bodyB,
				vec3f rA, vec3f rB,
				float K, float D, float L);

	void applyForce();

	void render();
};

class field_F_t
{
    virtual void apply_force(std::vector<body_t*> bodies) const = 0;
};

/*
 component #1: perpendicular to radial vecor. strength increase inversely with dinstance to center (faster and faster spin)
 component #2: radial direction. constant strength?
 */
class vortex_F_t : public field_F_t
{
    vec3f center, up;
    // inverse distance coefficients
    // their ratio decides how aggressively the vortex spirals
    // note: very strong forces are not handles well by an explicit integrator
    float Crad = 750;
    float Cang = 100;
    float Cup = 100;
    
public:
    
    vortex_F_t(const vec3f& center, const vec3f& up) : center(center), up(up)
    {
    
    }
    
    void apply_force(std::vector<body_t*> bodies) const override
    {
        for (body_t* b : bodies)
        {
            vec3f bvec = b->X-center;
            vec3f bproj = up*(dot(up, bvec));
            vec3f brej = bvec-bproj;
            float brej_norm = brej.norm2();
            if (brej_norm < 1) brej_norm = 1; // avoid singularity-like circumstances in the middle
            float idist = (brej_norm<1e-4? 0 : 1.0f/brej_norm);
            
            // angular & radial components
            vec3f u = normalize(up%brej);
            vec3f v = normalize(brej);
            
            vec3f angular_F = u*idist*Cang;
            vec3f radial_F = -v*sqrtf(idist)*Crad;
            vec3f up_F = up*idist*idist*idist*Cup;
            
            b->apply_force(radial_F+angular_F+up_F, vec3f(0,0,0));
        }
    }
};

#endif /* force_t_H */
