//
//  t3collision.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2012-04-28.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef T3_COLLISION_H
#define T3_COLLISION_H

#include "vec.h"
#include "config.h"
#include "contact_constraint.h"
#include <vector>
#include <ostream>

//class world_t;
class body_t;
//class geometry_t;
//class poly_geom_t;

bool collide_geoms(collider_t* geomA, collider_t* geomB, body_t* bodyA, body_t* bodyB, contact_manifold_t &cm);

void collide_bodies(body_t* bodyA, body_t* bodyB, contact_manifold_t& cm);

void generate_contacts_SAT(std::vector<body_t*>& bodies, std::vector<contact_manifold_t>& cms);


#endif
