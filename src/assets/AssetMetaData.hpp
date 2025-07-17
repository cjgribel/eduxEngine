// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include <string>

#pragma once

namespace eeng
{
    struct AssetMetaData
    {
        Guid guid;
        Guid guid_parent;

        std::string name;               // Tank.fbx
        //std::string type_display_name;  // "Model", "Mesh", "Texture2D", etc.
        std::string type_name; // meta_type.info().name() / entt::type_name<T>. E.g., "eeng::mock::Model"
        // std::string file_path;

        // bool is_collection; ???

        AssetMetaData() = default;

        AssetMetaData(
            Guid guid,
            Guid guid_parent,
            std::string name,
            // std::string type_display_name,
            std::string type_name
            // std::string file_path
        )
            : guid(guid)
            , guid_parent(guid_parent)
            , name(std::move(name))
            // , type_display_name(std::move(type_display_name))
            , type_name(std::move(type_name))
            // , file_path(std::move(file_path))
        {
        }   
    };
}