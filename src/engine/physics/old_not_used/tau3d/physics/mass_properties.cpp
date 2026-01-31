//
//  mass_properties.cpp
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-11-29.
//
//

#include "mass_properties.h"
#include "collider.h"

/* 
 * TODO: evaluate
 * compute internal point of a convex polyhedron
 *
 * this point may or may not be equal to the center of mass,
 * depending on the vertex distribution
 */
vec3f compute_Steiner_point(poly_collider_t *pg)
{
    vec3f vsum = vec3f_zero;

    for (vec3f& v : pg->vertices)
        vsum += v;
    
    vsum *= (1.0f/pg->vertices.size());
    
    return vsum;
}

/*
 * Moment of inertia (I) wrt to a reference point R
 *
 * Ig:     I of body at its center of mass
 * mass:   mass of body
 * R:      reference point
 * returns
 * J:      I of body wrt reference point R
 *
 *   J[i,j] = Ig[i,j] + m( |R|^2 kronecker[i,j] - R[i]R[j] )
 *   J = Ig + m [ (R.R)E_3x3 - R outer prod R ]
 *
 * http://en.wikipedia.org/wiki/Parallel_axis_theorem#Tensor_generalization
 * http://www.bulletphysics.org/Bullet/phpBB3/viewtopic.php?f=4&t=3702
 *
 */
mat3f I_Steiner(const mat3f &Ig, float m, const vec3f &R)
{
    return Ig + (mat3f_identity*R.dot(R) - R.outer_product(R)) * m;
}

/*
 * Tetrahedron volume
 *
 * V = det| a-d b-d c-d | / 6 = ( ((a-d) x (b-d)).(c-d) ) / 6
 *
 * http://en.wikipedia.org/wiki/Tetrahedron#Volume
 *
 */
float tetrahedron_V(vec3f &a, vec3f &b, vec3f &c, vec3f &d)
{
    return fabs(((a-d) % (b-d)).dot(c-d))/6.0f;
}

/*
 * Tetrahedron center of mass = its centroid
 *
 * http://en.wikipedia.org/wiki/Centroid#Of_triangle_and_tetrahedron
 * http://www.globalspec.com/reference/52702/203279/4-8-the-centroid-of-a-tetrahedron
 *
 */
vec3f tetrahedron_COM(vec3f &a, vec3f &b, vec3f &c, vec3f &d)
{
    return vec3f(a.x+b.x+c.x+d.x, a.y+b.y+c.y+d.y, a.z+b.z+c.z+d.z)/4.0f;
}

/*
 * Tetrahedron mass properties
 *
 * Tetrahedron = {a, b, c, d} in local space
 * Volume V = |a-d b-d c-d|/6 = ((a-d)xb-d).(c-d)) -> |a b c| = det(a,b,c) = 6V
 * http://en.wikipedia.org/wiki/Tetrahedron#Volume
 *
 * Aggregate center of mass
 * http://en.wikipedia.org/wiki/Center_of_mass#A_system_of_particles
 *
 * Moment of inertia tensor (I)
 *
 *     |   a   -b' -c' |
 * J = |   -b' b   -a' |
 *     |   -c' -a' c   |
 *
 * "Explicit Exact Formulas for the 3-D Tetrahedron Inertia Tensor in Terms of its Vertex Coordinates"
 * http://thescipub.com/html/10.3844/jmssp.2005.8.11
 *
 * Theoretical derivation:
 * "How to find the inertia tensor (or other mass properties) of a 3D solid body represented by a triangle mesh"
 * http://number-none.com/blow/inertia/bb_inertia.doc
 *
 */
