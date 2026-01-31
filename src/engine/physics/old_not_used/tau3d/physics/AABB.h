//
//  aabb.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2015-01-20.
//
//

#ifndef tau3d_aabb_h
#define tau3d_aabb_h

#include "vec.h"
#include "ray.h"
#include <float.h>

using linalg::vec3f;

/*
 OPT: world AABB from local AABB + rotation: Ericson p86
 
 */

/*
 struct AABB
 {
	float x_min, x_max, y_min, y_max;
 
	AABB(void)
	{ }
 
	void reset(vec2f v)
	{
 x_min = v.x;
 x_max = v.x;
 y_min = v.y;
 y_max = v.y;
	}
 
	void append(vec2f &v) { append(v.x, v.y); }
 
	void append(float x, float y)
	{
 x_min = minf(x_min, x);
 x_max = maxf(x_max, x);
 y_min = minf(y_min, y);
 y_max = maxf(y_max, y);
	}
 
	bool overlap(AABB &aabb)
	{
 return	x_min <	aabb.x_max		&&
 x_max >= aabb.x_min		&&
 y_min < aabb.y_max		&&
 y_max >= aabb.y_min;
	}
 };
 */

struct AABB_t
{
    union
    {
        vec3f vmin;
        float min[3];   // skip and use vmin.vec[]
    };
    union
    {
        vec3f vmax;
        float max[3];
    };
    
    AABB_t(float* min, float* max)
    {
        std::copy(this->min, this->min+3, min);
        std::copy(this->max, this->max+3, max);
    }
    
    AABB_t()
    {
        reset();
    }
    
    inline void reset()
    {
        min[0] = min[1] = min[2] = std::numeric_limits<float>::infinity();
        max[0] = max[1] = max[2] = -std::numeric_limits<float>::infinity();
    }
    
    inline void grow(const vec3f &p)
    {
        grow(p.vec);
    }
    
    inline void grow(const float p[3])
    {
        for (int i=0; i<3; i++)
        {
            min[i] = std::fminf(min[i], p[i]);
            max[i] = std::fmaxf(max[i], p[i]);
        }
    }
    
    inline void grow(const AABB_t& aabb)
    {
        grow(aabb.min);
        grow(aabb.max);
    }
    
    inline AABB_t operator+ (const vec3f& v)
    {
        AABB_t aabb = *this;
        aabb.vmin +=  v;
        aabb.vmax +=  v;
        
        return aabb;
    }
    
    //
    // AABB resulting from this AABB being rotated and translated.
    //
    // Ericsson, Real-time Collision Detection, page 86.
    // -However had to change index order (i<->j) in the matrix element extraction,
    // perhaps because Ericson assumes row-odered matrix elements in his code.
    // With this change the "axes" in the outer loop correspond to columns of
    // the rotation matrix, as intended per the decription in the book.
    //
    inline AABB_t post_transform(const linalg::vec3f& T, const linalg::mat3f& R)
    {
        AABB_t aabb;
        
        // For all three axes
        for (int i = 0; i < 3; i++) {
            // Start by adding in translation
            aabb.min[i] = aabb.max[i] = T.vec[i];
            // Form extent by summing smaller and larger terms respectively
            for (int j = 0; j < 3; j++) {
                float e = R.mat[j][i] * min[j];
                float f = R.mat[j][i] * max[j];
                //            float e = R.mat[i][j] * aabb.min[j];
                //            float f = R.mat[i][j] * aabb.max[j];
                if (e < f) {
                    aabb.min[i] += e;
                    aabb.max[i] += f;
                } else {
                    aabb.min[i] += f;
                    aabb.max[i] += e;
                }
            }
        }
        
        return aabb;
    }
    
    // Can it be assumed that M can be factorized into R and T this way?
//    inline AABB_t post_transform(const linalg::mat4f& M)
//    {
//        return post_transform_AABB(M.col[3].xyz(), M.get_3x3());
//    }
    
#define AABB_EPS FLT_EPSILON
    
    inline bool intersect(const AABB_t &aabb) const
    {
#ifdef MESHCOLLIDERTEST
        return true; // !
#endif
        
        if (max[0] < aabb.min[0] - AABB_EPS || min[0] > aabb.max[0] + AABB_EPS) return false;
        if (max[1] < aabb.min[1] - AABB_EPS || min[1] > aabb.max[1] + AABB_EPS) return false;
        if (max[2] < aabb.min[2] - AABB_EPS || min[2] > aabb.max[2] + AABB_EPS) return false;
        return true;
    }
    
//    bool intersect_sphere(const linalg::vec3f& p, float r)
//    {
//        return true;
//    }
    
    // see RayAABB.cpp by Terdiman
    //
    bool intersect(const ray_t& ray) const
    {
        return false;
    }
    
private:
    
    //
    // Split AABB at frac ∈ [0,1] in the dim:th dimension
    //
    void split2(int dim, float frac, AABB_t& aabb_left, AABB_t& aabb_right)
    {
        aabb_left = *this;
        aabb_right = *this;
        float halfx = min[dim] + (max[dim]-min[dim])*frac;
        
        aabb_left.max[dim] = halfx;
        aabb_right.min[dim] = halfx;
    }
    
public:
    
    //
    // Split AABB into four in the xz-plane
    //
    void split4_xz(AABB_t aabb[4])
    {
        AABB_t l, r;
        split2(0, 0.5f, l, r);               // split in x
        l.split2(2, 0.5f,aabb[0], aabb[1]);  // split in z
        r.split2(2, 0.5f, aabb[2], aabb[3]); // split in z
    }
};

#endif
