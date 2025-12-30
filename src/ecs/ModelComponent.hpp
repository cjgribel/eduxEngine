// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef ModelComponents_hpp
#define ModelComponents_hpp

#include "assets/types/ModelAssets.hpp"
// #include "Guid.h"
#include "Entity.hpp"
#include <string>

namespace eeng::ecs
{
    struct ModelComponent
    {
        std::string name;
        // std::string chunk_tag; // PROBABLY SKIP
        // Guid guid;
        AssetRef<eeng::assets::GpuModelAsset> model_ref;
        int clip_index = -1;
        float clip_time = 0.0f; // runtime
        float clip_speed = 1.0f;
        bool loop = true;

        std::vector<glm::mat4> bone_matrices; // runtime
        std::vector<glm::mat4> node_global_matrices; // runtime

        ModelComponent() = default;
        ModelComponent(
            const std::string& name,
            const AssetRef<eeng::assets::GpuModelAsset> model_ref
        )
        : name(name)
        , model_ref(model_ref)
        {}
    };

    inline std::string to_string(const ModelComponent& t) 
    {
        return std::format("ModelComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(ModelComponent& m, Visitor&& visitor) 
    {
        visitor(m.model_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(ModelComponent& m, Visitor&& visitor)  {}
}

#endif // HeaderComponents_hpp
