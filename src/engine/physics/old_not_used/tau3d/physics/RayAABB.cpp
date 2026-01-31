
#if 0

// source: http://www.codercorner.com/RayAABB.cpp

typedef unsigned int udword;

//! Integer representation of a floating-point value.
#define IR(x)	((udword&)x)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	A method to compute a ray-AABB intersection.
 *	Original code by Andrew Woo, from "Graphics Gems", Academic Press, 1990
 *	Optimized code by Pierre Terdiman, 2000 (~20-30% faster on my Celeron 500)
 *	Epsilon value added by Klaus Hartmann. (discarding it saves a few cycles only)
 *
 *	Hence this version is faster as well as more robust than the original one.
 *
 *	Should work provided:
 *	1) the integer representation of 0.0f is 0x00000000
 *	2) the sign bit of the float is the most significant one
 *
 *	Report bugs: p.terdiman@codercorner.com
 *
 *	\param		aabb		[in] the axis-aligned bounding box
 *	\param		origin		[in] ray origin
 *	\param		dir			[in] ray direction
 *	\param		coord		[out] impact coordinates
 *	\return		true if ray intersects AABB
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define RAYAABB_EPSILON 0.00001f
bool RayAABB(const AABB& aabb, const Point& origin, const Point& dir, Point& coord)
{
	BOOL Inside = TRUE;
	Point MinB = aabb.mCenter - aabb.mExtents;
	Point MaxB = aabb.mCenter + aabb.mExtents;
	Point MaxT;
	MaxT.x=MaxT.y=MaxT.z=-1.0f;

	// Find candidate planes.
	for(udword i=0;i<3;i++)
	{
		if(origin.m[i] < MinB.m[i])
		{
			coord.m[i]	= MinB.m[i];
			Inside		= FALSE;

			// Calculate T distances to candidate planes
			if(IR(dir.m[i]))	MaxT.m[i] = (MinB.m[i] - origin.m[i]) / dir.m[i];
		}
		else if(origin.m[i] > MaxB.m[i])
		{
			coord.m[i]	= MaxB.m[i];
			Inside		= FALSE;

			// Calculate T distances to candidate planes
			if(IR(dir.m[i]))	MaxT.m[i] = (MaxB.m[i] - origin.m[i]) / dir.m[i];
		}
	}

	// Ray origin inside bounding box
	if(Inside)
	{
		coord = origin;
		return true;
	}

	// Get largest of the maxT's for final choice of intersection
	udword WhichPlane = 0;
	if(MaxT.m[1] > MaxT.m[WhichPlane])	WhichPlane = 1;
	if(MaxT.m[2] > MaxT.m[WhichPlane])	WhichPlane = 2;

	// Check final candidate actually inside box
	if(IR(MaxT.m[WhichPlane])&0x80000000) return false;

	for(i=0;i<3;i++)
	{
		if(i!=WhichPlane)
		{
			coord.m[i] = origin.m[i] + MaxT.m[WhichPlane] * dir.m[i];
#ifdef RAYAABB_EPSILON
			if(coord.m[i] < MinB.m[i] - RAYAABB_EPSILON || coord.m[i] > MaxB.m[i] + RAYAABB_EPSILON)	return false;
#else
			if(coord.m[i] < MinB.m[i] || coord.m[i] > MaxB.m[i])	return false;
#endif
		}
	}
	return true;	// ray hits box
}

#endif