//
//  body_t.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2012-05-04.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef T3_BODY_H
#define T3_BODY_H

#include "tau3d.h"
#include "vec.h"
#include "mat.h"
#include "ray.h"
#include "config.h"
#include "collider.h"
#include "mass_properties.h"
#include "collider_data.h"
#include "aabb.h"
#include <vector>
#include <string>

#ifdef _WIN32
#include "../GL/glut.h"
#elif __APPLE__
#include "GLUT/glut.h"
#endif

//using linalg::vec3f;
//using linalg::vec4f;
//using linalg::mat3f;
//using lin
//using linalg::quatf;
using namespace linalg;


class body_t
{
public:
        
    // Mass state
	float   m, im, rho;     // mass / inverse mass (aux) / density (aux)
	mat3f   I, iI, iI_w;    // inertia tensor / inverse (aux) / inverse world (aux)
    // Linear state
	vec3f   X, V, F;        // position / velocity / force accumulator
    vec3f   X_r;            // anchor point of initial X wrt to com (X), when computed
    // Angular state
    quatf   Q;              // orientation (quaternion)
	mat3f   R, Ri;          // orientation (matrix) / inverse (aux)
	vec3f   W, T;           // angular velocity / torque accumulator

    // Constitutive state
    float restitution = REST_DEFAULT;   // Coefficient of restitution
    float my_s = MY_S_DEFAULT;          // Static friction
    float my_d = MY_D_DEFAULT;          // Dynamic friction
    
    bool is_static;      // static bodies have constant state
    std::string id;
    std::vector<collider_t*> colliders;
    AABB_t AABB_w;
    
    body_t(const vec3f &p, const std::string &id = "") :
        X(p), X_r(0,0,0), V(0,0,0), F(0,0,0),
        R(1), Ri(1), Q(),
        W(0,0,0), T(0,0,0),
        m(1), im(1), rho(1),
        I(1), iI(1), iI_w(1),
        is_static(false),
        id(id)
    {
        
    }
    
    void set_rotation(const mat3f& R)
    {
        this->R = linalg::mat3f_identity;
        apply_rotation(R);
    }
    
    void apply_rotation(const mat3f& R)
    {
        this->R = R * this->R;
        Ri = transpose(this->R);
        
        Q = quatf(this->R);
        Q.normalize();
    }
    
    void apply_impulse(const vec3f& p, const vec3f& r)
    {
        if (is_static) return;
        
        // linear
        V += p * im;
        // angular
        W += iI_w * (r % p);
    }
    
    void apply_angular_impulse(const vec3f& l)
    {
        if (is_static) return;
        
        W += iI_w * l;
    }
    
    void apply_force(const vec3f& f, const vec3f& r)
    {
        if (is_static) return;
        
        F += f;
        T += r % f;
    }
    
    void apply_torque(const vec3f& t)
    {
        if (is_static) return;
        
        // Check this
        T += t;
    }
    
    // make static
    //
    void make_static()
    {
        m = std::numeric_limits<float>::infinity();
        I = mat3f( std::numeric_limits<float>::infinity() );
        
        im = 0.0f;
        iI = mat3f(0.0f);
        
        is_static = true;
    }
    
    // set linear and angular mass
    //
    void set_mass(float m, const mat3f &I)
    {
        this->m = m;
        this->I = I;
        
        im = 1.0f/m;
        iI = I.inverse();
    }
    
    //
    // Compute from colliders: center-of-mass (COM), mass, inertia tensor.
    //
    // Offsets body, geometries and initial position anchor to the new COM
    //
    void set_mass_attribs_from_colliders(float rho = RHO_DEFAULT)
    {
        // Aggregate attribs from colliders (local space)
        mass_properties_t body_attribs = body_mass_properties(colliders, rho);
        
        // Update mass
        set_mass(body_attribs.m, body_attribs.I);

        // Offset colliders wrt to new local COM
        for (collider_t *g : colliders)
            g->X -= body_attribs.com;
        
        // Offset original position wrt new local COM
        X_r = -body_attribs.com;
        //X_r -= body_attribs.com;  // May err when this function is called
                                    // consequently
        
        // Align body COM to new COM
        X += R * body_attribs.com;
        
        this->rho = rho;
        // DBG
//        printf("created body\nI =\n"); I.debugPrint();
//        printf("attribs com "); body_attribs.com.debugPrint();
    }

