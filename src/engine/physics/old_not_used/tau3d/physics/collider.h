//
//  geometry.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2014-10-27.
//
//

#include "vec.h"
#include "mat.h"
#include <vector>
#include "body.h"
#include "AABB.h"
#include "collider_data.h"
#include "mesh.h"

#ifndef tau3d_collider_h
#define tau3d_collider_h

using linalg::vec3f;
using linalg::mat3f;

enum COLLIDER_TYPE { SPHERE, POLYHEDRON, CYLINDER, PLANE, POINT };

class collider_t
{
public:
    float3 X, X_w;          // Center of mass in body and (TODO) world space
    AABB_t AABB;            // Local space AABB
    AABB_t AABB_w;          // World space AABB. Note: not necessarily to be used for e.g. sphere colliders
    COLLIDER_TYPE gtype;
    
    collider_t() {}
    
    collider_t(COLLIDER_TYPE gtype) : X(), gtype(gtype) {}
    
    //    virtual mat4 get_transform() { return mat4(); }
    
    virtual void compute_AABB() { }
    
    virtual void vertex_shade(const mat3f &R, const vec3f &X) = 0;
    
    virtual bool intersect(ray_t &ray) { return false; }
    
    virtual void render() = 0;
    
public:
    
    virtual ~collider_t() {}
};

class poly_collider_t : public collider_t
{
public:
    float3 p_w; // center of mass in world space
    std::vector<float3> vertices, normals;        // geometry data
    std::vector<float3> vertices_w, normals_w;    // geometry data (world)
    std::vector<ui32> faces, edges, unique_edge_dirs;
    ui32 nbr_faces=0, face_stride;
    float3 color;
    
    poly_collider_t(): collider_t(POLYHEDRON) { }
    
    virtual void vertex_shade(const mat3f &R, const float3 &X)
    {
        p_w = X + R * this->X;
        
        for (int i=0; i<vertices.size(); i++)
            vertices_w[i] = X + R * (this->X + vertices[i]);
        
        for (int i=0; i<nbr_faces; i++)
            normals_w[i] = R * normals[i];
        
        // World space AABB
        AABB_w = (AABB + this->X).post_transform(X, R);
    }
    
    // Compute local space AABB
    //
    void compute_AABB()
    {
        AABB.reset();
        
        for (auto& v : vertices)
            AABB.grow(v);
    }
    
private:
    
    // Compute a Steiner point
    //
    // In this context = a point inside the polyhedron
    // (which may or may not be the center of mass) [Ericsson]
    //
    vec3f compute_Steiner_point()
    {
        vec3f sp = vec3f(0,0,0);
        for (vec3f &v : vertices)
            sp += v;
        
        sp *= (1.0f/vertices.size());
        
        return sp;
    }
    
    //
    // Möller-Trumbore ray-triangle intersection
    //
    // http://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
    //
    bool intersect_triangle(const float3 &v0, const float3 &v1, const float3 &v2, ray_t &ray)
    {
#define EPSILON 0.000001
        float3 e1 = v1-v0;
        float3 e2 = v2-v0;
        
        float3 P = ray.dir % e2;
        float det = e1.dot(P);
        if (fabs(det) < EPSILON) return false;
        
        float inv_det = 1.0f/det;
        float3 T = ray.origo - v0;
        float u = T.dot(P) * inv_det;
        if (u < 0.0f || u > 1.0f) return false;
        
        float3 Q = T % e1;
        float v = ray.dir.dot(Q) * inv_det;
        if (v < 0.0f || u+v > 1.0f) return false;
        
        float t = e2.dot(Q) * inv_det;
        if (t > EPSILON && t < ray.znear)
        {
            ray.znear = t;
            return true;
        }
        return false;
    }
    
public:
    
    /*
     * intersect polyhedron with ray: triangulate face polygons and intersect each individual triangle
     */
    bool intersect(ray_t &ray)
    {
        bool hit = false;
        for (int i=0; i<nbr_faces; i++)
            for (int j=1; j<face_stride-1; j++)
            {
                float3 v0 = vertices_w[ faces[i*face_stride + 0]    ];
                float3 v1 = vertices_w[ faces[i*face_stride + j]    ];
                float3 v2 = vertices_w[ faces[i*face_stride + j+1]  ];
                
                hit |= intersect_triangle(v0, v1, v2, ray);
            }
        return hit;
    }
    
    void render()
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(X.x, X.y, X.z);
        
