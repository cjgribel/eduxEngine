
/*
 * Tau3D Dynamics 
 * Carl Johan Gribel (c) 2011, cjgribel@gmail.com
 *
 */

void warmstarting() {}
/*
 warm starting notes
 
 data:
    body indices/pointers (A & B)
    geometry indices? (A & B)
    indices of face, edge, vertex (just need one since body/geom order should be the same, right?)
 
    think box-on-box, how to distinguish the eight contacts?
        bodies and geom's – given   
        all 8 have the same face index (from which cn is used)
        4 'inner' contacts: 4 vertex indices, edge = -1
        4 'surface' contacts (from clipping) - 4 edge indices, vertex = -1
    issue: clipped edges may come from both objects. 
 
 list version:
    Find_first, find_last
 
 hash version: hash warm contact / warm lambda with old contact as key
    contact_t then needs [10]:
    1. struct overloading () and returning hash value (mult large primes with each index and add together, or use [10])
    2. overload == (for unordered_map to handle collisions)
 
 BULLET
 presentation:
 Adding points to cache
 •Check for duplicate points
    •Use feature id or distance tolerance
 •Reduce points when more than 4 points
    •Always keep point with deepest penetration
    •Try to maximize area
 
 see btPersistentManifold
 
 BOX2D
 uint8 	flip            A value of 1 indicates that the reference edge is on shape2.
 uint8 	incidentEdge 	The edge most anti-parallel to the reference edge.
 uint8 	incidentVertex 	The vertex (0 or 1) on the incident edge that was clipped.
 uint8 	referenceEdge 	The edge that defines the outward contact normal.
 
 contact storing:
	hash with id's as key: O(1) retrieval. for vertex index triplet hash function = LARGE_PRIME_A*vi + LARGE_PRIME_B*ni+ LARGE_PRIME_C*ti
	vector: O(N) retrieval, but may search intelligently, e.g. assume order coherence and search from the position of the last hit
 go with vector at first
 
 core trait of identification sequence: uniqueness. no two contacts may have the same sequence.
 tau2d: bodies, geoms, edge, vertex
 + faces?
 spheres: edge id = 0 (always match) for vertex contact, and vertex id = 0 for edge contact
 
 body and geom id's comes directly from the test
 edge and face id's should be pretty straightforward too
 what about id to vertices from clipped edges? i.e. that do not exist as part of a geometry.
	flag? I guess one edge can only give rise to one original vertex and one clipped vertex (or possibly 2 original vertices)
 
 store in contact class
 to fetch id's, use contacts directly when computing manifold
 some code (also discussion about warm starting + friction tangents) [http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?t=8886]
 store contacts directly in manifold (instead of vec3),
 
 #define LARGE_PRIME_A 10007
 #define LARGE_PRIME_B 11003
 struct contact_id {
 //unsigned id...
 //bool operator==
 //        int hash_value()
 //        inline unsigned int ordered_two_int_hash(unsigned int a, unsigned int b) {
 //            return LARGE_PRIME_A * a + LARGE_PRIME_B * b;
 //        }
 };
 */


#include <iostream>
#include "world.h"

/*
 * time-step simulation from t=ti to t=ti+h
 *
 * 1. vertex-shade geometries
 * 2. collision detection
 * 3. apply external forces
 * 4. integrate velocities from forces
 * 5. solve contacts and constraints
 * 6. integrate positions from velocities
 *
 */
