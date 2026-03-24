// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "Guid.h"

#include <string>

namespace eeng
{
    class ResourceManager;
    struct EngineContext;
}

namespace eeng::assets
{
    /**
     * @brief Result of cooking a single `TerrainRecipeAsset`.
     *
     * The cook either produces one cooked `TerrainAsset` manifest plus its
     * generated chunk assets, or returns an error message describing why the
     * cook failed.
     */
    struct TerrainCookResult
    {
        bool success = false;
        std::string error_message{};
        Guid terrain_guid{};
    };

    /**
     * @brief Offline terrain cooker for chunked render/collision terrain data.
     *
     * `TerrainCooker` consumes one editor-side `TerrainRecipeAsset`, samples
     * the referenced source terrain mesh into a regular height grid, then
     * emits:
     * - one cooked `TerrainAsset` manifest
     * - one cooked `TerrainChunkAsset` per chunk
     * - one chunk-local render `ModelDataAsset`
     * - one chunk-local `GpuModelAsset`
     *
     * The cooker is intentionally deterministic with respect to output paths
     * so that repeated re-cooks overwrite the same generated asset set.
     */
    class TerrainCooker
    {
    public:
        /**
         * @brief Cook one terrain recipe into runtime terrain assets.
         * @param rm Resource manager used for reading source assets and writing cooked assets.
         * @param recipe_guid GUID of the `TerrainRecipeAsset` to cook.
         * @param ctx Engine context used for resource-manager/editor integration.
         * @return Result containing success state, error message, and cooked terrain GUID.
         */
        static TerrainCookResult cook_recipe(
            ResourceManager& rm,
            const Guid& recipe_guid,
            EngineContext& ctx);
    };
}
