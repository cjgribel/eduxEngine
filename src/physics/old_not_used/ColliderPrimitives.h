//
//  geometry_data.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2015-01-08.
//
//

#ifndef collider_data_h
#define collider_data_h

#include "vec.h"

using linalg::v3f;
using linalg::v2f;
using uint = uint32_t;

struct Cube3dData
{
    static const v3f vertices[8];
    static const uint faces[24];
    static const uint face_strides[6];
    static const uint edges[24];
    static const uint unique_edge_dirs[6];
};

struct Wedge3dData
{
    static const v3f vertices[6];
    static const unsigned faces[18];
    static const uint face_strides[5];
//    static const int face_stride;
    static const unsigned edges[18];
    static const unsigned unique_edge_dirs[8];
};

struct Cube2dData
{
    static const v2f vertices[4];
//    static const uint unique_edge_dirs[2];
};

template<unsigned N>
struct CircleRing3d
{
    v3f vertices[N];
    unsigned nbr_vertices = N;
    
    CircleRing3d()
    {
        for (int i = 0; i < N; i++)
        {
            const float rad = float(i)/(N - 1) * 2.0f * M_PI;
            vertices[i] = v3f { cos(rad), sin(rad), 0.0f };
        }
    }
};



#if 0
//
// Wedge
//



const int wedge_nbr_faces = 8;

const int wedge_face_stride = 3;



//
// Tetrahedron
//

static float3 tetrahedron_va[4] =
{
    float3(0.0f,-0.5f, -0.5f),
    float3(-0.5f, 0.f, 0.5f),
    float3(0.5f, 0.f, 0.5f),
    float3(0.0f, 0.5f, -0.5f)
};

const int tetrahedron_va_size = 4;

static ui32 tetrahedron_faces[4*3] =
{
    0,2,1,
    0,3,2,
    0,1,3,
    1,2,3
};

const int tetrahedron_faces_size = 4;

const ui32 tetrahedron_face_stride = 3;

static ui32 tetrahedron_edges[6*2] =
{
    0,1,
    0,2,
    0,3,
    1,2,
    2,3,
    3,1
};

//
// Octagon
//

static float3 octagon_va[6] =
{
    float3(0.0f,-0.5f, 0.0f),
    float3(-0.5f, 0.0f, 0.5f),
    float3(0.5f, 0.0f, 0.5f),
    float3(0.5f, 0.0f, -0.5f),
    float3(-0.5f, 0.0f, -0.5f),
    float3(0.0f, 0.5f, 0.0f)
};

const int octagon_va_size = 6;

static ui32 octagon_faces[8*3] =
{
    0,2,1,
    0,3,2,
    0,4,3,
    0,1,4,
    5,1,2,
    5,2,3,
    5,3,4,
    5,4,1
};

const int octagon_faces_size = 8;

static ui32 octagon_edges[12*2] =
{
    0,1,
    0,2,
    0,3,
    0,4,
    1,2,
    2,3,
    3,4,
    4,1,
    5,1,
    5,2,
    5,3,
    5,4
};

const ui32 octagon_face_stride = 3;

//
// Double-sided triangle
//

static float3 triangle_va[6] =
{
    float3(0, 0.5, 0),
    float3(0, 0.5, 1),
    float3(1, 0.5, 0),
    float3(0, -0.5, 0),
    float3(0, -0.5, 1),
    float3(1, -0.5, 0)
//    float3(-0.33f,-0.33f, 0.0f),
//    float3(0.66f, -0.33f, 0.0f),
//    float3(-0.33f, 0.66f, 0.0f)
};

const int triangle_va_size = 6;

static ui32 triangle_faces[4*3] =
{
    0,1,2,
    0,3,1,
    1,4,2,
    2,5,0
};

const int triangle_faces_size = 4;

static ui32 triangle_edges[3*2] =
{
    0,1,
    1,2,
    2,0,
    
//    0,3,
//    1,4,
//    2,5
};

const ui32 triangle_face_stride = 3;

#endif

#endif