void world_t::update(float h)
{
//    //
//    // 1. Vertex shade geometries to world space using body state
//    //
//    
//    for (auto body : bodies)
//        body->vertex_shade();
    
    //
    // 2. Run collision detection (using world space geometries) abd collect contact data
    //
    
//    cmanifolds_prev = cmanifolds; // TODO: do not copy vector like this, use pointers
//    cmanifolds.clear();
//    //contact_manifolds.clear();
//    generate_contacts_SAT(bodies, cmanifolds);
    
    std::swap(cmanifolds_cur, cmanifolds_prev);
    cmanifolds_cur->clear();
    generate_contacts_SAT(bodies, *cmanifolds_cur);
    
#if 0
    float maxage = 0;
    for (auto &cm : *cmanifolds_prev)
    {
        for (auto& c : cm.contacts)
            maxage = fmaxf(maxage, c.age);
    }
    printf("max age found %f\n", maxage);
#endif
    
    //
    // 3. Apply external forces
    //
    // F += f
    //
    
    for (auto f : forces)
        f->applyForce();

#ifdef VORTEX
    // vortex test
    static const vortex_F_t vortex_F = vortex_F_t(vec3f(0,0,0), normalize(vec3f(1,1,0)));
    vortex_F.apply_force(bodies);
#endif
    
    //
    // 4. Integrate velocities from accumulated force at t=ti (explicit integration)
    //
    // Linear velocity from gravity and accumulated linear force
    // V[ti+h] <- V[ti] + (F[ti]/m + g)*h
    //
    // Angular velocity from accumulated torque
    // W[ti+h] <- W[ti] + I^-1*T[ti]*h
    //
    for (auto body : bodies)
    {
        if (body->is_static) continue;
        
        body->V += (body->F*body->im + g - body->V*V_DAMPING) * h;
        body->W += body->iI_w * ((body->T - body->W*W_DAMPING) * h);
#if 0
        // clamp W to cap
        f32 W_norm = body->W.norm2();
        if(W_norm > W_CAP)
            body->W = body->W * (W_CAP/W_norm);
#endif
        // reset applied forces
        body->F.set(0, 0, 0);
        body->T.set(0, 0, 0);
    }
    
    //
    // 5a. Pre-solve / Warm-start
    //
    
    // Contacts
    //
//    int warm_count = 0, warmcp_count = 0;
    for (contact_manifold_t &cm : *cmanifolds_cur)
    {
        contact_manifold_t *cm_warm = nullptr;
        // warm-start
        if (warm_start) {
            auto cm_warm_it = std::find(cmanifolds_prev->begin(), cmanifolds_prev->end(), cm);
            if (cm_warm_it != cmanifolds_prev->end()) {
                cm_warm = &*cm_warm_it;
//                warm_count++;
//                warmcp_count += cm_warm->contacts.size();
            }
        }
        cm.pre_solve(cm_warm, h);
    }
//    printf("warm/all %d/%d (%d%%), contacts %d\n",
//           warm_count,
//           cmanifolds_cur->size(),
//           (int)((float)warm_count/cmanifolds_cur->size()*100),
//           warmcp_count);
    
    // Constraints
    //
    for (constraint_t *c : constraints)
        c->pre_solve();
    
    //
    // 5b. Solve the DAE using PSG.
    //
    
    for (int i=0; i<T3_SOLVER_ITERATIONS; i++)
    {
        for (auto& cmanifold : *cmanifolds_cur)
            cmanifold.solve(h);
        
        for (auto c : constraints)
            c->solve();
    }
    
    // Save the last iteration for bodies in contact with another static body.
    // Idea: Eliminate velocities which cause penetration into e.g. the ground
    //
//    for (auto& cmanifold : cmanifolds)
//    {
//        if (cmanifold.bodyA->is_static || cmanifold.bodyB->is_static)
//            cmanifold.solve();
//    }
    
    //
    // 5c. Post-solve
    //
    
    for (auto& cmanifold : *cmanifolds_cur)
        cmanifold.post_solve(h);
    
    //
    // 6. Integrate linear & angular positions from velocities at t=ti+h (implicit integration)
    //
    // Position from linear velocity
    // X[ti+h] <- X[ti] + V[ti+h]*h
    //
    // Orientation from angular velocity
    // Q[ti+h] <- Q[ti] + Qdot(W[ti+h])*h (quaternion form)
    // R[ti+h] <- R[ti] + Rdot(W[ti+h])*h (matrix form)
    //
    
    for (auto body : bodies)
    {
        if (body->is_static) continue;

        body->X += body->V*h;
#if 1
        // quaternion orientation
        quatf Qdot = body->Q.getQdot(body->W);
        body->Q += Qdot*h;
        body->Q.normalize();
        // auxiliary
        body->R = mat3f(body->Q);
        body->Ri = body->R; body->Ri.transpose();
#else
        // matrix orientation
        mat3 Rdot = body->R.getRdot(body->W);
        body->R += Rdot * h;
        body->R.normalize();
        // auxiliary
        body->Ri = body->R; body->Ri.transpose();
#endif
    }
    
    //
    // 1. Vertex shade geometries to world space using body state
    //
    
    for (auto body : bodies)
        body->vertex_shade();
}

void world_t::intersect_bodies(ray_t &ray)
{
    for (auto body : bodies)
        body->intersect_colliders(ray);
}

void world_t::render()
{
    // world axes
//    glColor3f(1, 0, 0);
//    glBegin(GL_LINES);
//    glVertex3f(0, 0, 0);
//    glVertex3f(1, 0, 0);
//    glEnd();
//    glColor3f(0, 1, 0);
//    glBegin(GL_LINES);
//    glVertex3f(0, 0, 0);
//    glVertex3f(0, 1, 0);
//    glEnd();
//    glColor3f(0, 0, 1);
//    glBegin(GL_LINES);
//    glVertex3f(0, 0, 0);
//    glVertex3f(0, 0, 1);
//    glEnd();
    
    for (body_t* body : bodies)
        body->render();
    
    for (force_t* force : forces)
        force->render();
    
    for (constraint_t *c : constraints)
        c->render();
}

world_t::~world_t(void)
{
    for (int i=0; i<bodies.size(); i++) delete bodies[i];
    for (int i=0; i<forces.size(); i++) delete forces[i];
}
