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
    void serialize_ModelDataAsset(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_ModelDataAsset(const nlohmann::json& j, entt::meta_any& any);

    // Register ModelDataAsset serialization hooks with entt meta.
    void register_modeldataasset_serialization();
}
