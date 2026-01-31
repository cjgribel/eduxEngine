
/*
 * Tau2D Rigid Body Dynamics 
 * CJ Gribel (c) 2008-2009, cjgribel@gmail.com
 *
 */

// inclusion guard
#pragma once
#ifndef T2MATH_H
#define T2MATH_H

#include <cmath>
//#include "t2Body.h"

#define PI			3.141592653f
#define INF			3.4028234e38
#define NEG_INF		-3.4028234e38
#define DEG_TO_RAD	PI / 180.0f
#define RAD_TO_DEG	180.0f / PI

/*
	2d-vector
*/
struct vec2f
{

	float x, y;

	vec2f()					: x(0.0f), y(0.0f) { }

	vec2f(float x, float y)	: x(x), y(y) { }

	vec2f(const vec2f& vec)	: x(vec.x), y(vec.y) { }

	void set(float x, float y)
	{
		this->x = x;
		this->y = y;
	}

	float dot(const vec2f &v)
	{
		return x*v.x + y*v.y;
	}

	static float dot(const vec2f &v1, const vec2f &v2)
	{
		return v1.x * v2.x + v1.y * v2.y;
	}

	// vector x vector = scalar
	static float cross(const vec2f &v1, const vec2f &v2)
	{
		return v1.x * v2.y - v1.y * v2.x;
	}

	// vector x scalar = vector
	static vec2f cross(const vec2f& v, float s)
	{
		return vec2f(s * v.y, -s * v.x);
	}

	// scalar x vector = vector
	static vec2f cross(float s, vec2f& v)
	{
		return vec2f(-s * v.y, s * v.x);
	}

	// normal: v = v2-v1, n = (v.x,v.y,0)x(0,0,1) = (v.y,-v.x,0)
	static vec2f getNormal(const vec2f &v1, const vec2f &v2)
	{
		return vec2f(v2.y - v1.y, v1.x - v2.x);
	}

	// project v1 on v2
	static vec2f projection(vec2f &v1, vec2f &v2)
	{
		return v2 * (vec2f::dot(v1, v2) / v2.normSquared());
	}

	static float getNorm(vec2f v)
	{
		return v.norm();
	}

	static float getAngle(vec2f &v1, vec2f &v2)
	{
		vec2f	vA = vec2f(v1).normalize(),
				vB = vec2f(v2).normalize();
		return acos( vec2f::dot( vA, vB ) );
	}

	vec2f& normalize()
	{
		float lenSq = x*x + y*y;

		if( lenSq == 0.0f )
			set(0.0f, 0.0f);
		else
		{
			float ilen = 1.0f / sqrt(lenSq);
			set(x * ilen, y * ilen);
		}
		return *this;

		//float length_inv = 1.0f / sqrt(x*x + y*y);
		//set(x * length_inv, y * length_inv);
		//return *this;
	}

	static vec2f& normalize(vec2f v)
	{
		return v.normalize();
	}

	float norm()
	{
		return sqrt(x*x + y*y);
	}

	float normSquared()
	{
		return x*x + y*y;
	}

	/*
		assignment operator
	*/
	vec2f& operator=(const vec2f &rhs)
	{
		x = rhs.x;
		y = rhs.y;
		return *this;
	}

	/*
		compound assignment operators:
	*/

	vec2f& operator+=(const vec2f &rhs)
	{
		x += rhs.x;
		y += rhs.y;
		return *this;
	}

	vec2f& operator-=(const vec2f &rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}

	vec2f& operator*=(const float &rhs)
	{
		x *= rhs;
		y *= rhs;
		return *this;
	}

	vec2f& operator*=(const vec2f &rhs)
	{
		x *= rhs.x;
		y *= rhs.y;
		return *this;
	}

	vec2f& operator/=(const float &rhs)
	{
		x /= rhs;
		y /= rhs;
		return *this;
	}

	/*
		unary arithmetic operator
	*/
	vec2f operator-()
	{
		return vec2f(-x, -y);
	}

	/*
		binary arithmetic operators:
	*/

	// vector * scalar = vector
	vec2f operator*(const float &rhs)
	{
		return vec2f(x * rhs, y * rhs);
	}

	// vector * vector = vector
	vec2f operator*(const vec2f &rhs)
	{
		return vec2f(x * rhs.x, y * rhs.y);
	}

	// vector / scalar = vector
	vec2f operator/(const float &rhs)
	{
		float irhs = 1.0f / rhs;
		return vec2f(x * irhs, y * irhs);
	}

	vec2f operator+(const vec2f &rhs)
	{
		return vec2f(x + rhs.x, y + rhs.y);
		//return vec2f(*this) += rhs;
	}

