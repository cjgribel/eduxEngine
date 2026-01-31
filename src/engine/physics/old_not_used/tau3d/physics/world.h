
/*
 * Tau3D Dynamics 
 * Carl Johan Gribel (c) 2011, cjgribel@gmail.com
 *
 */

#pragma once
#ifndef world_t_H
#define world_t_H

#include "tau3d.h"
#include "body.h"
#include "force.h"
#include "constraint.h"
#include "sat_collision.h"
#include "ray.h"
#include "config.h"
#include <vector>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include "../GL/glut.h"
#elif __APPLE__
#include "GLUT/glut.h"
#endif

class world_t
{
public:
    std::vector<body_t*> bodies;
    std::vector<force_t*> forces;
    
//    std::vector<contact_manifold_t> cmanifolds, cmanifolds_prev;
    std::vector<contact_manifold_t> cmanifoldsA, cmanifoldsB;
    std::vector<contact_manifold_t> *cmanifolds_cur, *cmanifolds_prev;
    
    std::vector<constraint_t*> constraints;
    bool warm_start = WARM_START;
    
    vec3f g = {0.0f, -9.82f, 0.0f};
    body_t *bg_body;

	world_t(void)
    {
        // Create a static, collider-less background body
        bg_body = new body_t(vec3f(0,0,0));
        bg_body->id = "world_bg";
        bg_body->make_static();
        add_body(bg_body);
        
        cmanifolds_cur = &cmanifoldsA;
        cmanifolds_prev = &cmanifoldsB;
    }
    
    void add_body(body_t* body)
    {
        // Generate an id if there is none
        if (body->id.length() == 0)
        {
            std::ostringstream ss;
            ss << "body" << std::setw(4) << std::setfill('0') << bodies.size();
            body->id = ss.str();
        }
        
        bodies.push_back(body);
    }
    
    body_t* find_body_by_id(std::string id)
    {
        printf("nbr bodies %d\n", (int)bodies.size());
        
        for (auto& b : bodies)
        {
            try { printf("%s\n", b->id.c_str()); } catch (std::runtime_error e) {  }
        }
        
        auto body = std::find_if(bodies.begin(), bodies.end(),
                                 [&id](body_t* body) -> bool
                                {
                                    //if (!body) return false;
                                    return !id.compare( body->id );
                                });
        return body == bodies.end() ? nullptr : *body;
    }
    
    void intersect_bodies(ray_t &ray);
    
    void update(f32 h);
    
    void render();

    ~world_t(void);
};

#endif /* world_t_H */
