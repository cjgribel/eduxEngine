//
//  MassProperties.cpp
//
//  Created by Carl Johan Gribel on 2014-11-29.
//  Updated July 2021
//

#include <entt/entt.hpp>
#include "MassProperties.hpp"

namespace {
/*
 * Moment of inertia tensor (I) wrt to a reference point R
 *
 * Ig:     I of body at its centre of mass
 * mass:   Mass of body
 * R:      Vector from centre of mass to new point (direction does not matter)
 * returns
 * J:      I of body wrt reference point R
 *
 *   J[i,j] = Ig[i,j] + m( |R|^2 kronecker[i,j] - R[i]R[j] )
 *   J = Ig + m [ (R.R)E_3x3 - R outer prod R ]
 *
 * This formula is a generalization of the parallel axis theorem,
 * which applies to scalar moments of inertia (with d = |R|):
 * J = I * m*d^2
 *
 *
 * http://en.wikipedia.org/wiki/Parallel_axis_theorem#Tensor_generalization
 * http://www.bulletphysics.org/Bullet/phpBB3/viewtopic.php?f=4&t=3702
 *
 */
m3f tensor_at(const m3f& Ig,
              float m,
              const v3f& R)
{
    return Ig + (m3f_1 * dot(R, R) - outer_product(R, R)) * m;
}

float tensor_at(float Ig,
                float m,
                const v2f& R)
{
    return Ig + length_squared(R) * m;
}

// MARK: --- 3D Mass properties ------------------------------------------------

/*
 * Tetrahedron volume (signed)
 *
 * V = det| a-d b-d c-d | / 6 = ( (a-d).( (b-d) x (c-d) ) / 6
 *
 * http://en.wikipedia.org/wiki/Tetrahedron#Volume
 *
 */
float TetrahedronVolume(v3f& a,
                        v3f& b,
                        v3f& c,
                        v3f& d)
{
    return ((a - d).dot((b - d) % (c - d)))/6.0f;
}

/*
 * Tetrahedron centre of mass = its centroid
 *
 * http://en.wikipedia.org/wiki/Centroid#Of_triangle_and_tetrahedron
 * http://www.globalspec.com/reference/52702/203279/4-8-the-centroid-of-a-tetrahedron
 *
 */
v3f TetrahedronCentroid(v3f& a,
                        v3f& b,
                        v3f& c,
                        v3f& d)
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
 * Aggregate centre of mass
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
MassProperties3d TetrahedronMassProperties(v3f& p,
                                           v3f& q,
                                           v3f& r,
                                           v3f& s,
                                           float rho)
{
    // Volume
    float th_V = -TetrahedronVolume(p,q,r,s);
    // Mass
    float th_m = th_V*rho;
    // Centre of mass (centroid)
    vec3f th_com = TetrahedronCentroid(p,q,r,s);
    
    //
    // Inertia tensor
    //
    
    // Vertices (p,q,r,s) in centre of mass space
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
    
    m3f th_J = m3f(Ja, -Jbp, -Jcp,
                   -Jbp, Jb, -Jap,
                   -Jcp, -Jap, Jc) * (rho * fabs(J_det));
    
    return { th_m, th_J, th_com };
}

} // Anonymous namespace

/*
 * Convex polyhedra mass properties
 *
 * Method:
 * 1. Tetrahedralize wrt to a Steiner point (= local origin) (tetrahedralization is always possible for convex polyhedra [Ericson04])
 * 2. Compute attribs for each tetrahedra
 * 3. Compute centre of mass for the entire polyhedra
 * 4. Compute inertia tensor for the entire polyhedra
 */