        for (int i=0; i<nbr_faces; i++)
        {
#if 0
            // face surfaces
            glBegin(GL_POLYGON);
            for (int j=0; j<face_stride; j++) {
                glNormal3fv(normals[i].vec);
                glVertex3fv(vertices[faces[i*face_stride+j]].vec);
            }
            glEnd();
#endif
#if 0
            // face normal
            glBegin(GL_LINES);
            glVertex3fv(vertices[faces[i*face_stride+0]].vec);
            glVertex3fv((vertices[faces[i*face_stride+0]]+normals[i]).vec);
            glEnd();
#endif
#if 1
            // face outline
            glColor3f(1, 0, 1);
            glLineWidth(1);
            glBegin(GL_LINE_LOOP);
            for (int j=0; j<face_stride; j++)
                glVertex3fv(vertices[faces[i*face_stride+j]].vec);
            glEnd();
#endif
#if 0
            // com
            render_marker(vec3(0,0,0), 1.0, 1, vec4(0,0,0,1));
#endif
        }
#if 0
        // edges
        for (int i=0;i<edges.size(); i+=2)
        {
            glBegin(GL_LINES);
            glVertex3fv(vertices[edges[i+0]].vec);
            glVertex3fv((vertices[edges[i+1]]).vec);
            glEnd();
        }
#endif
        
        glPopMatrix();
    }
};

class box_collider_t : public poly_collider_t
{
public:
    
    box_collider_t(const float3 &size, const mat3f &R_local = mat3f(1)) : box_collider_t(size, size, R_local) { }
    
    box_collider_t(const float3 &size_front, const float3 &size_back, const mat3f &R_local = mat3f(1))
    {
        // vertices
        for (int i=0; i<unit_box_nbr_vertices/2; i++)
            vertices.push_back( R_local*(unit_box_va[i]*size_front) );
        for (int i=unit_box_nbr_vertices/2; i<unit_box_nbr_vertices; i++)
            vertices.push_back( R_local*(unit_box_va[i]*size_back) );
        
        // faces
        for (int i=0; i<unit_box_nbr_faces*unit_box_face_stride; i++)
            faces.push_back(unit_box_faces[i]);
        nbr_faces = unit_box_nbr_faces;
        face_stride = unit_box_face_stride;
        
        // edges
        edges.assign(&unit_box_edges[0], &unit_box_edges[ sizeof(unit_box_edges)/sizeof(ui32) ]);
        unique_edge_dirs.assign(&unit_box_unique_edge_dirs[0], &unit_box_unique_edge_dirs[ sizeof(unit_box_unique_edge_dirs)/sizeof(ui32) ]);
        
        // normals
        for (int i=0; i<unit_box_nbr_faces; i++)
        {
            float3 v0 = vertices[faces[i*unit_box_face_stride+0]];
            float3 v1 = vertices[faces[i*unit_box_face_stride+1]];
            float3 v2 = vertices[faces[i*unit_box_face_stride+2]];
            float3 n = (v1 - v0) % (v2 - v0);
            n.normalize();
            normals.push_back(n);
        }
        
        // random color
        color = float3(rnd(0.2f, 0.8f), rnd(0.2f, 0.8f), rnd(0.2f, 0.8f));
        
        // world space vertices
        vertices_w.resize(vertices.size());
        normals_w.resize(normals.size());
        
        compute_AABB();
    }
};

class wedge_collider_t : public poly_collider_t
{
public:
    
    wedge_collider_t(const float3 &size, const mat3f &R_local = mat3f(1)) : wedge_collider_t(size, size, R_local) { }
    
    wedge_collider_t(const float3 &size_front, const float3 &size_back, const mat3f &R_local = mat3f(1))
    {
        // vertices
        for (int i=0; i<wedge_nbr_vertices/2; i++)
            vertices.push_back( R_local*(wedge_va[i]*size_front) );
        for (int i=wedge_nbr_vertices/2; i<wedge_nbr_vertices; i++)
            vertices.push_back( R_local*(wedge_va[i]*size_back) );
        
        // faces
        for (int i=0; i<wedge_nbr_faces*wedge_face_stride; i++)
            faces.push_back(wedge_faces[i]);
        nbr_faces = wedge_nbr_faces;
        face_stride = wedge_face_stride;
        
        // edges
        edges.assign(&wedge_edges[0], &wedge_edges[ sizeof(wedge_edges)/sizeof(ui32) ]);
//        unique_edge_dirs.assign(&wedge_edges[0], &wedge_edges[ sizeof(wedge_edges)/sizeof(ui32) ]);
        unique_edge_dirs.assign(&wedge_unique_edge_dirs[0], &wedge_unique_edge_dirs[ sizeof(wedge_unique_edge_dirs)/sizeof(ui32) ]);
        
        // normals
        for (int i=0; i<wedge_nbr_faces; i++)
        {
            float3 v0 = vertices[faces[i*wedge_face_stride+0]];
            float3 v1 = vertices[faces[i*wedge_face_stride+1]];
            float3 v2 = vertices[faces[i*wedge_face_stride+2]];
            float3 n = (v1 - v0) % (v2 - v0);
            n.normalize();
            normals.push_back(n);
        }
        
        // random color
        color = float3(rnd(0.2f, 0.8f), rnd(0.2f, 0.8f), rnd(0.2f, 0.8f));
        
        // world space vertices
        vertices_w.resize(vertices.size());
        normals_w.resize(normals.size());
        
        compute_AABB();
    }
};