    void vertex_shade()
    {
        // World space inverted inertia tensor
        iI_w =  R*iI*Ri;
        
        // Transform colliders and AABB to world space
        AABB_w.reset();
        for (auto c : colliders)
        {
            c->vertex_shade(R, X);
            AABB_w.grow(c->AABB_w);
        }
        
        
    }
    
    /* 
     test geometries for intersection
     
     if there is a hit:
     create an anchor point by transforming the hit point from world->body space and attach it to the ray
     */
    void intersect_colliders(ray_t &ray)
    {
        for (collider_t *geometry : colliders)
        {
            // see if geometry was hit
            if (geometry->intersect(ray))
            {
                // set body-pointer to this body
                ray.body = this;
                
                // hit point in world space
                vec3f p_hit_w = ray.origo + ray.dir*ray.znear;
                
                // transform hit point from world to local body space using inverse of body transform
                // body->world: T(X)*R =>
                // body<-world: Ri*Ti = Ri*T(-X)
                ray.r =  Ri*(p_hit_w - X);
                // long version
                //ray.r = Ri*(mat4f::translation(-X)*p_hit_w.xyz1()).xyz();
            }
        }
    }
    
	void render()
    {
		float theta = Q.getEulerAngle();
		vec3f ea = Q.getEulerAxis(theta);
        
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glTranslatef(X.x, X.y, X.z);
		glRotatef(fTO_DEG*theta, ea.x, ea.y, ea.z);
#if 1
        // com
        render_marker(vec3f(0,0,0), 0.2, 1, vec4f(0,0,0,1));
#endif
#if 0
        // local axes
        glColor3f(1, 0, 0);
        glBegin(GL_LINES);
        glVertex3f(0, 0, 0);
        glVertex3f(1, 0, 0);
        glEnd();
        glColor3f(0, 1, 0);
        glBegin(GL_LINES);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 1, 0);
        glEnd();
        glColor3f(0, 0, 1);
        glBegin(GL_LINES);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 0, 1);
        glEnd();
#endif
        // geometries
        for (int i=0; i<colliders.size(); i++) colliders[i]->render();
        
		glPopMatrix();
    };
    
    ~body_t(void)
    {
        for (int i=0; i<colliders.size(); i++) delete colliders[i];
    }
};


//
// primitive bodies
//


class box_body_t : public body_t
{
public:
    
    box_body_t(const vec3f &p, const vec3f &size, const std::string& id = "", const mat3f &R_local = mat3f(1)) : box_body_t(p, size, size, id, R_local) { }
    
	box_body_t(const vec3f &p, const vec3f &size_front, const vec3f &size_back, const std::string& id = "", const mat3f &R_local = mat3f(1)) : body_t(p, id)
	{
        box_collider_t *pgeom = new box_collider_t(size_front, size_back, R_local);

        colliders.push_back(pgeom);
        set_mass_attribs_from_colliders();

#if 0
        // mass
		f32 rho = 25.0f;
		m = rho*size.x*size.y*size.z;
		im = 1.0f/m;
        
		// box I
		I.m11 = m*(size.y*size.y + size.z*size.z)/12.0f;
		I.m22 = m*(size.x*size.x + size.z*size.z)/12.0f;
		I.m33 = m*(size.x*size.x + size.y*size.y)/12.0f;
		iI.m11 = 1.0f/I.m11;
		iI.m22 = 1.0f/I.m22;
		iI.m33 = 1.0f/I.m33;
#endif
#if 0
        printf("\nauto-computed attribs:\n");
        printf("m = %f\n", m);
        printf("com = "); X.debugPrint();
        printf("I = \n"); I.debugPrint();
        
        printf("manually-computed attribs:\n");
        float m_ = RHO_DEFAULT*size.x*size.y*size.z;
        printf("m = %f\n", m_);
        //im = 1.0f/m;
        mat3 I_ = mat3(1);
        I_.m11 = m*(size.y*size.y + size.z*size.z)/12.0f;
        I_.m22 = m*(size.x*size.x + size.z*size.z)/12.0f;
        I_.m33 = m*(size.x*size.x + size.y*size.y)/12.0f;
        printf("I = \n"); I_.debugPrint();
//        mat3 Idiag = I, rot; Idiag.diagonalize(rot, 0.00001, 20);
//        printf("rot\n"); Idiag.debugPrint();
#endif
        
	}
};


