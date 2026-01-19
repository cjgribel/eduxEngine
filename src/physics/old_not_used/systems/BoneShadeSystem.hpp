//
//  BoneShadeSystem.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-09-10.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef BoneShadeSystem_hpp
#define BoneShadeSystem_hpp

#include <entt/entt.hpp>
#include "mat.h"

class Scene;

class BoneShadeSystem
{
    static inline bool first = true;
    static inline linalg::m4f D = linalg::m4f_1;
    
public:
    static void update(float dt,
                       Scene& scene);
};

#endif /* BoneShadeSystem_hpp */
