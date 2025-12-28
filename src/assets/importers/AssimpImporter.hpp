// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef ASSIMP_IMPORTER_HPP
#define ASSIMP_IMPORTER_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Guid.h"
#include "AssetRef.hpp"
#include "EngineContext.hpp"
#include "assets/types/ModelAssets.hpp"

namespace eeng::assets
{
    enum class ImportFlags : unsigned int
    {
        None = 0,
        GenerateTangents = 1 << 0,
        FlipUVs = 1 << 1,
        OptimizeMesh = 1 << 2,
        GenerateNormals = 1 << 3,
        GenerateUVs = 1 << 4,
        SortByPType = 1 << 5,
        OptimizeGraph = 1 << 6
    };

    /// @brief Import options for Assimp-backed model import.
    struct AssimpImportOptions
    {
        std::filesystem::path assets_root;
        std::filesystem::path source_file;
        std::string           model_name;
        float                 scale = 1.0f;
        ImportFlags           flags = ImportFlags::None;
        bool                  append_animations = false;
    };

    /// @brief Import result with primary asset handles.
    struct AssimpImportResult
    {
        bool success = false;
        std::string error_message;
        AssetRef<GpuModelAsset> gpu_model;
        Guid model_guid = Guid::invalid();
    };

    /// @brief Parsed data extracted from Assimp before asset construction.
    /// This mirrors the RenderableMesh extraction but stays CPU-only.
    struct AssimpParseResult
    {
        ModelDataAsset model_data;
        std::vector<MaterialAsset> materials;
        std::vector<TextureAsset> textures;

        // Maps aiMaterial index to material list index.
        std::unordered_map<unsigned, size_t> material_index_map;
    };

    class AssimpImporter
    {
    public:
        AssimpImporter();
        ~AssimpImporter();

        AssimpImportResult import_model(
            const AssimpImportOptions& options,
            EngineContext& ctx);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;

        AssimpParseResult parse_scene(
            const std::filesystem::path& source_file,
            const AssimpImportOptions& options,
            EngineContext& ctx);

        AssimpImportResult build_assets(
            const AssimpParseResult& parsed,
            const AssimpImportOptions& options,
            EngineContext& ctx);
    };
} // namespace eeng::assets

#endif // ASSIMP_IMPORTER_HPP
