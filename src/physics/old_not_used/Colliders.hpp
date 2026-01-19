//
//  Colliders.hpp
//  editor
//
//  Created by Carl Johan Gribel on 2023-02-24.
//  Copyright © 2023 Carl Johan Gribel. All rights reserved.
//

#ifndef Colliders_hpp
#define Colliders_hpp

#include <stdio.h>
#include <vector>
#include "hash_combine.h"
#include "vec.h"
#include "mat.h"
#include "AABB.h"
#include "config.h"
#include "ColliderPrimitives.h"
#include "SourceMesh.hpp"
//#include "RigidBody.hpp"

using namespace linalg;

struct Transform3dSeparated
{
    v3f position    = v3f_000;
    v3f rotation    = v3f_000;
    v3f scaling     = v3f_111;
};

struct Transform2dSeparated
{
    v2f position    = v2f_00;
    float rotation  = 0.0f;
    v2f scaling     = v2f_11;
};

// MARK: --- Collider types (ENGINE) -------------------------------------------

enum class Collider3dType : unsigned
{
    Plane = 0,
    Point,
    Sphere,
    Polyhedron,
    Capsule, // not implemented
    Mesh,
    Count
};

enum class Collider2dType : unsigned
{
    Circle,
    Polygon,
    Count
};

// MARK: --- Collision mask (ENGINE) -------------------------------------------

template<int NbrLayers>
class CollisionLayerMaskType
{
public:
    static inline std::bitset<NbrLayers*(NbrLayers+1)/2> layer_mapping;
    
    static inline int pair_index(uint i,
                                 uint j)
    {
        assert(i < NbrLayers);
        assert(j < NbrLayers);
//        assert((int)i < (int)CollisionLayer::NbrLayers);
//        assert((int)j < (int)CollisionLayer::NbrLayers);
        
        uint i_ = i, j_ = j;
        if (i_ > j_) std::swap(i_, j_);
        return NbrLayers*i_ - 0.5f*(i_*i_ + i_) + j_;
    }
    
    static inline bool check(uint i,
                             uint j)
    {
        return layer_mapping[pair_index(i, j)];
    }
    
    static inline void set(uint i,
                           uint j,
                           bool flag)
    {
        layer_mapping[pair_index(i, j)] = flag;
    }
    
    static inline void set_all(bool flag)
    {
        if (flag)
            layer_mapping.set();
        else
            layer_mapping.reset();
    }
};
using CollisionLayerMask = CollisionLayerMaskType<8>;

// MARK: --- Collider helpers --------------------------------------------------

struct SphereCollider;
struct PolyhedronCollider;
struct MeshCollider;
struct CircleCollider;
struct Polygon2dCollider;

// TODO: ColliderOp
namespace ColliderAux {

void compute_normals(PolyhedronCollider& poly);

void compute_normals(MeshCollider& mesh);

void compute_normals(Polygon2dCollider& poly);

void apply_local_transform(PolyhedronCollider& poly);

void apply_local_transform(Polygon2dCollider& poly);

AABB3d compute_AABB(const SphereCollider& sphere);

AABB3d compute_AABB(const PolyhedronCollider& poly);

AABB3d compute_AABB(const MeshCollider& mesh);

AABB2d compute_AABB(const CircleCollider& circle);

AABB2d compute_AABB(const Polygon2dCollider& poly);

}

// MARK: --- Base Colliders ----------------------------------------------------

struct Base3dCollider
{
    Collider3dType type;
    uint layer;
    AABB3d aabb;
    AABB3d aabb_w; // RUNTIME
//    v3f inside_point; // SKIP & compute from AABB?
//    v3f inside_point_w;  // RUNTIME
    bool is_trigger;
    float density = DensityDefault;
};

struct Base2dCollider
{
    Collider2dType type;
    uint layer;
    AABB2d aabb;
    AABB2d aabb_w; // RUNTIME
//    v3f inside_point; // SKIP & compute from AABB?
//    v3f inside_point_w;  // RUNTIME
    bool is_trigger;
    v2u particle_colors;
    float density = DensityDefault;
};

// MARK: --- 2D Colliders ------------------------------------------------------

struct CircleCollider
{
    v2f pos;
    float r;
    
    v2f pos_w; // RUNTIME
    float r_w;
};

