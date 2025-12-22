// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef MockImporter_hpp
#define MockImporter_hpp

#include "Guid.h"
#include "Handle.h"
#include "EngineContext.hpp"
#include "mock/MockAssetTypes.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>

//
#include "assets/types/ModelAssets.hpp"

namespace eeng::mock
{
    class ModelImporter
    {
    public:
        static AssetRef<Model> import(
            const std::filesystem::path& assets_root,
            EngineContextPtr ctx);

        static AssetRef<eeng::assets::GpuModelAsset> import_quads_modeldata(
            const std::filesystem::path& assets_root,
            EngineContextPtr ctx);
    };
} // namespace eeng

#endif // ASSIMP_IMPORTER_HPP