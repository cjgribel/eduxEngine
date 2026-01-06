// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef ASSIMP_IMPORTER_HPP
#define ASSIMP_IMPORTER_HPP

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Guid.h"
#include "AssetRef.hpp"
#include "AssetMetaData.hpp"
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
        // Additional animation files or folders to append during import.
        std::vector<std::filesystem::path> animation_sources;
    };

    /// @brief Import result with primary asset handles.
    struct AssimpImportResult
    {
        bool success = false;
        std::string error_message;
        AssetRef<GpuModelAsset> gpu_model;
        Guid model_guid = Guid::invalid();
    };

    /// @brief Result for appending animations to an existing model.
    struct AssimpAppendResult
    {
        bool success = false;
        std::string error_message;
        Guid model_guid = Guid::invalid();
        size_t appended_clips = 0;
    };

    /// @brief Options for appending animations to an existing model.
    struct AssimpAppendOptions
    {
        std::filesystem::path source_file;
        Guid target_model = Guid::invalid();
        ImportFlags flags = ImportFlags::None;
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

    struct AssimpAssetCopy
    {
        std::filesystem::path source;
        std::filesystem::path destination;
    };

    template<typename T>
    struct AssimpAssetWrite
    {
        T asset;
        AssetMetaData meta;
        std::filesystem::path asset_path;
        std::filesystem::path meta_path;
    };

    struct AssimpImportPlan
    {
        std::filesystem::path assets_root;
        std::vector<std::filesystem::path> directories;
        std::vector<AssimpAssetCopy> file_copies;

        std::vector<AssimpAssetWrite<TextureAsset>> textures;
        std::vector<AssimpAssetWrite<GpuTextureAsset>> gpu_textures;
        std::vector<AssimpAssetWrite<MaterialAsset>> materials;
        std::vector<AssimpAssetWrite<GpuMaterialAsset>> gpu_materials;
        std::optional<AssimpAssetWrite<ModelDataAsset>> model;
        std::optional<AssimpAssetWrite<GpuModelAsset>> gpu_model;

        AssimpImportResult result;
    };

    /// @brief Assimp-backed model importer that builds CPU/GPU assets.
    ///
    /// Policy notes:
    /// - We keep ModelDataAsset as pure data; no GL calls happen in parse_scene.
    /// - Embedded textures are handled lazily (only if a material references "*<index>").
    /// - External textures are copied into the model's texture folder and stored as relative paths.
    /// - Some Assimp format quirks are handled in parsing (e.g., normal maps in HEIGHT slot).
    class AssimpImporter
    {
    public:
        AssimpImporter();
        ~AssimpImporter();

        /// @brief Parse and build an import plan (heavy work).
        /// @note Embedded textures may be exported during parsing.
        AssimpImportPlan prepare_import_plan(
            const AssimpImportOptions& options,
            EngineContext& ctx);

        /// @brief Apply a prepared import plan (file IO + rm.import).
        /// @note Policy: run on the RM strand to serialize with scans.
        static AssimpImportResult apply_import_plan(
            const AssimpImportPlan& plan,
            EngineContext& ctx);

        /// @note Performs file IO + rm.import on the caller thread; serialize with scans.
        AssimpImportResult import_model(
            const AssimpImportOptions& options,
            EngineContext& ctx);

        /// @note Performs file IO + rm.import on the caller thread; serialize with scans.
        AssimpImportResult import_model_with_animations(
            const AssimpImportOptions& options,
            const std::vector<std::filesystem::path>& animation_inputs,
            EngineContext& ctx);

        AssimpAppendResult append_animations(
            const AssimpAppendOptions& options,
            EngineContext& ctx);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;

        AssimpParseResult parse_scene(
            const std::filesystem::path& source_file,
            const AssimpImportOptions& options,
            EngineContext& ctx);

        AssimpImportPlan build_import_plan(
            const AssimpParseResult& parsed,
            const AssimpImportOptions& options,
            EngineContext& ctx);

        AssimpImportResult build_assets(
            const AssimpParseResult& parsed,
            const AssimpImportOptions& options,
            EngineContext& ctx);
    };
} // namespace eeng::assets

#endif // ASSIMP_IMPORTER_HPP