class tetrahedron_collider_t : public poly_collider_t
{
    // todo
};

class octagon_collider_t : public poly_collider_t
{
    // todo
};

class sphere_collider_t : public collider_t
{
public:
    float3 p_w;
    float r;
    float3 color;
    
    sphere_collider_t(float r) : collider_t(SPHERE), r(r)
    {
        compute_AABB();
    }
    
    // Compute of local space AABB
    //
    void compute_AABB()
    {
        AABB.reset();
        
        vec3f v = vec3f(1,1,1);
        AABB.grow(v*r);
        AABB.grow(v*-r);
    }
    
    void vertex_shade(const mat3f &R, const float3 &X)
    {
        p_w = X + R*this->X;
        
        AABB_w = AABB + X + this->X;
    }
    
    /* 
     * line-sphere intersection
     * http://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
     */
    bool intersect(ray_t &ray)
    {
        float loc = ray.dir.dot(ray.origo-p_w);
        float oc = (ray.origo-p_w).norm2();
        float sr = loc*loc - oc*oc + r*r;
        if (sr>=0)
        {
            // hit; is it closer than previous hit?
            float d = -loc - sqrt(sr);
            if (!ray || d<ray.znear) {
                ray.znear = d;
                return true;
            }
        }
        return false;
    }
    
    void render()
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(X.x, X.y, X.z);
        
        glColor3f(1, 0, 1);
        glutSolidSphere(r, 8, 8);
        
        glPopMatrix();
    }
};

class cylinder_collider_t : public collider_t
{
public:
    float r, l;
    
    cylinder_collider_t() : collider_t(CYLINDER)
    {
    }
    
    void vertex_shade(const mat3f &R, const float3 &X) { }
    
    /*
     cylinder –  ray intersection
     
     [Ericsson p194]
     */
    bool intersect(ray_t &ray) { return false; }

};

class plane_collider_t : public collider_t
{
public:
    float3 n;         // normal in local space
    float3 p_w, n_w;  // position and normal in world space
    
    plane_collider_t(float3 &n) : collider_t(PLANE), n(n) {}
    
    virtual void vertex_shade(const mat3f &R, const float3 &X)
    {
        p_w = X + R*this->X;
        n_w = R*n;
    }
    
    void render()
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(X.x, X.y, X.z);
        
        glColor3f(0,0.5,0);
        glutSolidSphere(0.1, 4, 4);
        render_line(float3(0, 0, 0), n_w, 1, float4(0,0.5,0,1));
        
        glPopMatrix();
    }
};

class point_collider_t : public collider_t
{
public:
    float3 p_w;  // position in world space
    vec3f color;
    
    point_collider_t() : collider_t(POINT)
    {
        compute_AABB();
    }
    
    virtual void vertex_shade(const mat3f &R, const float3 &X) override
    {
        p_w = X;
        
        AABB_w = AABB + X;
    }
    
    void compute_AABB() override
    {
        AABB.reset();
        AABB.grow({0,0,0}); // point is infitesimal; rely on AABB precision epsilon for size.
        AABB.grow({0.01,0.01,0.01});
        AABB.grow({-0.01,-0.01,-0.01});
    }
    
    void render()
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(X.x, X.y, X.z);
        
        glColor3f(1, 0, 1);
        glutSolidSphere(1, 8, 8);
        
        glPopMatrix();
    }
};

/*
static std::vector<geometry_t*> polycolliders_from_obj_decomposition(std::string filename)
{
    // load geometries from arbitrary obj
    // convex decompsition? [Ericson]
};
 */