#define Collider2dMaxVertices 16
class Polygon2dCollider
{
public:
    v2f vertices_src[Collider2dMaxVertices];
//    v2f normals_src[Collider2dMaxVertices];
    
    v2f vertices_loc[Collider2dMaxVertices];
    v2f normals_loc[Collider2dMaxVertices];
    
    v2f vertices_w[Collider2dMaxVertices];     // runtime
    v2f normals_w[Collider2dMaxVertices];      // runtime
    unsigned nbr_vertices = 0;
    
    Transform2dSeparated transform_loc; // src -> loc
};

namespace Collider2dSpawners
{

template<class P>
Polygon2dCollider
Spawn(const m4f& M = m4f_1);

Polygon2dCollider
Import(std::shared_ptr<SourceMesh> src_mesh,
      unsigned mesh_index);

}

// MARK: ---3D Colliders -------------------------------------------------------

struct SphereCollider
{
public:
    v3f pos;
    float r;
    
    v3f pos_w;
    float r_w;
};

// TODO: AABB ???
struct PlaneCollider
{
public:
    v3f p, n;
    v3f p_w, n_w;
};

// Placeholder
// Design the AABB carefully here
//class PointCollider : public ColliderBase
//{
//    explicit PointCollider(const v3f& point,
//                           bool is_trigger = false)
//    : ColliderBase(ColliderType::Point, is_trigger) { inside_point = point; }
//};

struct PolyhedronCollider
{
    std::vector<v3f> vertices_src; //, normals_src;         // SRC
    std::vector<v3f> vertices_loc, normals_loc;     // LOCAL = transform * SRC
    std::vector<v3f> vertices_w, normals_w;             // WORLD = LOCAL * entity transform
    
    std::vector<unsigned> faces, edges, unique_edge_dirs;
    std::vector<unsigned> face_strides;
    unsigned nbr_faces = 0;
    
    Transform3dSeparated transform_loc; // src -> loc
};

struct MeshCollider
{
    std::vector<v3f> vertices_loc, normals_loc;
    std::vector<v3f> vertices_w, normals_w;
    
    std::vector<unsigned> faces, edges, unique_edge_dirs /* ??? */;
    //std::vector<unsigned> face_strides;
    //unsigned nbr_faces = 0;
};

class CapsuleCollider
{
    // Line segment + radius
    // http://media.steampowered.com/apps/valve/2015/DirkGregorius_Contacts.pdf (slide 99)
};

namespace Collider3dSpawners
{
template<class P>
PolyhedronCollider 
Spawn();

PolyhedronCollider 
Spawn(const AABB3d& aabb);

PolyhedronCollider 
Import_PolyhedronCollider(std::shared_ptr<SourceMesh> src_mesh,
                          unsigned mesh_index);

MeshCollider 
Import_MeshCollider(std::shared_ptr<SourceMesh> src_mesh,
                    unsigned mesh_index);
}

// MARK: --- ColliderTypePair --------------------------------------------------

struct Collider3dTypePair
{
    Collider3dType typeA, typeB;
};

struct Collider2dTypePair
{
    Collider2dType typeA, typeB;
};

template<>
struct std::hash<Collider3dTypePair>
{
    std::size_t operator () (const Collider3dTypePair& pair) const
    {
        size_t seed = 0;
        hash_combine(seed, pair.typeA, pair.typeB);
        return seed;
    }
};

template<>
struct std::hash<Collider2dTypePair>
{
    std::size_t operator () (const Collider2dTypePair& pair) const
    {
        size_t seed = 0;
        hash_combine(seed, pair.typeA, pair.typeB);
        return seed;
    }
};

bool operator == (const Collider3dTypePair& lhs,
                  const Collider3dTypePair& rhs);

bool operator == (const Collider2dTypePair& lhs,
                  const Collider2dTypePair& rhs);

// MARK: -- Enum names registration --------------------------------------------

namespace detail {

template<>
struct enum_meta<Collider3dType>
{
    static inline const char* names[] = { "Plane", "Point", "Sphere", "Polyhedron", "Capsule", "Mesh" };
    static inline const int values[] = { 0, 1, 2, 3, 4, 5 };
};

template<>
struct enum_meta<Collider2dType>
{
    static inline const char* names[] = { "Circle", "Polygon" };
    static inline const int values[] = { 0, 1 };
};

}

#endif /* Colliders_hpp */
