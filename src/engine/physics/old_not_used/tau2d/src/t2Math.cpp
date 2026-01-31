
/*
	Tau2D Dynamics Engine
	(c) CJ Gribel 2008-2010, cjgribel@gmail.com
*/

#include "t2Math.h"
#include "t2Body.h"
#include "t2Geometry.h"

/*
I = moment of inertia for body
COM = center of mass of body
*/

float body_I(	t2Geometry* geoms[],
				int nbr_geoms,
				float density )
{
	/* I for body rel COM */
	float	_body_I		= 0.0f;
	/* COM for body */
	vec2f	_body_COM	= body_COM( geoms, nbr_geoms );

	/* accumulate I incrementally from geometries
	*/
	for( int i = 0; i < nbr_geoms; i++ )
	{
		/* entities for this geometry
		*/
				/* I rel its COM */
		float	_geom_I,
				/* geometry mass */
				_geom_mass;
				/* COM rel its offset */
		vec2f	_geom_COM,
				/* vector body COM -> geometry COM */
				_geom_dist;

		/* decide type of geometry & compute entitites
		*/
		if( geoms[i]->type == GEOMTYPE_POLY )
		{
			t2PolygonGeometry* p_geom = static_cast<t2PolygonGeometry*> ( geoms[i] );
			_geom_I		= poly_I( p_geom->vertices_local, p_geom->nbrVertices, density );
			_geom_mass	= poly_mass( p_geom->vertices_local, p_geom->nbrVertices, density );
			_geom_COM	= poly_COM( p_geom->vertices_local, p_geom->nbrVertices );
		}
		else
		{
			t2CircleGeometry* c_geom = static_cast<t2CircleGeometry*> ( geoms[i] );
			_geom_I		= circle_I( c_geom->radius, density );
			_geom_mass	= circle_mass( c_geom->radius, density );
			_geom_COM	= circle_COM();
		}
		_geom_dist = ( _geom_COM + geoms[i]->offset ) - _body_COM;

		/* add geometry I to body I using Steiner's rule
		*/
		_body_I += _geom_I + _geom_dist.normSquared() * _geom_mass;
	}
	return _body_I;
}

vec2f body_COM(t2Geometry* geoms[],
				int nbr_geoms )
{
	float	_body_area = 0.0f;
	vec2f	_body_COM(0.0f, 0.0f);

	if ( nbr_geoms == 0 ) return _body_COM;

	/* weigh in the COM of each geometry to the final COM
	*/
	for( int i = 0; i < nbr_geoms; i++ )
	{
		float	_geom_area;
		vec2f	_geom_COM;

		if( geoms[i]->type == GEOMTYPE_POLY )
		{
			t2PolygonGeometry* p_geom = static_cast<t2PolygonGeometry*> ( geoms[i] );
			_geom_area = poly_area( p_geom->vertices_local, p_geom->nbrVertices );
			_geom_COM = poly_COM( p_geom->vertices_local, p_geom->nbrVertices );
		}
		else
		{
			t2CircleGeometry* c_geom = static_cast<t2CircleGeometry*> ( geoms[i] );
			_geom_area = circle_area( c_geom->radius );
			_geom_COM = circle_COM();
		}
		_body_COM += ( _geom_COM + geoms[i]->offset ) * _geom_area;
		_body_area += _geom_area;
	}
	_body_COM /= _body_area;

	return _body_COM;
}

float body_mass(t2Geometry* geoms[],
				int nbr_geoms,
				float density)
{
	return density * body_area(geoms, nbr_geoms);
}

float body_area(t2Geometry* geoms[],
				int nbr_geoms)
{
	float _body_area = 0.0f;
	
	for( int i = 0; i < nbr_geoms; i++ )
	{
		if( geoms[i]->type == GEOMTYPE_POLY )
		{
			t2PolygonGeometry* p_geom = static_cast<t2PolygonGeometry*> (geoms[i]);
			_body_area += poly_area(p_geom->vertices_local, p_geom->nbrVertices);
		}
		else
		{
			t2CircleGeometry* c_geom = static_cast<t2CircleGeometry*> (geoms[i]);
			_body_area += circle_area(c_geom->radius);
		}
	}
	return _body_area;
}

