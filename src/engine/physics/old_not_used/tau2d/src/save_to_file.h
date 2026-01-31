//
//  save_to_file.h
//  tau2d
//
//  Created by Carl Johan Gribel on 2013-11-08.
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//

#ifndef tau2d_save_to_file_h
#define tau2d_save_to_file_h

#include "config.h"
#include "t2World.h"
#include <string>
#include <fstream>


extern ui16 T2_SOLVER_ITERATIONS;

static int find_body_index(t2WorldInstance *world, t2Body *body);

static void save_to_file(t2WorldInstance *world)
{
    string filename = "/Users/cjgribel/Desktop/misc/tau2d/save/t2d_world.txt";
    int nbr_warnings = 0;
    stringstream err_ss;
    
    ofstream file_out;
    file_out.open(filename.c_str());

    /*
     meta
     */
    file_out << filename.substr(filename.find_last_of("/")+1, std::string::npos) << "\n\n";
    
    /*
     world
     */
    file_out << "<world>\n";
    /*
     TODO: camera pos?, ..
     */
    file_out << "iterations " << world->iterations << "\n";
    file_out << "</world>\n\n";
    
    /*
     bodies
     */
    for (int i=0; i<world->nbrBodies; i++)
    {
        file_out << "<body>\n";

        t2Body *body = world->bodies[i];
        //file_out << "index " << i << "\n";
        file_out << "X " << body->X.x << " " << body->X.y << "\nV " << body->V.x << " " << body->V.y << "\n";
        file_out << "R " << body->R << "\nW " << body->W << "\n";
        file_out << "mass " << body->mass << "\ni " << body->I << "\n";
        file_out << "restitution " << body->restitution << "\n";
        file_out << "static_friction " << body->static_friction << "\n";
        file_out << "kinetic_friction " << body->kinetic_friction << "\n";
        file_out << "gravity " << body->gravity.x << " " <<body->gravity.y << "\n";
        file_out << "isStatic " << body->isStatic << "\n";
        file_out << "colorBorder " << body->colorBorderR << " " << body->colorBorderG << " " << body->colorBorderB << "\n";
        file_out << "colorFill " << body->colorFillR << " " << body->colorFillG << " " << body->colorFillB << "\n";
        
        /*
         geometries
         */
        for (int j=0; j<body->nbrGeometries; j++)
        {
            t2Geometry *geom = body->geometries[j];
            if (geom->type == GEOMTYPE_POLY)
            {
                file_out << "<polygon_geometry>\n";
                t2PolygonGeometry *pgeom = static_cast<t2PolygonGeometry*>(body->geometries[j]);

                /*
                 vertices
                 */
                for (int k=0; k<pgeom->nbrVertices; k++)
                    file_out << "v " << pgeom->vertices_local[k].x << " " << pgeom->vertices_local[k].y << "\n";
                
                /*
                 offset
                 */
                file_out << "offset " << pgeom->offset.x << " " <<pgeom->offset.y << "\n";
                
                file_out << "</polygon_geometry>\n";
            }
            if (geom->type == GEOMTYPE_CIRCLE)
            {
                file_out << "<circle_geometry>\n";
                t2CircleGeometry *cgeom = static_cast<t2CircleGeometry*>(body->geometries[j]);
                file_out << "radius " << cgeom->radius << "\n";
                file_out << "</circle_geometry>\n";
            }
        }
        file_out << "</body>\n\n";
    }
    
    /*
     forces (springdampers)
     */
    for (int i=0; i<world->forces.size(); i++)
    {
        file_out << "<springdamper>\n";
        file_out << "</springdamper>\n\n";
    }
    
    /* 
     joints
     */
    for( std::list<t2Joint*>::iterator j_it = world->joints.begin(); j_it != world->joints.end(); j_it++ )
    {
        // bodies & anchors common for ALL joints!? -no need for if-stmt
        
        stringstream ss;
//        if((*j_it)->type == JOINT_DISTANCE || (*j_it)->type == JOINT_REVOLUTE_A ||
//           (*j_it)->type == JOINT_PRISMATIC || (*j_it)->type == JOINT_HYDRAULICACTUATOR ||
//           (*j_it)->type == JOINT_ELEVATEDHINGE || (*j_it)->type == JOINT_ELEVATEDHINGEDUMMY)
//        {
            int indexA = find_body_index(world, (*j_it)->bodyA);
            int indexB = find_body_index(world, (*j_it)->bodyB);
            ss << "bodyA " << indexA << "\n";
            ss << "bodyB " << indexB << "\n";
            ss << "anchorA " << (*j_it)->anchorA.x << " " << (*j_it)->anchorA.y  << "\n";
            ss << "anchorB " << (*j_it)->anchorB.x << " " << (*j_it)->anchorB.y  << "\n";
//        }
        
        if((*j_it)->type == JOINT_DISTANCE)
        {
            t2DistanceJoint *dj = static_cast<t2DistanceJoint*>(*j_it);
            
            file_out << "<distance_joint>\n";
            file_out << ss.str();
            file_out << "L " << dj->L << "\n";
            file_out << "</distance_joint>\n\n";
        }
        else if((*j_it)->type == JOINT_REVOLUTE_A)
        {
            t2RevoluteJoint *rj = static_cast<t2RevoluteJoint*>(*j_it);
            
            file_out << "<revolute_joint>\n";
            file_out << ss.str();
            file_out << "</revolute_joint>\n\n";
        }
        else if((*j_it)->type == JOINT_PRISMATIC_A)
        {
            t2PrismaticJoint *pj = static_cast<t2PrismaticJoint*>(*j_it);
            
            file_out << "<prismatic_joint>\n";
            file_out << ss.str();
            file_out << "</prismatic_joint>\n\n";
        }
        else if((*j_it)->type == JOINT_ELEVATEDHINGE || (*j_it)->type == JOINT_ELEVATEDHINGEDUMMY)
        {
            if((*j_it)->type == JOINT_ELEVATEDHINGE)
            {
                file_out << "<elevatedhinge_joint>\n";
                file_out << ss.str();
                file_out << "</elevatedhinge_joint>\n\n";

            }
            if((*j_it)->type == JOINT_ELEVATEDHINGEDUMMY)
            {
                file_out << "<elevatedhingedummy_joint>\n";
                file_out << ss.str();
                file_out << "</elevatedhingedummy_joint>\n\n";
            }
            
            // common attributes ...
        }
        else if((*j_it)->type == JOINT_ANGULARACTUATOR)
        {
            t2LinearActuatorJoint *rj = static_cast<t2LinearActuatorJoint*>(*j_it);
            
            file_out << "<hydraulicactuator_joint>\n";
            file_out << ss.str();
            file_out << "</hydraulicactuator_joint>\n\n";
        }
        else
        {
            file_out << "warning: unknown joint\n\n";
            err_ss << "warning: unknown joint\n";
            nbr_warnings++;
        }
    }
    
    file_out.close();
    cout << "saved file " << filename << " (" << nbr_warnings << " warnings)\n";
    cout << err_ss.str();
}