MassProperties3d PolyhedronMassProperties(const entt::entity collider_entity,
                                          entt::registry& registry,
                                          const m4f& tfm)
{
    auto& base_collider = registry.get<Base3dCollider>(collider_entity);
    auto& poly = registry.get<PolyhedronCollider>(collider_entity);
    
    auto vertices = poly.vertices_loc;
    for (auto& v : vertices) v = xyz(tfm * xyz1(v));
    
    float poly_m = 0;
    v3f poly_com = v3f(0,0,0); //  Any point, does not need to be inide polyhedron.
    m3f poly_I = m3f_0;
    std::vector<MassProperties3d> th_mprops;
    
    // Centroid for tetrahedralization
    v3f p = v3f(0,0,0);
    for (int i = 0; i < vertices.size(); i++)
        p += vertices[i];
    p /= vertices.size();
        // or
    //    base_collider.aabb.get_midpoint();
    
    // Create tetrahedra in local geometry space and compute their attributes
    unsigned vindex = 0;
    for (int i=0; i<poly.nbr_faces; i++)
    {
        for (int j = 1; j < poly.face_strides[i]-1; j++)
        {
            v3f q = vertices[ poly.faces[vindex + 0]    ];
            v3f r = vertices[ poly.faces[vindex + j]    ];
            v3f s = vertices[ poly.faces[vindex + j+1]  ];
            
            th_mprops.push_back(TetrahedronMassProperties(p, q, r, s, base_collider.density));
        }
        vindex += poly.face_strides[i];
    }
    
    // Aggregate mass and centre of mass
    for (MassProperties3d& th_mprop : th_mprops)
    {
        poly_m += th_mprop.m;
        poly_com += th_mprop.com * th_mprop.m;
    }
    poly_com /= poly_m;
    
    // Aggregate inertia tensor
    for (MassProperties3d& th_mprop : th_mprops)
        poly_I += tensor_at(th_mprop.I,
                            th_mprop.m,
                            poly_com - th_mprop.com);
    
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
 * Sphere may no be centred
 *
 * mass = rho * volume = rho * 4PIr^3/3
 * moment of inertia, I = 2mr^2/5 * 3x3_identity
 */
MassProperties3d SphereMassProperties(const entt::entity collider_entity,
                                      entt::registry& registry,
                                      const m4f& tfm)
{
    auto& base_collider = registry.get<Base3dCollider>(collider_entity);
    auto& sphere = registry.get<SphereCollider>(collider_entity);
    float tfm_s = extract_scaling(tfm).x;
    
    const v3f& p = xyz(tfm * xyz1(sphere.pos));
    const float r = tfm_s * sphere.r;
    
    const float m = base_collider.density * 4.0f*fPI * r*r*r/3.0f;
    const m3f I = m3f_1 * (2.0f * m * r*r/5.0f);
  
    return MassProperties3d { m, I, p };
}

/*
 * Point mass properties
 * Since a point mass has zero volume: assume rho is passed on as desired mass,
 * and model MOI after a sphere.
 *
 * mass = rho
 * moment of inertia, I = 2m/5 * 3x3_identity
 */
MassProperties3d PointMassProperties(const entt::entity collider_entity,
                                     entt::registry& registry,
                                     const m4f& tfm)
{
    auto& base_collider = registry.get<Base3dCollider>(collider_entity);
    
    float m = base_collider.density;
    
    mat3f I = mat3f(0);
    I.m11 = I.m22 = I.m33 = 2.0f*m/5.0f;
    
    return { m, I, extract_translation(tfm) };
}

/*
 * AABB/Cube mass properties
 *
 * mass = rho
 * moment of inertia, I = 2m/5 * 3x3_identity
 */
//MassProperties2d RectangleMassProperties(Handle<AABB2dCollider> pc)
//{
//    float m = pc->density;
//
//    mat3f I = mat3f(0);
//    I.m11 = I.m22 = I.m33 = 2.0f*m/5.0f;
//
//    return { m, I, {0,0,0} };
//}

/*
 * Collider mass properties
 */
MassProperties3d
Collider3dMassProperties(const entt::entity collider_entity,
                         entt::registry& registry,
                         const m4f& tfm)
{
    auto& base_collider = registry.get<Base3dCollider>(collider_entity);
    
    switch (base_collider.type)
    {
        case Collider3dType::Sphere:
            return SphereMassProperties(collider_entity, registry, tfm);
            break;
            
        case Collider3dType::Polyhedron:
            return PolyhedronMassProperties(collider_entity, registry, tfm);
            break;
            
        case Collider3dType::Point:
            return PointMassProperties(collider_entity, registry, tfm);
            break;

        case Collider3dType::Mesh:
            return MassProperties3d {
                .m = 1.0f,
                .I = mat3f(1.0f),
                .com = extract_translation(tfm)
            };
            break;
            
        case Collider3dType::Plane:
            assert(0 && "PlaneCollider attribs should be set explicitly");
            return MassProperties3d {
                .m = std::numeric_limits<float>::infinity(),
                .I = mat3f(std::numeric_limits<float>::infinity()),
                .com = extract_translation(tfm)
            };
            break;
            
        default:
            throw std::runtime_error("MassProperties not available for collider type");
            break;
    }
}

MassProperties3d
AggregateMassProperties3d(const std::vector<entt::entity>& collider_entities,
                          entt::registry& registry,
                          const m4f& tfm)
{
#if 0
    v3f A1 = v3f(8.33220, -11.86875, 0.93355);
     v3f A2 = v3f(0.75523 ,5.00000, 16.37072);
     v3f A3 = v3f(52.61236, 5.00000, -5.38580);
     v3f A4 = v3f(2.00000, 5.00000, 3.00000);
    return TetrahedronMassProperties(A1, A2, A3, A4, 1);
#endif
    
    float gs_m = 0;
    m3f gs_I = m3f_0;
    v3f gs_com = v3f_000;
    std::vector<MassProperties3d> g_mprops;   // collider mass properties
    
    // Collider mass properties and local offsets
    for (auto& collider_entity : collider_entities)
        g_mprops.push_back(Collider3dMassProperties(collider_entity, registry, tfm));
    
    // Aggregate mass & centre of mass
    for (int i=0; i<g_mprops.size(); i++)
    {
        gs_m += g_mprops[i].m;
        gs_com += g_mprops[i].com * g_mprops[i].m;
    }
    gs_com /= gs_m;
    
    // Aggeregate inertia tensor
    for (int i=0; i<g_mprops.size(); i++)
        gs_I += tensor_at(g_mprops[i].I,
                          g_mprops[i].m,
                          gs_com - g_mprops[i].com);
    
    // Notes on diagonalization
    //
    // The intertia tensor I of a body is always symmetric.
    // I is also diagonal, if its eigenvectors are aligned with the local frame.
    // Diagonalize I to I_diag => rotate the body by R so that the local basis
    // aligns with the eigenvectors of I:
    //      I = R * I_diag * R^T
    
    // diagonalize J and create a final tensor from its diagonal elements
    //    mat3 rot;
    //    gs_I.diagonalize(rot, 0.00001, 20);
    //    gs_I = mat3(gs_I.m11, gs_I.m22, gs_I.m33);
    //    printf("rot\n"); rot.debugPrint();
    
    return { gs_m, gs_I, gs_com };
}

// MARK: --- 2D Mass properties ------------------------------------------------

MassProperties2d
Triangle2dMassProperties(const v2f& v0,
                         const v2f& v1,
                         const v2f& v2,
                         float density)
{
    const float area = cross(v1 - v0, v2 - v0) * 0.5f;
    const float mass = density * area;
    const v2f com = (v0 + v1 + v2) / 3.0f;
    
    const v2f e = v1 - v0, f = v2 - v0;
    const float I = mass/18.0f*(length_squared(e) + length_squared(f) - dot(e, f));
    
    return MassProperties2d { mass, I, com };
}

MassProperties2d
Polygon2dMassProperties(const entt::entity collider_entity,
                        entt::registry& registry,
                        const m4f& tfm)
{
    auto& base_collider = registry.get<Base2dCollider>(collider_entity);
    auto& poly = registry.get<Polygon2dCollider>(collider_entity);
    
    v2f vertices[poly.nbr_vertices];
    for (int i = 0; i < poly.nbr_vertices; i++)
        vertices[i] = xy(tfm * xy01(poly.vertices_loc[i]));
    
    float poly_m = 0;
    v2f poly_com = v2f_00;
    float poly_I = 0.0f;
    std::vector<MassProperties2d> tri_mprops;
    
    // Create triangles in local geometry space and compute their attributes
    for (int i = 1; i< poly.nbr_vertices - 1; i++)
    {
        const v2f& q = vertices[0];
        const v2f& r = vertices[i];
        const v2f& s = vertices[i+1];
        
        tri_mprops.push_back( Triangle2dMassProperties(q,
                                                       r,
                                                       s,
                                                       base_collider.density) );
    }
    
    // Aggregate mass and centre of mass
    for (auto& tri_mprop : tri_mprops)
    {
        poly_m += tri_mprop.m;
        poly_com += tri_mprop.com * tri_mprop.m;
    }
    poly_com /= poly_m;
    
    // Aggregate inertia tensor
    for (MassProperties2d& tri_mprop : tri_mprops)
        poly_I += tensor_at(tri_mprop.I,
                            tri_mprop.m,
                            poly_com - tri_mprop.com);
    
    return MassProperties2d { poly_m, poly_I, poly_com };
}

MassProperties2d Circle2dMassProperties(const entt::entity collider_entity,
                                        entt::registry& registry,
                                        const m4f& tfm)
{
    auto& base_collider = registry.get<Base2dCollider>(collider_entity);
    auto& circle = registry.get<CircleCollider>(collider_entity);
    float tfm_s = extract_scaling(tfm).x;
    
    const v2f& p = xy(tfm * xy01(circle.pos));
    const float r = tfm_s * circle.r;
    
    const float area = M_PI * r * r;
    const float m = base_collider.density * area;
    const float I = m * 0.5f * r * r;
    
    return MassProperties2d { m, I, p };
}

//MassProperties2d AAbb2dMassProperties(const entt::entity collider_entity,
//                                      entt::registry& registry)
//{
//    auto& base_collider = registry.get<Base2dCollider>(collider_entity);
//
//    const AABB2d& aabb = base_collider.aabb_w;
//    const float w = aabb.max.x - aabb.min.x;
//    const float h = aabb.max.y - aabb.min.y;
//    const float area = w * h;
//    const float mass = base_collider.density * area;
//    const v2f com = aabb.get_midpoint();
//    const float I = mass * 1.0f/12.0f * (w*w + h*h);
//
//    return MassProperties2d { mass, I, com };
//}

MassProperties2d
Collider2dMassProperties(const entt::entity collider_entity,
                         entt::registry& registry,
                         const m4f& tfm)
{
    auto& base_collider = registry.get<Base2dCollider>(collider_entity);
    
    switch (base_collider.type)
    {
        case Collider2dType::Polygon:
            return Polygon2dMassProperties(collider_entity, registry, tfm);
            break;

        case Collider2dType::Circle:
            return Circle2dMassProperties(collider_entity, registry, tfm);
            break;
            
//        case ColliderType::AABB2d:
//            return AAbb2dMassProperties(collider_entity, registry);
//            break;
            
        default:
            throw std::runtime_error("MassProperties not available for collider type");
            break;
    }
}

MassProperties2d
AggregateMassProperties2d(const std::vector<entt::entity>& collider_entities,
                          entt::registry& registry,
                          const m4f& tfm)
{
    float gs_m = 0;
    float gs_I = 0.0f;
    v2f gs_com = v2f_00;
    std::vector<MassProperties2d> g_mprops;   // collider mass properties
    
    // Collider mass properties and local offsets
    //for (collider_t *g : geometries)
    for (auto& collider_entity : collider_entities)
        g_mprops.push_back(Collider2dMassProperties(collider_entity, registry, tfm));
    
    // Aggregate mass & centre of mass
    for (int i=0; i<g_mprops.size(); i++)
    {
        gs_m += g_mprops[i].m;
        gs_com += g_mprops[i].com * g_mprops[i].m;
    }
    gs_com /= gs_m;
    
    // Aggeregate inertia tensor
    for (int i=0; i<g_mprops.size(); i++)
        gs_I += tensor_at(g_mprops[i].I,
                          g_mprops[i].m,
                          gs_com - g_mprops[i].com);
    
    return { gs_m, gs_I, gs_com };
}
