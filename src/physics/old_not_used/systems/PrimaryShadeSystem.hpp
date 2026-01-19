//
//  PrimaryShadeSystem.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-10.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef PrimaryShadeSystem_hpp
#define PrimaryShadeSystem_hpp

#include <entt/entt.hpp>
class Scene;

class PrimaryShadeSystem
{
public:
    static void update(float dt,
                       Scene& scene,
                       bool editor_mode);
};

#endif /* PrimaryShadeSystem_hpp */