static int find_body_index(t2WorldInstance *world, t2Body *body)
{
    for (int i=0; i<world->nbrBodies; i++)
        if (world->bodies[i] == body)
            return i;
    return -1;
}

static void load_from_file(t2WorldInstance *world)
{
    char filename[] = "/Users/cjgribel/Desktop/misc/tau2d/save/t2d_world.txt";
    
    ifstream file_in(filename);
    if (!file_in) {
		cout << "failed to open file " << filename << "\n";
		return;
	}
    
    t2Body *body = NULL;
    t2PolygonGeometry *pgeom = NULL;
    t2CircleGeometry *cgeom = NULL;
    t2Joint *cur_j = NULL;
    t2DistanceJoint *dj = NULL;
    float nbr0, nbr1, nbr2;
    int int0;
    int nbr_initial_bodies = world->nbrBodies;
    
    string line;
    while (getline(file_in, line))
    {
        /*
         bodies
         */
        if (sscanf(line.c_str(), "iterations %f", &nbr0) == 1)
        {
            world->iterations = nbr0;
        }
        else if (line == "<body>")
        {
            body = new t2Body();
        }
        else if (sscanf(line.c_str(), "X %f %f", &nbr0, &nbr1) == 2 && body)
        {
            body->X = vec2f(nbr0, nbr1);
        }
        else if (sscanf(line.c_str(), "V %f %f", &nbr0, &nbr1) == 2 && body)
        {
            body->V = vec2f(nbr0, nbr1);
        }
        else if (sscanf(line.c_str(), "R %f", &nbr0) == 1 && body)
        {
            body->R = nbr0;
        }
        else if (sscanf(line.c_str(), "W %f", &nbr0) == 1 && body)
        {
            body->W = nbr0;
        }
        else if (sscanf(line.c_str(), "mass %f", &nbr0) == 1 && body)
        {
            body->setMass(nbr0, body->I);
        }
        else if (sscanf(line.c_str(), "i %f", &nbr0) == 1 && body)
        {
            body->setMass(body->mass, nbr0);
        }
        else if (sscanf(line.c_str(), "restitution %f", &nbr0) == 1 && body)
        {
            body->restitution = nbr0;
        }
        else if (sscanf(line.c_str(), "static_friction %f", &nbr0) == 1 && body)
        {
            body->static_friction = nbr0;
        }
        else if (sscanf(line.c_str(), "kinetic_friction %f", &nbr0) == 1 && body)
        {
            body->kinetic_friction = nbr0;
        }
        else if (sscanf(line.c_str(), "gravity %f %f", &nbr0, &nbr1) == 2 && body)
        {
            body->gravity = vec2f(nbr0, nbr1);
        }
        else if (sscanf(line.c_str(), "isStatic %f", &nbr0) == 1 && body)
        {
            body->isStatic = nbr0?true:false;
        }
        else if (sscanf(line.c_str(), "colorBorder %f %f %f", &nbr0, &nbr1, &nbr2) == 3 && body)
        {
            body->colorBorderR = nbr0;
            body->colorBorderG = nbr1;
            body->colorBorderB = nbr2;
        }
        else if (sscanf(line.c_str(), "colorFill %f %f %f", &nbr0, &nbr1, &nbr2) == 3 && body)
        {
            body->colorFillR = nbr0;
            body->colorFillG = nbr1;
            body->colorFillB = nbr2;
        }
        else if (line == "</body>")
        {
            world->addBody(body);
            body = NULL;
        }
        /*
         geometries
         */
        else if (line == "<polygon_geometry>")
        {
            pgeom = new t2PolygonGeometry();
        }
        else if (sscanf(line.c_str(), "v %f %f", &nbr0, &nbr1) == 2 && pgeom)
        {
            pgeom->addVertex(vec2f(nbr0, nbr1));
        }
        else if (sscanf(line.c_str(), "offset %f %f", &nbr0, &nbr1) == 2 && pgeom)
        {
            pgeom->offset = vec2f(nbr0, nbr1);
        }
        else if (line == "</polygon_geometry>")
        {
            body->addGeometry(pgeom);
            pgeom = NULL;
        }
        /*
         joints
         */
        else if (line == "<distance_joint>")
        {
            dj = new t2DistanceJoint();
            cur_j = dj;
        }
        else if (sscanf(line.c_str(), "L %f", &nbr0) == 1 && dj)
        {
            dj->L = nbr0;
        }
        else if (line == "</distance_joint>")
        {
            world->joints.push_back(dj);
            dj = NULL;
            cur_j = NULL;
        }
        else if (sscanf(line.c_str(), "bodyA %d", &int0) == 1 && cur_j)
        {
            cur_j->bodyA = world->bodies[int0+nbr_initial_bodies];
        }
        else if (sscanf(line.c_str(), "bodyB %d", &int0) == 1 && cur_j)
        {
            cur_j->bodyB = world->bodies[int0+nbr_initial_bodies];
        }
        else if (sscanf(line.c_str(), "anchorA %f %f", &nbr0, &nbr1) == 2 && cur_j)
        {
            cur_j->anchorA = vec2f(nbr0, nbr1);
        }
        else if (sscanf(line.c_str(), "anchorB %f %f", &nbr0, &nbr1) == 2 && cur_j)
        {
            cur_j->anchorB = vec2f(nbr0, nbr1);
        }
    }
    
    cout << "loaded file " << filename << "\n";
}

#endif

