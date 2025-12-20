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
        Guid guid_parent;   // remove ?

        std::string name;
        std::string type_id;

        std::vector<Guid> contained_assets; // ???

        AssetMetaData() = default;

        AssetMetaData(
            Guid guid,
            Guid guid_parent,
            std::string name,
            std::string type_id
        )
            : guid(guid)
            , guid_parent(guid_parent)
            , name(std::move(name))
            , type_id(std::move(type_id))
        {
        }
    };
}