	vec2f operator-(const vec2f &rhs)
	{
		return vec2f(x - rhs.x, y - rhs.y);
	}

	float operator%(const vec2f &rhs)
	{
		return x * rhs.y - y * rhs.x;
	}

};

//vec2f operator*(const vec2f &lhs, const float &rhs){
//	return vec2f(lhs.x * rhs, lhs.y * rhs);
//}

/*
	2d matrix
	| m11 m12 | 
	| m21 m22 |
*/
struct mat2
{
	float m11, m12, m21, m22;

	mat2() { }

	mat2(float m11, float m12, float m21, float m22)
		: m11(m11), m12(m12), m21(m21), m22(m22)
	{ }

	mat2(float rad)
	{
		float	c = cos(rad),
				s = sin(rad);
		m11 = c; m12 = -s;
		m21 = s; m22 = c;
	}

	mat2(float scale_x, float scale_y)
	{
		m11 = scale_x;	m12 = 0.0f;
		m21 = 0.0f;		m22 = scale_y;
	}

	/*
		invert
		TODO: test for invertability
	*/
	mat2 invert() const
	{
		float det = m11 * m22 - m12 * m21;
		//if(det == 0.0f)
		return mat2(m22, -m21, -m12, m11) * (1.0f / det);
	}

	/*
		unary arithmetic operator
	*/
	mat2 operator-()
	{
		return mat2(-m11, -m12, -m21, -m22);
	}

	/*
		binary arithmetic operators:
	*/

	// matrix * scalar = matrix
	mat2 operator*(const float s) const
	{
		return mat2(m11 * s, m12 * s, m21 * s, m22* s);
	}

	// matrix * vector = vector
	vec2f operator*(const vec2f &rhs) const
	{
		return vec2f(m11 * rhs.x + m12 * rhs.y, m21 * rhs.x + m22 * rhs.y);
	}

};

/*
	min
*/
inline float minf(float a, float b)
{
	return a < b ? a : b;
}

/*
	max
*/
inline float maxf(float a, float b)
{
	return a > b ? a : b;
}

/*
	clamp
*/
inline float clampf(float a, float min, float max)
{
	return maxf(min, minf(max, a));
}

/*
	radians < - > degrees
*/
inline float deg_to_rad(float deg)
{
	return deg * DEG_TO_RAD;
}

inline float rad_to_deg(float rad)
{
	return rad * RAD_TO_DEG;
}

/*
	AABB
*/
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

/*
	ContactPoint
	NOTE: no longer in use
*/
struct t2dContactPoint
{
	vec2f cp, cn;		// contact point / normal
	float depth;		// penetration depth
	int ei;				// intersected edge index

	t2dContactPoint() : cp(), cn(), depth(0.0f), ei() {}
};

/*
	PocData
	NOTE: no longer in use
*/
struct PocData {
	vec2f poc, noc, penetA, penetB;
	PocData() : poc(), noc(), penetA(), penetB() {}
};

/*
	mass, area, center of mass (COM) and
	moment of inertia (I) computations
	for various primitives
*/

/* body operations (set of polygons and/or circles) */

class t2Geometry;

float body_I(		t2Geometry* geoms[],
					int nbr_geoms,
					float density);

vec2f body_COM(	t2Geometry* geoms[],
					int nbr_geoms);

float body_mass(	t2Geometry* geoms[],
					int nbr_geoms,
					float density);

float body_area(	t2Geometry* geoms[],
					int nbr_geoms);

/* circle operations */

float circle_I(		float radius,
					float density);

vec2f circle_COM(	);

float circle_mass(	float radius,
					float density);

float circle_area(	float radius);

/* polygon operations */

float poly_I(		vec2f vertices[],
					int nbr_vertices,
					float density);

vec2f poly_COM(	vec2f vecs[],
					int nbr_vertices);

float poly_mass(	vec2f vecs[],
					int nbr_vertices,
					float density);

float poly_area(	vec2f vertices[],
					int nbr_vertices);

/* triangle operations */

float triangle_I(	vec2f v1,
					vec2f v2,
					vec2f v3,
					float density);

vec2f triangle_COM(vec2f v1,
					vec2f v2,
					vec2f v3);

float triangle_mass(vec2f v1,
					vec2f v2,
					vec2f v3,
					float density);

float triangle_area(vec2f v1,
					vec2f v2,
					vec2f v3);

/*
	generate set of convex geometries from a (potentially) concave polygon
*/
class t2Body;

void triangulate_polygon( vec2f v_source[], const int &vc_source, t2Body *body );

#endif /* T2MATH_H */