/*
	moment of inertia at center of mass for polygon (assume uniform weight distribution)
	
	polygon I: calc I for all sub-triangles, add together using Steiner's rule: I = I_g + mass*d^2,
	where I_g is I at COM for respective triangle, and d is the vector triangle COM -> polygon COM
	hence: poly_I = tri#1_I + tri#1_mass*tri#1_dist^2 + ... + tri#n_I + tri#n_mass*tri#n_dist^2
*/
float poly_I(vec2f vertices[],
			 int nbr_vertices,
			 float density)
{
			/* I for entire polygon */
	float	_poly_I = 0.0f, 
			/* I for sub-triangle */
			_tri_I,
			/* mass of sub-triangle */
			_tri_mass;			
			/* COM for polygon */
	vec2f	_poly_COM = poly_COM(vertices, nbr_vertices),
			/* COM for sub-triangle */
			_tri_COM,
			/* vector from sub-triangle COM to polygon COM */
			_tri_dist;

	/* accumulate I from each sub-triangle
	*/
	for( int i = 1; i < nbr_vertices - 1; i++ )
	{
		_tri_I = triangle_I(vertices[0], vertices[i], vertices[i+1], density);
		_tri_mass = triangle_mass(vertices[0], vertices[i], vertices[i+1], density);
		_tri_COM = triangle_COM(vertices[0], vertices[i], vertices[i+1]);
		_tri_dist = _tri_COM - _poly_COM;

		_poly_I += _tri_I + _tri_dist.normSquared() * _tri_mass; // density*tri_area;
	}
	return _poly_I;
}

/*
	center of mass (COM) for polygon (assume uniform weight distribution)

	poly_COM = (tri#1_area*tri#1_COM + ... + tri#n_area*tri#n_COM) / (tri#1_area + ... + tri#n_area)
*/
vec2f poly_COM(vec2f vertices[],
				int nbr_vertices)
{
	float	_poly_area = 0.0f,
			_tri_area;
	vec2f	_poly_COM(0.0f, 0.0f),
			_tri_COM;

	/* weigh in the COM of each sub-triangle to the final COM
	*/
	for( int i = 1; i < nbr_vertices - 1; i++ )
	{
		_tri_area = triangle_area(vertices[0], vertices[i], vertices[i+1]);
		_tri_COM = triangle_COM(vertices[0], vertices[i], vertices[i+1]);

		_poly_COM += _tri_COM * _tri_area;
		_poly_area += _tri_area;
	}
	_poly_COM /= _poly_area;

	return _poly_COM;
}

float poly_mass(vec2f vecs[],
				int nbr_vertices,
				float density)
{
	return density * poly_area(vecs, nbr_vertices);
}

/*
	area of polygon
*/
float poly_area(vec2f vertices[],
				int nbr_vertices)
{
	float _poly_area = 0.0f;
	
	for(int i = 1; i < nbr_vertices - 1; i++)
		_poly_area += triangle_area(vertices[0], vertices[i], vertices[i+1]);

	return _poly_area;
}

float circle_I(float radius,
			   float density)
{
	return circle_mass(radius, density) / 2.0f * radius*radius;
}

vec2f circle_COM()
{
	return vec2f();
}

float circle_mass(	float radius,
					float density)
{
	return density * circle_area(radius);
}

float circle_area(	float radius)
{
	return PI * radius * radius;
}

/*
	moment of inertia at center of mass for triangle (assume uniform weight distribution)

	tri_I = tri_mass/18 * ( (v2-v1)^2 + (v3-v1)^2 - dot((v2-v1),(v3-v1)) )
*/
float triangle_I(vec2f v1,
				 vec2f v2,
				 vec2f v3,
				 float density)
{
	vec2f	_vA = v2 - v1,
			_vB = v3 - v1;
	float	_tri_mass = triangle_mass(v1, v2, v3, density);

	return _tri_mass / 18.0f * (_vA.normSquared() + _vB.normSquared() - vec2f::dot(_vA, _vB));
}

