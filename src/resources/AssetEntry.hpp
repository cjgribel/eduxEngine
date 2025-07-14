// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "AssetMetaData.hpp"
#include <filesystem>

namespace eeng
{
    struct AssetEntry
    {
        AssetMetaData meta;
        std::filesystem::path relative_path;  // relative to asset root
        std::filesystem::path absolute_path;  // full disk path to .json
    };
}