static mass_properties_t tetrahedron_mass_properties(vec3f &p, vec3f &q, vec3f &r, vec3f &s, float rho)
{
    // volume
    float th_V = tetrahedron_V(p,q,r,s);
    
    // mass
    float th_m = th_V*rho;
    
    // center of mass (centroid)
    vec3f th_com = tetrahedron_COM(p,q,r,s);
    
    // inertia tensor
    
    // vertices (p,q,r,s) in center-of-mass space
    // (p,q,r,s) are expressed in local geometry space, which is arbitrary
    vec3f a = p - th_com;
    vec3f b = q - th_com;
    vec3f c = r - th_com;
    vec3f d = s - th_com;
    
    float J_det = th_V*6.0f;
    float Ja, Jb, Jc, Jap, Jbp, Jcp;
    
    float Jabc_x = a.x*a.x + a.x*b.x + b.x*b.x + a.x*c.x + b.x*c.x + c.x*c.x + a.x*d.x + b.x*d.x + c.x*d.x + d.x*d.x;
    float Jabc_y = a.y*a.y + a.y*b.y + b.y*b.y + a.y*c.y + b.y*c.y + c.y*c.y + a.y*d.y + b.y*d.y + c.y*d.y + d.y*d.y;
    float Jabc_z = a.z*a.z + a.z*b.z + b.z*b.z + a.z*c.z + b.z*c.z + c.z*c.z + a.z*d.z + b.z*d.z + c.z*d.z + d.z*d.z;
    
    Ja = (Jabc_y + Jabc_z)/60.0f;
    Jb = (Jabc_x + Jabc_z)/60.0f;
    Jc = (Jabc_x + Jabc_y)/60.0f;
    
    Jap = ((2.0f*a.y + b.y + c.y + d.y)*a.z +
           (a.y + 2.0f*b.y + c.y + d.y)*b.z +
           (a.y + b.y + 2.0f*c.y + d.y)*c.z +
           (a.y + b.y + c.y + 2.0f*d.y)*d.z)/120.0f;
    
    Jbp = ((2.0f*a.x + b.x + c.x + d.x)*a.z +
           (a.x + 2.0f*b.x + c.x + d.x)*b.z +
           (a.x + b.x + 2.0f*c.x + d.x)*c.z +
           (a.x + b.x + c.x + 2.0f*d.x)*d.z)/120.0f;
    
    Jcp = ((2.0f*a.x + b.x + c.x + d.x)*a.y +
           (a.x + 2.0f*b.x + c.x + d.x)*b.y +
           (a.x + b.x + 2.0f*c.x + d.x)*c.y +
           (a.x + b.x + c.x + 2.0f*d.x)*d.y)/120.0f;
    
    mat3f th_J = mat3f(Ja, -Jbp, -Jcp, -Jbp, Jb, -Jap, -Jcp, -Jap, Jc) * (rho * fabs(J_det));
    
    return { th_m, th_J, th_com };
}

/*
 * Convex polyhedra mass properties
 *
 * Method:
 * 1. Tetrahedralize wrt to a Steiner point (= local origin) (tetrahedralization is always possible for convex polyhedra [Ericson04])
 * 2. Compute attribs for each tetrahedra
 * 3. Compute center-of-mass for the entire polyhedra
 * 4. Compute inertia tensor for the entire polyhedra
 */
mass_properties_t polyhedron_mass_properties(poly_collider_t *pg, float rho)
{
    float poly_m = 0;
    vec3f poly_com = vec3f(0,0,0); //  Any point, does not need to be inide polyhedron. Do we call this a Steiner point?
    mat3f poly_I = mat3f(0);
    std::vector<mass_properties_t> th_mprops;
    
    // create tetrahedrons in local geometry space and compute their attributes
    vec3f p = vec3f(0,0,0); // Steiner point = local origin, assumed to be inside polyhedra
    int pcount = 0;
    for (int i=0; i<pg->nbr_faces; i++)
        for (int j=1; j<pg->face_stride-1; j++)
        {
            vec3f q = pg->vertices[ pg->faces[i*pg->face_stride + 0]    ];
            vec3f r = pg->vertices[ pg->faces[i*pg->face_stride + j]    ];
            vec3f s = pg->vertices[ pg->faces[i*pg->face_stride + j+1]  ];
            
            th_mprops.push_back( tetrahedron_mass_properties(p, q, r, s, rho) );
            pcount++;
        }
    
    // aggregate COM & mass
    for (mass_properties_t& th_mprop : th_mprops)
    {
        poly_m += th_mprop.m;
        poly_com += th_mprop.com * th_mprop.m;
    }
    poly_com /= poly_m;
    
    // aggregate I
    for (mass_properties_t& th_mprop : th_mprops)
    {
        // offset from polyhedron COM -> tetrahedron COM
        vec3f th_R = -(th_mprop.com - poly_com);
        // accumulate polyhedron tensor
        poly_I += I_Steiner(th_mprop.I, th_mprop.m, th_R);
    }
    
#if 0
    // Symmetry estimate
    mat3f m = transpose(poly_I);
    float asym = 0;
    for (int i=0; i<9; i++) asym += fabsf(poly_I.array[i] - m.array[i]);
    if (asym > 1.0e-4)
    {
        std::cout << poly_I;
        std::cout << asym << "\n";
    }
#endif
    
    return { poly_m, poly_I, poly_com };
}