class sphere_t : public body_t
{
public:
    sphere_t(const vec3f &p, float r, const std::string& id = "") : body_t(p, id)
    {
        sphere_collider_t *sg = new sphere_collider_t(r);
        colliders.push_back(sg);
        
        // color
        sg->color = vec3f(rnd(0.2f, 0.6f), rnd(0.2f, 0.6f), rnd(0.2f, 0.6f));
        
        set_mass_attribs_from_colliders();
        
        //        // sphere mass = density * volume = rho * 4PIr^3/3
        //        m = RHO_DEFAULT*4*fPI*r*r*r/3;
        //        im = 1.0f/m;
        //
        //        // sphere moment of inertia, I = 2mr^2/5 * 3x3_identity, Ii = 5/2mr^2 * 3x3_dentity
        //        I.m11 = I.m22 = I.m33 = 2.0f*m*r*r/5.0f;
        //        iI.m11 = iI.m22 = iI.m33 = 5.0f/(2.0f*m*r*r);
    }
};


class plane_body_t : public body_t
{
public:
    plane_body_t(vec3f p, vec3f n) : body_t(p)
    {
        plane_collider_t *pg = new plane_collider_t(n);
        colliders.push_back(pg);
        
        make_static();
    }
};

class point_body_t : public body_t
{
public:
    point_body_t(const vec3f &p, float mass, const std::string& id = "") : body_t(p, id)
    {
        point_collider_t *pc = new point_collider_t();
        colliders.push_back(pc);
        
        // color
        pc->color = vec3f(rnd(0.2f, 0.6f), rnd(0.2f, 0.6f), rnd(0.2f, 0.6f));
        
        // density from mass, assuming point is a unit sphere:
        float rho = (3.0f*mass)/(4.0f * 3.14159f);
        set_mass_attribs_from_colliders(rho);
        
        //        // sphere mass = density * volume = rho * 4PIr^3/3
        //        m = RHO_DEFAULT*4*fPI*r*r*r/3;
        //        im = 1.0f/m;
        //
        //        // sphere moment of inertia, I = 2mr^2/5 * 3x3_identity, Ii = 5/2mr^2 * 3x3_dentity
        //        I.m11 = I.m22 = I.m33 = 2.0f*m*r*r/5.0f;
        //        iI.m11 = iI.m22 = iI.m33 = 5.0f/(2.0f*m*r*r);
    }
};

class wedge_t : public body_t
{
public:
    
    wedge_t(const vec3f &p, const vec3f &size, const std::string& id = "", const mat3f &R_local = mat3f(1)) : wedge_t(p, size, size, id, R_local) { }
    
    wedge_t(const vec3f &p, const vec3f &size_front, const vec3f &size_back, const std::string& id = "", const mat3f &R_local = mat3f(1)) : body_t(p, id)
    {
        wedge_collider_t *wgeom = new wedge_collider_t(size_front, size_back, R_local);
        
        colliders.push_back(wgeom);
        set_mass_attribs_from_colliders();
    }
};


class tetrahedron_t : public body_t
{
public:
    
    tetrahedron_t(vec3f p, vec3f size) : body_t(p)
    {
        // vertices
        poly_collider_t *pgeom = new poly_collider_t();
        for (int i=0; i<tetrahedron_va_size; i++)
            pgeom->vertices.push_back(tetrahedron_va[i]*size);
        
        // faces
        for (int i=0; i<tetrahedron_faces_size*tetrahedron_face_stride; i++)
            pgeom->faces.push_back(tetrahedron_faces[i]);
        
        pgeom->nbr_faces = tetrahedron_faces_size;
        pgeom->face_stride = tetrahedron_face_stride;
        
        // edges
        
        pgeom->edges.assign(&tetrahedron_edges[0], &tetrahedron_edges[ sizeof(tetrahedron_edges)/sizeof(ui32) ]);
        pgeom->unique_edge_dirs.assign(&tetrahedron_edges[0], &tetrahedron_edges[ sizeof(tetrahedron_edges)/sizeof(ui32) ]);
        
        // normals
        for (int i=0; i<tetrahedron_faces_size; i++)
        {
            vec3f v0 = pgeom->vertices[pgeom->faces[i*tetrahedron_face_stride+0]];
            vec3f v1 = pgeom->vertices[pgeom->faces[i*tetrahedron_face_stride+1]];
            vec3f v2 = pgeom->vertices[pgeom->faces[i*tetrahedron_face_stride+2]];
            vec3f n = (v1 - v0) % (v2 - v0);
            n.normalize();
            pgeom->normals.push_back(n);
        }
        
        // color
        pgeom->color = vec3f(rnd(0.2f, 0.8f), rnd(0.2f, 0.8f), rnd(0.2f, 0.8f));
        
        // world space vertices
        pgeom->vertices_w.resize(pgeom->vertices.size());
        pgeom->normals_w.resize(pgeom->normals.size());
        
        pgeom->compute_AABB();
        
        colliders.push_back(pgeom);
        set_mass_attribs_from_colliders();
    }
};


