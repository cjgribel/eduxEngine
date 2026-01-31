//
//  geometry_data.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2015-01-08.
//
//

#ifndef collider_data_h
#define collider_data_h

//
// Geometry data for:
// - Box
// - Wedge
// - Tetrahedron
// - Octagon
// - Double-sided triangle


#if 0
//struct collider_geometry_t
//{
//    const vec3f vertices;
//    const vec3f indices;
//};

//struct collider_box_t
//{
//    const float3 vertices[8] =
//    {
//        float3(0.5f,-0.5f, 0.5f),
//        float3(0.5f, 0.5f, 0.5f),
//        float3(-0.5f, 0.5f, 0.5f),
//        float3(-0.5f,-0.5f, 0.5f),
//        float3(0.5f,-0.5f,-0.5f),
//        float3(0.5f, 0.5f,-0.5f),
//        float3(-0.5f, 0.5f,-0.5f),
//        float3(-0.5f,-0.5f,-0.5f)
//    };
//    
//} collider_box;

class convex_polyhedron_t
{
    virtual int nbr_vertices() = 0;
    
    virtual float* vertices() = 0;
};

class box_t : public convex_polyhedron_t
{
    int nbr_vertices() override { return 0; }
    
    float* vertices() override { return nullptr;  }
};

static box_t box;
#endif


//
// Box
//

const float3 unit_box_va[8] =
{
    float3(0.5f,-0.5f, 0.5f),
    float3(0.5f, 0.5f, 0.5f),
    float3(-0.5f, 0.5f, 0.5f),
    float3(-0.5f,-0.5f, 0.5f),
    float3(0.5f,-0.5f,-0.5f),
    float3(0.5f, 0.5f,-0.5f),
    float3(-0.5f, 0.5f,-0.5f),
    float3(-0.5f,-0.5f,-0.5f)
};

const int unit_box_nbr_vertices = 8;

const ui32 unit_box_faces[6*4] =
{
    0,1,2,3,
    4,7,6,5,
    0,4,5,1,
    3,2,6,7,
    1,5,6,2,
    0,3,7,4
};

const int unit_box_nbr_faces = 6;

const int unit_box_face_stride = 4;

const ui32 unit_box_edges[12*2] =
{
    0,1,
    0,3,
    2,1,
    2,3,
    4,5,
    4,7,
    6,5,
    6,7,
    0,4,
    1,5,
    2,6,
    3,7
};

const ui32 unit_box_unique_edge_dirs[6] =
{
    0,1,
    0,3,
    0,4
};

//
// Wedge
//

const float3 wedge_va[8] =
{
    float3(0.5f,-0.5f, 0.5f),
    float3(0.5f, 0.5f, 0.5f),
    float3(-0.5f,-0.5f, 0.5f),
    float3(0.5f,-0.5f,-0.5f),
    float3(0.5f, 0.5f,-0.5f),
    float3(-0.5f,-0.5f,-0.5f)
};

const int wedge_nbr_vertices = 6;

const ui32 wedge_faces[8*3] =
{
    0,1,2,
    0,3,4,
    4,1,0,
    3,5,4,
    0,2,5,
    5,3,0,
    1,4,5,
    5,2,1
};

const int wedge_nbr_faces = 8;

const int wedge_face_stride = 3;

const ui32 wedge_edges[9*2] =
{
    0,1,
    1,2,
    2,0,
    0,3,
    1,4,
    2,5,
    3,4,
    4,5,
    5,3
};

const ui32 wedge_unique_edge_dirs[4*2] =
{
    0,1,
    0,2,
    0,3,
    1,2
};

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