static std::vector<collider_t*> polycolliders_from_obj(const std::string& filename,
                                                       std::vector<collider_t*>& colliders,
                                                       std::vector<std::string>* collider_names = nullptr,
                                                       const mat4f& M = linalg::mat4f_identity,
                                                       const std::string& group_prefix = "__collider")
{
//    std::vector<collider_t*> colliders;
    
    mesh_t mesh;
    mesh.load_obj(filename, false, false, true, "");
    
    // edge indices
    struct uint2 {
        unsigned i,j;
        
        // allow bidirectionality, (i,j)=(i,j) and (i,j)=(j,i)
        bool operator == (const uint2& rhs) const {
            return (i==rhs.i && j==rhs.j) || (i==rhs.j && j==rhs.i);
        }
    };
    
    for(drawcall_t &dc : mesh.drawcalls)
    {
        // see if this drawcall group is prefixed as a collider
        if (dc.group_name.compare(0, group_prefix.length(), group_prefix))
            continue;
        
        if (!dc.tris.size() && !dc.quads.size())
            continue;
            //throw runtime_error("polycolliders_from_obj error: drawcall was empty\n");
        
        if (dc.tris.size() && dc.quads.size())
//            continue;
            throw runtime_error("polycolliders_from_obj error: drawcall contains both triangles and quads\n");
        
        // source vertex index to welded index hash
        poly_collider_t* g = new poly_collider_t();
        std::unordered_map<unsigned, unsigned> index_hash;
        std::vector<uint2> edge_hash;
//        std::unordered_map<uint2, unsigned, uint2_hashfunction> edge_hash;
        
        if (dc.tris.size())
        {
            // weld triangles
            
            g->face_stride = 3;
            g->nbr_faces = (unsigned)dc.tris.size();
            g->X = {0, 0, 0};
            
            for (triangle_t &tri : dc.tris)
            {
                for (int i=0; i<3; i++)
                {
                    // vertex
                    unsigned vi = tri.vi[i];
                    auto s = index_hash.find(vi);
                    if (s == index_hash.end())
                    {
                        // index-combo does not exist, create it
                        g->faces.push_back((unsigned)g->vertices.size());
                        index_hash[vi] = (unsigned)g->vertices.size();
                        
                        g->vertices.push_back(mesh.vertices[vi].vp); //printf("Y");
                    }
                    else
                    {
                        // use existing index-combo
                        g->faces.push_back(s->second); //printf("X");
                    }
                }
            }
            
            // weld tri's edges
            for (triangle_t &tri : dc.tris)
                for (int i=0; i<3; i++)
                {
                    // hash out the welded indices for the edge
                    uint2 edge = { index_hash[tri.vi[i]], index_hash[tri.vi[(i+1)%3]] };
                    auto s = std::find(edge_hash.begin(), edge_hash.end(), edge);
                    if (s == edge_hash.end())
                    {
                        g->edges.push_back(edge.i);
                        g->edges.push_back(edge.j);
                        edge_hash.push_back(edge);
                    }
                }
        }
        else
        {
            // weld quads

            g->face_stride = 4;
            g->nbr_faces = (unsigned)dc.quads.size();
            g->X = {0, 0, 0};
            
            for (quad_t_ &quad : dc.quads)
            {
                for (int i=0; i<4; i++)
                {
                    // vertex
                    unsigned vi = quad.vi[i];
                    auto s = index_hash.find(vi);
                    if (s == index_hash.end())
                    {
                        // index-combo does not exist, create it
                        g->faces.push_back((unsigned)g->vertices.size());
                        index_hash[vi] = (unsigned)g->vertices.size();
                        
                        g->vertices.push_back(mesh.vertices[vi].vp); //printf("Y");
                    }
                    else
                    {
                        // use existing index-combo
                        g->faces.push_back(s->second); //printf("X");
                    }
                }
            }
            
            // weld quad's edges
            for (quad_t_ &quad : dc.quads)
                for (int i=0; i<4; i++)
                {
                    // hash out the welded indices for the edge
                    uint2 edge = { index_hash[quad.vi[i]], index_hash[quad.vi[(i+1)%4]] };
                    auto s = std::find(edge_hash.begin(), edge_hash.end(), edge);
                    if (s == edge_hash.end())
                    {
                        g->edges.push_back(edge.i);
                        g->edges.push_back(edge.j);
                        edge_hash.push_back(edge);
                    }
                }
        }
        
        // unique edges
        g->unique_edge_dirs = g->edges;
        
        // Custom transform
        for (auto& v : g->vertices)
        {
            v = ( M * v.xyz1() ).xyz();
        }
        
        // normals
        for (int i=0; i<g->nbr_faces; i++)
        {
            vec3f v0 = g->vertices[g->faces[i*g->face_stride+0]];
            vec3f v1 = g->vertices[g->faces[i*g->face_stride+1]];
            vec3f v2 = g->vertices[g->faces[i*g->face_stride+2]];
            vec3f n = normalize((v1-v0)%(v2-v0));
            g->normals.push_back(n);
        }
        
        // allocate world space vertices
        g->vertices_w.resize(g->vertices.size());
        g->normals_w.resize(g->normals.size());
        
        g->compute_AABB();
        
        colliders.push_back(g);
        if (collider_names)
            collider_names->push_back(dc.group_name);
    }
    
    return colliders;
}

#endif
