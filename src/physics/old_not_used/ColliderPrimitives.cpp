//
//  ColliderPrimitives.cpp
//  helloglfw
//
//  Created by Carl Johan Gribel on 2021-07-25.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include <stdio.h>
#include "ColliderPrimitives.h"
#include "vec.h"

using linalg::v3f;

// MARK: --- 3D Cube -----------------------------------------------------------

const v3f Cube3dData::vertices[8] =
{
    v3f(0.5f,-0.5f, 0.5f), // lower-right, front
    v3f(0.5f, 0.5f, 0.5f), // upper-right, front
    v3f(-0.5f, 0.5f, 0.5f), // upper-left, front
    v3f(-0.5f,-0.5f, 0.5f), // lower-left, front
    v3f(0.5f,-0.5f,-0.5f), // lower-right, back
    v3f(0.5f, 0.5f,-0.5f), // upper-right, back
    v3f(-0.5f, 0.5f,-0.5f), // upper-left, back
    v3f(-0.5f,-0.5f,-0.5f) // lower-left, back
};

const uint Cube3dData::faces[24] =
{
    0,1,2,3,    // front
    4,7,6,5,    // back
    0,4,5,1,    // right
    3,2,6,7,    // left
    1,5,6,2,    // top
    0,3,7,4     // bottom
};

const uint Cube3dData::face_strides[6] = { 4,4,4,4,4,4 };

const uint Cube3dData::edges[24] =
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

const uint Cube3dData::unique_edge_dirs[6] =
{
    0,1,
    0,3,
    0,4
};

// MARK: --- 3D Wedge ----------------------------------------------------------

const v3f Wedge3dData::vertices[6] =
{
    v3f(0.5f,-0.5f, 0.5f),
    v3f(0.5f, 0.5f, 0.5f),
    v3f(-0.5f,-0.5f, 0.5f),
    v3f(0.5f,-0.5f,-0.5f),
    v3f(0.5f, 0.5f,-0.5f),
    v3f(-0.5f,-0.5f,-0.5f)
};

const uint Wedge3dData::faces[18] =
{
    0,1,2,3,    // bottom
    0,3,4,1,    // back
    1,4,5,2,    // slope
    0,1,2,      // hither side
    3,5,4       // dither side
};

const uint Wedge3dData::face_strides[5] = { 4,4,4,3,3 };

const uint Wedge3dData::edges[18] =
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

const uint Wedge3dData::unique_edge_dirs[8] =
{
    0,1,
    0,2,
    0,3,
    1,2
};

// MARK: --- 2D Cube -----------------------------------------------------------

const v2f Cube2dData::vertices[] =
{
    v2f(0.5f,-0.5f), // lower-right
    v2f(0.5f, 0.5f), // upper-right
    v2f(-0.5f, 0.5f), // upper-left
    v2f(-0.5f,-0.5f), // lower-left
};

//const uint Cube2dData::unique_edge_dirs[] =
//{
//    0,
//    1
//};