// todo: make a geometry/collider class which loads the geometry
//
class octagon_t : public body_t
{
public:
    
    octagon_t(vec3f p, vec3f size, const std::string& id = "") : body_t(p, id)
    {
        // vertices
        poly_collider_t *pgeom = new poly_collider_t();
        for (int i=0; i<octagon_va_size; i++)
            pgeom->vertices.push_back(octagon_va[i]*size);
        
        // faces
        for (int i=0; i<octagon_faces_size*octagon_face_stride; i++)
            pgeom->faces.push_back(octagon_faces[i]);
        
        pgeom->nbr_faces = octagon_faces_size;
        pgeom->face_stride = octagon_face_stride;
        
        // edges
        
        pgeom->edges.assign(&octagon_edges[0], &octagon_edges[ sizeof(octagon_edges)/sizeof(ui32) ]);
        pgeom->unique_edge_dirs.assign(&octagon_edges[0], &octagon_edges[ sizeof(octagon_edges)/sizeof(ui32) ]);
        
        // normals
        for (int i=0; i<octagon_faces_size; i++)
        {
            vec3f v0 = pgeom->vertices[pgeom->faces[i*octagon_face_stride+0]];
            vec3f v1 = pgeom->vertices[pgeom->faces[i*octagon_face_stride+1]];
            vec3f v2 = pgeom->vertices[pgeom->faces[i*octagon_face_stride+2]];
            vec3f n = (v1 - v0) % (v2 - v0);
            n.normalize();
            pgeom->normals.push_back(n);
        }
        
        // color
        pgeom->color = vec3f(rnd(0.2f, 0.8f), rnd(0.2f, 0.8f), rnd(0.2f, 0.8f));
        
        // world space vertices
        pgeom->vertices_w.resize(pgeom->vertices.size());
        pgeom->normals_w.resize(pgeom->normals.size());

        pgeom->compute_AABB();
        
        colliders.push_back(pgeom);
        set_mass_attribs_from_colliders();
    }
};

// todo: make a geometry/collider class which loads the geometry
//
class flattriangle_t : public body_t
{
public:
    