/*
 * Sphere mass properties
 *
 * mass = rho * volume = rho * 4PIr^3/3
 * moment of inertia, I = 2mr^2/5 * 3x3_identity
 */
mass_properties_t sphere_mass_properties(sphere_collider_t *sg, float rho)
{
    float r2 = sg->r*sg->r, r3 = r2*sg->r;
    float m = rho*4*fPI*r3/3;
    
    mat3f I = mat3f(0);
    I.m11 = I.m22 = I.m33 = 2.0f*m*r2/5.0f;
    
    return { m, I, {0,0,0} };
}

/*
 * Point mass properties
 * Since a point mass has zero volume: assume rho is passed on as desired mass,
 * and model MOI after a sphere.
 *
 * mass = rho
 * moment of inertia, I = 2m/5 * 3x3_identity
 */
mass_properties_t point_mass_properties(point_collider_t *sg, float rho)
{
    float m = rho;
    
    mat3f I = mat3f(0);
    I.m11 = I.m22 = I.m33 = 2.0f*m/5.0f;
    
    return { m, I, {0,0,0} };
}

/*
 * Collider mass properties
 */
mass_properties_t collider_mass_properties(collider_t *g, float rho)
{
    if (g->gtype == POLYHEDRON)
    {
        poly_collider_t *pg = static_cast<poly_collider_t*>(g);
        return polyhedron_mass_properties(pg, rho);
    }
    else if (g->gtype == SPHERE)
    {
        sphere_collider_t *sg = static_cast<sphere_collider_t*>(g);
        return sphere_mass_properties(sg, rho);
    }
    else if (g->gtype == PLANE)
    {
        printf("warning: plane_collider_t attribs should be set explicitly\n");
        
        return {std::numeric_limits<float>::infinity(), mat3f(std::numeric_limits<float>::infinity()), {0,0,0} };
    }
    else if (g->gtype == POINT)
    {
        point_collider_t *pc = static_cast<point_collider_t*>(g);
        return point_mass_properties(pc, rho);
    }
    else
        throw runtime_error("No valid collider type for collider_mass_properties\n");
}

/*
 * Rigid Body mass properties (set of colliders)
 */
mass_properties_t body_mass_properties(std::vector<collider_t*> &geometries, float rho)
{
    float gs_m = 0;
    mat3f gs_I = mat3f(0);
    vec3f gs_com = vec3f(0,0,0);
    std::vector<mass_properties_t> g_mprops;   // collider mass properties
    std::vector<vec3f> g_Rg;                    // collider offsets
    
    // compute collider mass properties and local offsets
    for (collider_t *g : geometries)
    {
        g_mprops.push_back( collider_mass_properties(g, rho) );
        g_Rg.push_back( g->X );
    }
    
    // aggregate mass & com
    for (int i=0; i<g_mprops.size(); i++)
    {
        gs_m += g_mprops[i].m;
        gs_com += (g_mprops[i].com + g_Rg[i]) * g_mprops[i].m;
    }
    gs_com /= gs_m;
    
    // aggeregate I
    for (int i=0; i<g_mprops.size(); i++)
    {
        // distance from aggregate COM -> geometry COM + geometry offset
        vec3f g_R = -((g_mprops[i].com + g_Rg[i]) - gs_com);
        
        gs_I += I_Steiner(g_mprops[i].I, g_mprops[i].m, g_R);
    }
    
    // diagonalize J and create a final tensor from its diagonal elements
    //    mat3 rot;
    //    gs_I.diagonalize(rot, 0.00001, 20);
    //    gs_I = mat3(gs_I.m11, gs_I.m22, gs_I.m33);
    //    printf("rot\n"); rot.debugPrint();
    
    return { gs_m, gs_I, gs_com };
}
