//
//  UniformGridCollisionSystem.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-12.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef UniformGridCollisionSystem_hpp
#define UniformGridCollisionSystem_hpp

#include "CollisionSystem.hpp"

// /Users/ag1498/Dropbox/dev/_auxlibs/entityx-master/examples/example.cc
// This class has STATE = the grid
// Colliders are stored elsewhere
// Manages colliders via - ID? handles?
class UniformGridCollisionSystem : public CollisionSystem<3>
{
    // Called colliders have been updated
    
    // Update the grid
    // update()
    
    // find_collisions()
    // Do the actual CD - find pairs of colliders
    //      0) CANDIDATE RB-COLLIDER PAIRS
    //      Store static candidates separately? -> avoid testing statics against each other
    //          Should there be STATIC COLLIDERS (eg colliders not linked to a (static) RB)
    //          Or is it enough to link to staic RB's?
    //          Case to consider: large mesh colliders, where collider triangles are potentially
    //              added to separate bins
    // Emit collision events
};

#endif /* UniformGridCollisionSystem_hpp */