    flattriangle_t(vec3f p, vec3f size) : body_t(p)
    {
        // vertices
        poly_collider_t *pgeom = new poly_collider_t();
        for (int i=0; i<triangle_va_size; i++)
            pgeom->vertices.push_back(triangle_va[i]*size);
        
        // faces
        for (int i=0; i<triangle_faces_size*triangle_face_stride; i++)
            pgeom->faces.push_back(triangle_faces[i]);
        
        pgeom->nbr_faces = triangle_faces_size;
        pgeom->face_stride = triangle_face_stride;
        
        // edges
        
        pgeom->edges.assign(&triangle_edges[0], &triangle_edges[ sizeof(triangle_edges)/sizeof(ui32) ]);
        pgeom->unique_edge_dirs.assign(&triangle_edges[0], &triangle_edges[ sizeof(triangle_edges)/sizeof(ui32) ]);
        
        // normals
//        pgeom->normals.push_back({0,0,1});
//        pgeom->normals.push_back({0,-1,0});
//        pgeom->normals.push_back(vec3f(1,1,0).normalize());
//        pgeom->normals.push_back({-1,0,0});
        
        for (int i=0; i<triangle_faces_size; i++)
        {
            vec3f v0 = pgeom->vertices[pgeom->faces[i*triangle_face_stride+0]];
            vec3f v1 = pgeom->vertices[pgeom->faces[i*triangle_face_stride+1]];
            vec3f v2 = pgeom->vertices[pgeom->faces[i*triangle_face_stride+2]];
            vec3f n = (v1 - v0) % (v2 - v0);
            n.normalize();
            pgeom->normals.push_back(n);
        }
        
        // color
        pgeom->color = vec3f(rnd(0.2f, 0.8f), rnd(0.2f, 0.8f), rnd(0.2f, 0.8f));
        
        // world space vertices
        pgeom->vertices_w.resize(pgeom->vertices.size());
        pgeom->normals_w.resize(pgeom->normals.size());
        
        colliders.push_back(pgeom);
        
        // set mass properties as if though a unit sphere (zero volume so zero mass)
        float r = 0.5;
        float r2 = r*r, r3 = r2*r;
        float m = RHO_DEFAULT*4*fPI*r3/3;
        mat3f I = mat3f(0);
        I.m11 = I.m22 = I.m33 = 2.0f*m*r2/5.0f;
        
        float h = size.y, w = size.x, d = size.z;
        m = h*w*d*RHO_DEFAULT;
        I.m11 = 1.0f/12*m*(h*h + d*d);
        I.m22 = 1.0f/12*m*(w*w + d*d);
        I.m33 = 1.0f/12*m*(w*w + h*h);
        
        set_mass(m, I);
    }
};

//
// compound bodies
//


class obj_body_t : public body_t
{
public:
    obj_body_t(const vec3f &p,
               const std::string& filename,
               const std::string& id,
               const mat4f& M = linalg::mat4f_identity,
               float rho = RHO_DEFAULT)
    : body_t(p, id)
    {
        polycolliders_from_obj(filename, colliders, nullptr, M);
        
        set_mass_attribs_from_colliders(rho);
    }
};


class chair_t : public body_t
{
public:
    
    chair_t(const vec3f &p, const vec3f &size, const std::string& id = "") : body_t(p, id)
    {
        vec3f rv = vec3f(-1,1,1).normalize();
        
        box_collider_t *leg1 = new box_collider_t({0.1,1,0.1}, mat3f::rotation(0*-3.14/4, rv.x, rv.y, rv.z));
        box_collider_t *leg2 = new box_collider_t({0.1,2,0.1});
        box_collider_t *leg3 = new box_collider_t({0.1,2,0.1});
        box_collider_t *leg4 = new box_collider_t({0.1,1,0.1});
        box_collider_t *seat = new box_collider_t({1.1,0.1,1.1});
        box_collider_t *rest = new box_collider_t({1.1,0.1,0.1});
        
        leg1->X = {0.5,0.5,0.5};
        leg2->X = {0.5,1,-0.5};
        leg3->X = {-0.5,1,-0.5};
        leg4->X = {-0.5,0.5,0.5};
        seat->X = {0,1,0};
        rest->X = {0,2,-0.5};
        
        colliders.push_back(leg1);
        colliders.push_back(leg2);
        colliders.push_back(leg3);
        colliders.push_back(leg4);
        colliders.push_back(seat);
        colliders.push_back(rest);
        
        set_mass_attribs_from_colliders();
    }
};


class openbox_body_t : public body_t
{
public:
    
    openbox_body_t(const vec3f &p, const vec3f &size, float thickness, const std::string& id = "") : body_t(p, id)
    {
        vec3f hs = size;//*0.5;
        box_collider_t *right = new box_collider_t({thickness,hs.y,hs.z});
        box_collider_t *left = new box_collider_t({thickness,hs.y,hs.z});
        box_collider_t *front = new box_collider_t({hs.x,hs.y,thickness});
        box_collider_t *back = new box_collider_t({hs.x,hs.y,thickness});
        box_collider_t *bottom = new box_collider_t({hs.x,thickness,hs.z});

        right->X = {hs.x/2-thickness/2,hs.y/2+thickness,0};
        left->X = {-hs.x/2+thickness/2,hs.y/2+thickness,0};
        front->X = {0,hs.y/2+thickness,hs.z/2-thickness/2};
        back->X = {0,hs.y/2+thickness,-hs.z/2+thickness/2};
        bottom->X = {0,thickness,0};
//        right->X = {0.535,0.05,0};
//        left->X = {-0.535,0.05,0};
//        front->X = {0,0.05,0.35};
//        back->X = {0,0.05,-0.35};
//        bottom->X = {0,-0.25,0};
        
        colliders.push_back(right);
        colliders.push_back(left);
        colliders.push_back(front);
        colliders.push_back(back);
        colliders.push_back(bottom);
        
        set_mass_attribs_from_colliders();
    }
};


