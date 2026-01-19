//
//  Colliders.cpp
//  editor
//
//  Created by Carl Johan Gribel on 2023-02-24.
//  Copyright © 2023 Carl Johan Gribel. All rights reserved.
//

#include "Colliders.hpp"
#include "ColliderPrimitives.h"

// --- Collider helpers --------------------------------------------------------

namespace {

#if 0
/*
 generate set of convex geometries from a (potentially) concave polygon
 */
void triangulate_polygon( vec2f v_source[], const int &vc_source, t2Body *body )
{
    /* make a copy of the vertex array
     */
    int vc = vc_source;
    vec2f *v = new vec2f[vc_source];
    for (int i = 0; i < vc; i++)    v[i] = v_source[i];
    
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
    int    i = 0,
    abort_counter = 0;
    while ( vc > 2 )
    {
        int j = (i+1) % vc,
        k = (i+2) % vc;
        vec2f    vi = v[i],
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
            vec2f    vji = -vij,
            vkj = vj - vk;
            for (int si = 1; si < vc-2; si++ )
            {
                int s = (k+si) % vc;
                outside &=    isCCW( vik, v[s] - vi ) ||
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
                for ( int s = j ; s < vc-1; s++ )    v[s] = v[s+1];
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
#endif

}

// MARK: --- ColliderAux -------------------------------------------------------

namespace ColliderAux {

void compute_normals(PolyhedronCollider& poly)
{
    unsigned vindex = 0;
    for (int i = 0; i < poly.nbr_faces; i++)
    {
        const v3f& v0 = poly.vertices_loc[poly.faces[vindex+0]];
        const v3f& v1 = poly.vertices_loc[poly.faces[vindex+1]];
        const v3f& v2 = poly.vertices_loc[poly.faces[vindex+2]];
        poly.normals_loc[i] = normalize((v1 - v0) % (v2 - v0));
        vindex += poly.face_strides[i];
    }
}

void compute_normals(MeshCollider& mesh)
{
    for (int i = 0; i < mesh.faces.size(); i += 3)
    {
        const v3f& v0 = mesh.vertices_loc[mesh.faces[i+0]];
        const v3f& v1 = mesh.vertices_loc[mesh.faces[i+1]];
        const v3f& v2 = mesh.vertices_loc[mesh.faces[i+2]];
        mesh.normals_loc[i/3] = normalize((v1 - v0) % (v2 - v0));
    }
}

void compute_normals(Polygon2dCollider& poly)
{
    for (int i = 0; i < poly.nbr_vertices; i++)
    {
        const v2f& v0 = poly.vertices_loc[i];
        const v2f& v1 = poly.vertices_loc[(i + 1) % poly.nbr_vertices];
        poly.normals_loc[i] = normalize(v2f(v1.y - v0.y, v0.x - v1.x));
    }
}

void apply_local_transform(PolyhedronCollider& poly)
{
    const v3f& t = poly.transform_loc.position;
    const v3f& r = poly.transform_loc.rotation;
    const v3f& s = poly.transform_loc.scaling;
    const m4f M =
    m4f::translation(t) *
    m4f::rotation(r.z*fTO_RAD, r.y*fTO_RAD, r.x*fTO_RAD) *
    m4f::scaling(s);
    
    for (int i = 0; i < poly.vertices_src.size(); i++)
    {
        poly.vertices_loc[i] = xyz(M * xyz1(poly.vertices_src[i]));
    }
    
//    for (auto& v : poly.vertices_loc) v = (M * v.xyz1()).xyz();
}

void apply_local_transform(Polygon2dCollider& poly)
{
    const v2f& t = poly.transform_loc.position;
    const float r = poly.transform_loc.rotation;
    const v2f& s = poly.transform_loc.scaling;
    
    for (int i = 0; i < poly.nbr_vertices; i++)
    {
        poly.vertices_loc[i] = t + xy(m3f::rotation_z(r * fTO_RAD) * xy0(s * poly.vertices_src[i]));
    }
    
//    for (auto& v : poly.vertices_loc)
//        v = xy(M * xy01(v));
}

AABB3d compute_AABB(const SphereCollider& sphere)
{
    const v3f& pos = sphere.pos;
    const float& r = sphere.r;
    return AABB3d { pos + v3f{-r, -r, -r}, pos + v3f{r, r, r}};
}

AABB3d compute_AABB(const PolyhedronCollider& poly)
{
    AABB3d aabb;
    for (auto& v : poly.vertices_loc) aabb.grow(v);
    return aabb;
}

AABB3d compute_AABB(const MeshCollider& mesh)
{
    AABB3d aabb;
    for (auto& v : mesh.vertices_loc) aabb.grow(v);
    return aabb;
}

AABB2d compute_AABB(const CircleCollider& circle)
{
    v2f pos = circle.pos;
    float r = circle.r;
    return AABB2d { pos + v2f(-r, -r), pos + v2f(r, r)};
}

AABB2d compute_AABB(const Polygon2dCollider& poly)
{
    AABB2d aabb;
    for (int i = 0; i < poly.nbr_vertices; i++) aabb.grow(poly.vertices_loc[i]);
    return aabb;
}

}

// MARK: --- 2D Colliders ------------------------------------------------------

namespace Collider2dSpawners
{

template<class P>
Polygon2dCollider
Spawn(const m4f& M)
{
    static_assert(arrlen(P::vertices) <= Collider2dMaxVertices, "");
        
    Polygon2dCollider poly;
    
    poly.transform_loc.position = xy(extract_translation(M));
    poly.transform_loc.rotation = extract_Euler_angle_z(M);
    poly.transform_loc.scaling = xy(extract_scaling(M));
    
    poly.nbr_vertices = arrlen(P::vertices);
    std::copy(P::vertices,
              P::vertices + poly.nbr_vertices,
              poly.vertices_src);
    
    // Computes local vertices from source vertices
    ColliderAux::apply_local_transform(poly);
    // Computes local normals
    ColliderAux::compute_normals(poly);
    
    return poly;
}

template
Polygon2dCollider
Spawn<Cube2dData>(const m4f&);
// + Instantiations for other 2d collider primitives

Polygon2dCollider
Import(std::shared_ptr<SourceMesh> src_mesh,
      unsigned mesh_index)
{
    Polygon2dCollider poly;
    
    // Source data pointers
    const v3f* vptr         = static_cast<const v3f*>(src_mesh->get_attrib_ptr_(VertexAttribTag::Position));
    const uint* strideptr   = static_cast<const uint*>(src_mesh->get_attrib_ptr_(VertexAttribTag::FaceStride));
    const uint* iptr        = static_cast<const uint*>(src_mesh->get_attrib_ptr_(VertexAttribTag::Index));
    
    // Mesh to import
    auto& mesh = src_mesh->m_meshes[mesh_index];
    int nbr_faces = mesh.nbr_faces;
    
    // Extract global mesh transform
    src_mesh->animate(-1, 0.0f);
    auto G = (mesh.node_index == -1? m4f_identity : src_mesh->m_nodetree.nodes[mesh.node_index].global_tfm);
    
    // Accumulated face strides
    uint face_stride_acc = 0;
    
    // Import vertices, face strides and face indices
    assert(nbr_faces == 1);
    for (int j = 0; j < nbr_faces; ++j)
    {
        uint nbr_vertices = 0;
        v2f vertices[Collider2dMaxVertices];
        uint face_stride = *(strideptr + mesh.base_face + j);
        assert(face_stride <= Collider2dMaxVertices);
        
        for (int k = 0; k < face_stride; ++k)
        {
            uint vertex_index = *(iptr + mesh.base_index + face_stride_acc + k);
            const v3f* vertex = vptr + mesh.base_vertex + vertex_index;
            vertices[nbr_vertices++] = xy(*vertex);
        }
        
        // Split if concave ...
        
        std::copy(vertices,
                  vertices + nbr_vertices,
                  poly.vertices_src);
        poly.nbr_vertices = nbr_vertices;
     
        // Apply global mesh transform to vertices
        for (int i = 0; i < poly.nbr_vertices; i++)
        {
            auto& v = poly.vertices_src[i];
            v = xy(G * xy01(v));
        }
        
        // Compute local vertices from source vertices
        ColliderAux::apply_local_transform(poly);
        // Compute local normals
        ColliderAux::compute_normals(poly);
        
        face_stride_acc += face_stride;
    }
    
    return poly;
}

}

// MARK: ---3D Colliders ------------------------------------------------------

namespace Collider3dSpawners
{

// Respawn(PolyhedronCollider& poly)
//{
//    poly = Spawn(poly.M);
//}

template<class P>
PolyhedronCollider Spawn()
{
    PolyhedronCollider poly;
    size_t nbr_vertices = arrlen(P::vertices);
    
    poly.vertices_src.insert(poly.vertices_src.end(), P::vertices, P::vertices + nbr_vertices);

    poly.nbr_faces = arrlen(P::face_strides);
    poly.face_strides.insert(poly.face_strides.end(),
                             P::face_strides,
                             P::face_strides + poly.nbr_faces);
    
    poly.faces.insert(poly.faces.end(),
                      P::faces,
                      P::faces + arrlen(P::faces));
    
    poly.edges.insert(poly.edges.end(),
                      P::edges,
                      P::edges + arrlen(P::edges));

    poly.unique_edge_dirs.insert(poly.unique_edge_dirs.end(),
                                 P::unique_edge_dirs,
                                 P::unique_edge_dirs + arrlen(P::unique_edge_dirs));

    poly.vertices_loc.resize(nbr_vertices);
    poly.normals_loc.resize(poly.nbr_faces);

    // Computes local vertices from source vertices
    ColliderAux::apply_local_transform(poly);
    // Computes local normals
    ColliderAux::compute_normals(poly);
    
    poly.vertices_w.resize(nbr_vertices);
    poly.normals_w.resize(poly.nbr_faces);

    return poly;
}

template
PolyhedronCollider Spawn<Cube3dData>();

PolyhedronCollider Spawn(const AABB3d& aabb)
{
    PolyhedronCollider poly = Spawn<Cube3dData>();
    
    poly.vertices_src[0] = v3f { aabb.max.x, aabb.min.y, aabb.max.z }; // lower-right, front
    poly.vertices_src[1] = v3f { aabb.max.x, aabb.max.y, aabb.max.z }; // upper-right, front
    poly.vertices_src[2] = v3f { aabb.min.x, aabb.max.y, aabb.max.z }; // upper-left, front
    poly.vertices_src[3] = v3f { aabb.min.x, aabb.min.y, aabb.max.z }; // lower-left, front
    poly.vertices_src[4] = v3f { aabb.max.x, aabb.min.y, aabb.min.z }; // lower-right, back
    poly.vertices_src[5] = v3f { aabb.max.x, aabb.max.y, aabb.min.z }; // upper-right, back
    poly.vertices_src[6] = v3f { aabb.min.x, aabb.max.y, aabb.min.z }; // upper-left, back
    poly.vertices_src[7] = v3f { aabb.min.x, aabb.min.y, aabb.min.z }; // lower-left, back
    
    ColliderAux::apply_local_transform(poly);
    ColliderAux::compute_normals(poly);
    
    return poly;
}

PolyhedronCollider Import_PolyhedronCollider(std::shared_ptr<SourceMesh> src_mesh,
                                             unsigned mesh_index)
{
    PolyhedronCollider poly;
    
    // Mesh to import
    auto& mesh = src_mesh->m_meshes[mesh_index];
    
    // Source data pointers
    const v3f* vptr         = static_cast<const v3f*>(src_mesh->get_attrib_ptr_(VertexAttribTag::Position));
    const uint* strideptr   = static_cast<const uint*>(src_mesh->get_attrib_ptr_(VertexAttribTag::FaceStride));
    const uint* iptr        = static_cast<const uint*>(src_mesh->get_attrib_ptr_(VertexAttribTag::Index));
    
    // Extract global mesh transform
    src_mesh->animate(-1, 0.0f);
    auto G = src_mesh->m_nodetree.nodes[mesh.node_index].global_tfm;
    
    // Import vertices
    poly.vertices_src.insert(poly.vertices_src.end(),
                             vptr + mesh.base_vertex,
                             vptr + mesh.base_vertex + mesh.nbr_vertices);
    
    // Apply global mesh transform to vertices
    for (auto& v : poly.vertices_src) v = xyz(G * xyz1(v));
    
    // Import strides
    poly.nbr_faces = mesh.nbr_faces;
    poly.face_strides.insert(poly.face_strides.end(),
                             strideptr + mesh.base_face,
                             strideptr + mesh.base_face + poly.nbr_faces);
    
    // Import faces
    poly.faces.insert(poly.faces.end(),
                      iptr + mesh.base_index,
                      iptr + mesh.base_index + mesh.nbr_indices);

    // Generate edges
    using Edge = std::pair<uint, uint>;
    auto EdgeHash = [](const Edge& p){ return std::hash<uint>()(p.first); };
    auto EdgeEqual = [](const Edge& p1, const Edge& p2)
    { return (p1.first == p2.first && p1.second == p2.second) ||
        (p1.first == p2.second && p1.second == p2.first); };
    std::unordered_set<Edge, decltype(EdgeHash), decltype(EdgeEqual)> edge_set(128, EdgeHash, EdgeEqual);
    unsigned vindex = 0;
    for (int i = 0; i < poly.nbr_faces; i++)
    {
        for (int j = 0; j < poly.face_strides[i]; j++)
            edge_set.insert(std::pair<uint, uint> { poly.faces[vindex+j], poly.faces[vindex+((j+1) % poly.face_strides[i])] });
        vindex += poly.face_strides[i];
    }
#if 1
    // Unique edges & edge directions
    std::vector<Edge> edges_tmp;
    std::vector<Edge> unique_edges_tmp;
    // Unique edges
    for (auto& candidate_pair : edge_set)
    {
        const v3f& e1a = poly.vertices_src[candidate_pair.first];
        const v3f& e1b = poly.vertices_src[candidate_pair.second];
        const v3f v1 = normalize(e1a - e1b);
        // Unique edges
        auto edge_dupl = std::find_if(edges_tmp.begin(), edges_tmp.end(), [&](const Edge& edge) -> bool
        {
            // See if edges coincide
            const float eps = 1.0e-5; const float epssq = eps*eps;
            const v3f& e2a = poly.vertices_src[edge.first];
            const v3f& e2b = poly.vertices_src[edge.second];
            if ( (e2a - e1a).norm2squared() < epssq && (e2b - e1b).norm2squared() < epssq )
                return true;
            if ( (e2a - e1b).norm2squared() < epssq && (e2b - e1a).norm2squared() < epssq )
                return true;
            return false;
        });
        // Only add edge if a coinciding edge does not already exists
        if (edge_dupl == edges_tmp.end()) edges_tmp.push_back(candidate_pair);
        
        // Unique edge directions
        auto uedge_dupl = std::find_if(unique_edges_tmp.begin(), unique_edges_tmp.end(), [&](const Edge& edge) -> bool
        {
            const float eps = 1.0e-5;;
            const v3f& e2a = poly.vertices_src[edge.first];
            const v3f& e2b = poly.vertices_src[edge.second];
            return (1.0f - fabs(v1.dot(normalize(e2a - e2b)))) < eps;
        });
        // Only add edge if not parallel to one that already exists
        if (uedge_dupl == unique_edges_tmp.end()) unique_edges_tmp.push_back(candidate_pair);
    }
    // If std::pair's are stored sequentially, data could be copied directly
    poly.edges.resize(edges_tmp.size()*2);
//    edges = gColliderManager.GeometryData.create_array<unsigned>(edges_tmp.size()*2);
    for (int i = 0; i < edges_tmp.size(); i++) { poly.edges[i*2+0] = edges_tmp[i].first; poly.edges[i*2+1] = edges_tmp[i].second; }
    poly.unique_edge_dirs.resize(unique_edges_tmp.size()*2);
    //    unique_edge_dirs = gColliderManager.GeometryData.create_array<unsigned>(unique_edges_tmp.size()*2);
    for (int i = 0; i < unique_edges_tmp.size(); i++) { poly.unique_edge_dirs[i*2+0] = unique_edges_tmp[i].first; poly.unique_edge_dirs[i*2+1] = unique_edges_tmp[i].second; }
#else
    edges = gColliderManager.GeometryData.create_array<unsigned>(edge_set.size()*2);
    // TODO: detect _unique_ edge directions
    unique_edge_dirs = gColliderManager.GeometryData.create_array<unsigned>(edge_set.size()*2);

    int i = 0;
    for (auto& p : edge_set)
    {
//        std::cout << p.first << ", " << p.second << std::endl;
        edges[i+0] = p.first;
        edges[i+1] = p.second;
        unique_edge_dirs[i+0] = p.first;
        unique_edge_dirs[i+1] = p.second;
        i += 2;
    }
#endif
    
    // Allocate memory for local vertices
    auto nbr_vertices = poly.vertices_src.size();
    poly.vertices_loc.resize(nbr_vertices);
    poly.normals_loc.resize(poly.nbr_faces);

    // Compute local vertices from source vertices
    ColliderAux::apply_local_transform(poly);
    // Compute local normals
    ColliderAux::compute_normals(poly);
    
    // Allocate memory for world vertices
    poly.vertices_w.resize(nbr_vertices);
    poly.normals_w.resize(poly.nbr_faces);
    
#if 0
    std::cout << "ImportedPolyhedronCollider" << std::endl;
    
    std::cout << (mesh.name? mesh.name.c_str() : "unnamed") << std::endl;
    std::cout << "Unique edges: " << edge_set.size()*2 << " -> " << edges_tmp.size()*2 << std::endl;
    std::cout << "Unique edge dirs: " << edge_set.size()*2 << " -> " << unique_edges_tmp.size()*2 << std::endl;
    
    std::cout << "# meshes" << ximesh->m_meshes.size() << std::endl;
    
    std::cout << "base_index " << mesh.base_index << std::endl;
    std::cout << "base_vertex " << mesh.base_vertex << std::endl;
    std::cout << "nbr_indices " << mesh.nbr_indices << std::endl;
    std::cout << "nbr_vertices " << mesh.nbr_vertices << std::endl;
    std::cout << "base_face " << mesh.base_face << std::endl;
    std::cout << "nbr_faces " << mesh.nbr_faces << std::endl;
    
    std::cout << "nbr edges " << edges.size() << std::endl;
    std::cout << "nbr unique edges " << unique_edge_dirs.size() << std::endl;
    
    std::cout << "vertices" << std::endl;
    for (auto& v : vertices)
        std::cout << "\t" << v << std::endl;
    
    std::cout << "face strides" << std::endl;
    for (auto& v : face_strides)
        std::cout << "\t" << v << std::endl;
#endif
    
    return poly;
}

MeshCollider Import_MeshCollider(std::shared_ptr<SourceMesh> src_mesh,
                                  unsigned mesh_index)
{
    auto poly = Import_PolyhedronCollider(src_mesh, mesh_index);
    
    for (auto stride : poly.face_strides)
    {
        assert(stride == 3);
    }
    
    MeshCollider mesh_collider;
    mesh_collider.vertices_loc = poly.vertices_loc;
    mesh_collider.vertices_w = poly.vertices_w; // Empty but resized
    mesh_collider.normals_loc = poly.normals_loc;
    mesh_collider.normals_w = poly.normals_w;  // Empty but resized
    mesh_collider.faces = poly.faces;
    mesh_collider.edges = poly.edges;
    mesh_collider.unique_edge_dirs = poly.unique_edge_dirs; // Probably not
    
    // TODO: Build data structure of some kind (spatial hash, AABB tree ...)
    
    return mesh_collider;
}

}

// MARK: --- ColliderTypePair --------------------------------------------------

bool operator == (const Collider3dTypePair& lhs,
                  const Collider3dTypePair& rhs)
{
    return lhs.typeA == rhs.typeA && lhs.typeB == rhs.typeB;
}

bool operator == (const Collider2dTypePair& lhs,
                  const Collider2dTypePair& rhs)
{
    return lhs.typeA == rhs.typeA && lhs.typeB == rhs.typeB;
}
