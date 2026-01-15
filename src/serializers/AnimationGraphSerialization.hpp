// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <nlohmann/json_fwd.hpp>

namespace entt
{
    class meta_any;
}

namespace eeng::serializers
{
    void serialize_AnimationGraphAsset(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_AnimationGraphAsset(const nlohmann::json& j, entt::meta_any& any);

    void register_animationgraphasset_serialization();
}