class openbox_body_t_ : public body_t
{
public:
    
    openbox_body_t_(const vec3f &p, const vec3f &size, const std::string& id = "") : body_t(p, id)
    {
        box_collider_t *right = new box_collider_t({0.15,0.6,0.8});
        box_collider_t *left = new box_collider_t({0.15,0.6,0.8});
        box_collider_t *front = new box_collider_t({1.2,0.6,0.15});
        box_collider_t *back = new box_collider_t({1.2,0.6,0.15});
        box_collider_t *bottom = new box_collider_t({1.225,0.15,0.825});
        
        right->X = {0.535,0.05,0};
        left->X = {-0.535,0.05,0};
        front->X = {0,0.05,0.35};
        back->X = {0,0.05,-0.35};
        bottom->X = {0,-0.25,0};
        
        colliders.push_back(right);
        colliders.push_back(left);
        colliders.push_back(front);
        colliders.push_back(back);
        colliders.push_back(bottom);
        
        set_mass_attribs_from_colliders();
    }
};


class hand_t : public body_t
{
public:
    
    hand_t(const vec3f &p, const vec3f &size, const std::string& id = "") : body_t(p, id)
    {
        vec3f rv = vec3f(-1,1,1).normalize();
        
        //        box_collider_t *leg1 = new box_collider_t({0.2,1,0.2}, mat3::rotation(0*-3.14/4, rv.x, rv.y, rv.z));
        //        box_collider_t *leg2 = new box_collider_t({0.2,2,0.2});
        //        box_collider_t *leg3 = new box_collider_t({0.2,2,0.2});
        //        box_collider_t *leg4 = new box_collider_t({0.2,1,0.2});
        //
        //        box_collider_t *seat = new box_collider_t({1,0.2,1});
        //        box_collider_t *rest = new box_collider_t({1,0.2,0.2});
        //
        //        leg1->X = {0.5,0.5,0.5};
        //        leg2->X = {0.5,1,-0.5};
        //        leg3->X = {-0.5,1,-0.5};
        //        leg4->X = {-0.5,0.5,0.5};
        //        rest->X = {0,2,-0.5};
        //        seat->X = {0,1,0};
        //
        //        geometries.push_back(leg1);
        //        geometries.push_back(leg2);
        //        geometries.push_back(leg3);
        //        geometries.push_back(leg4);
        //        geometries.push_back(seat);
        //        geometries.push_back(rest);
        
        set_mass_attribs_from_colliders();
    }
};


class basicragdoll_t : public body_t
{
public:
    
    basicragdoll_t(const vec3f &p, const vec3f &size, const std::string& id = "") : body_t(p, id)
    {
        vec3f rv = vec3f(-1,1,1).normalize();
        
        //        box_collider_t *leg1 = new box_collider_t({0.2,1,0.2}, mat3::rotation(0*-3.14/4, rv.x, rv.y, rv.z));
        //        box_collider_t *leg2 = new box_collider_t({0.2,2,0.2});
        //        box_collider_t *leg3 = new box_collider_t({0.2,2,0.2});
        //        box_collider_t *leg4 = new box_collider_t({0.2,1,0.2});
        //
        //        box_collider_t *seat = new box_collider_t({1,0.2,1});
        //        box_collider_t *rest = new box_collider_t({1,0.2,0.2});
        //
        //        leg1->X = {0.5,0.5,0.5};
        //        leg2->X = {0.5,1,-0.5};
        //        leg3->X = {-0.5,1,-0.5};
        //        leg4->X = {-0.5,0.5,0.5};
        //        rest->X = {0,2,-0.5};
        //        seat->X = {0,1,0};
        //
        //        geometries.push_back(leg1);
        //        geometries.push_back(leg2);
        //        geometries.push_back(leg3);
        //        geometries.push_back(leg4);
        //        geometries.push_back(seat);
        //        geometries.push_back(rest);
        
        set_mass_attribs_from_colliders();
    }
};

#endif
