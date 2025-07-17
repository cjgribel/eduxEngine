// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"
#include "util/VecTree.h"
#include <filesystem>

#pragma once

namespace eeng
{
    struct AssetTreeViews
    {
        VecTree<Guid> dependency_tree;
        VecTree<std::filesystem::path> file_tree;

        // reference tree, inclusion tree, chunk nesting tree...
    };
}