/*
	center of mass for triangle (assume uniform weight distribution)
*/
vec2f triangle_COM(vec2f v1,
					vec2f v2,
					vec2f v3)
{
	return (v1 + v2 + v3) / 3.0f;
}

/*
	triangle mass
*/
float triangle_mass(vec2f v1,
					vec2f v2,
					vec2f v3,
					float density)
{
	return density * triangle_area(v1, v2, v3);
}

/*
	triangle area
*/
float triangle_area(vec2f v1,
					vec2f v2,
					vec2f v3)
{
	return vec2f::cross(v2 - v1, v3 - v1) * 0.5f;
}


/*
	concave polygon triangulation
*/

/*
	check if vectors are CCW
*/
bool isCCW( vec2f v1, vec2f v2 )
{
	return vec2f::cross( v1, v2 ) > 0.0f;
}

/* check if polygon is CCW */
bool isCCW( vec2f v[], int vc )
{
	float rsum = 0.0f;
	for( int i = 0; i < vc; i++ )
	{
		int j = (i+1) % vc,
			k = (i+2) % vc;
		vec2f	vi = v[i],
				vj = v[j],
				vk = v[k],
				vij = vj - vi,
				vjk = vk - vj;
		float	sign = ( vec2f::cross( vij, vjk ) > 0.0f ? 1.0f : -1.0f ),
				angle = vec2f::getAngle( vij, vjk );
		rsum += sign * angle;
	}
	return rsum > 0.0f;
}

/*
	generate set of convex geometries from a (potentially) concave polygon
*/
void triangulate_polygon( vec2f v_source[], const int &vc_source, t2Body *body )
{
	/* make a copy of the vertex array
	*/
	int vc = vc_source;
	vec2f *v = new vec2f[vc_source];
	for (int i = 0; i < vc; i++)	v[i] = v_source[i];

	if (vc < 3) return;
	
	/* if vertices are in CCw order - reverse array
	*/
	if( !isCCW( v, vc ) )
	{
		for( int i = 0; i < vc/2; i++ )
		{
			int j = vc-1-i;
			vec2f vtmp = v[i];
			v[i] = v[j];
			v[j] = vtmp;
		}
	}

	/* triangulate
	*/
	int	i = 0,
		abort_counter = 0;
	while ( vc > 2 )
	{
		int j = (i+1) % vc,
			k = (i+2) % vc;
		vec2f	vi = v[i],
				vj = v[j],
				vk = v[k],
				vij = vj - vi,
				vik = vk - vi;

		/* if left turn this is a potential triangle
		*/
		if ( isCCW( vij, vik ) )
		{
			/* check if other vertices are inside this triangle
			*/
			bool outside = true;
			vec2f	vji = -vij,
					vkj = vj - vk;
			for (int si = 1; si < vc-2; si++ )
			{
				int s = (k+si) % vc;
				outside &=	isCCW( vik, v[s] - vi ) ||
							isCCW( vji, v[s] - vj ) ||
							isCCW( vkj, v[s] - vk );
			}
			if ( outside )
			{
				/* create triangle from i,j,k
				*/
				t2PolygonGeometry *poly_geom = new t2PolygonGeometry();
				poly_geom->addVertex( vi );
				poly_geom->addVertex( vj );
				poly_geom->addVertex( vk );
				body->addGeometry( poly_geom );

				/* remove j:th vertex
				*/
				for ( int s = j ; s < vc-1; s++ )	v[s] = v[s+1];
				vc--;

				i = (i+1) % vc;
			}
			else
				i = j;
		}
		else
			i = j;

		abort_counter++;
		if ( abort_counter > 10.0f * vc_source )
		{
			printf("TRIANGULATION FAILED\n");
			return;
		}

	}
	delete[] v;
}
