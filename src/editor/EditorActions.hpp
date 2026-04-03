// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <deque>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include "EngineContext.hpp"

namespace eeng::assets
{
    enum class ImportFlags : unsigned int;
}

namespace eeng::editor
{
    struct SceneActions
    {
        static void create_entity(EngineContext& ctx, const ecs::Entity& parent_entity);
        static void delete_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection);
        static void copy_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection);
        static void parent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection);
        static void unparent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection);
        static void add_components(EngineContext& ctx, const std::deque<ecs::Entity>& selection, entt::id_type comp_id);
        static void remove_components(EngineContext& ctx, const std::deque<ecs::Entity>& selection, entt::id_type comp_id);
        static void spawn_entity_branch_from_json(
            EngineContext& ctx,
            nlohmann::json branch_json,
            const ecs::Entity& parent_entity = ecs::Entity{},
            bool remap_guids = true);
        static void bake_transform_branch(EngineContext& ctx, const ecs::Entity& root_entity);
    };

    struct BatchActions
    {
        static void load_batch(EngineContext& ctx, const BatchId& id);
        static void unload_batch(EngineContext& ctx, const BatchId& id);
        static void load_all(EngineContext& ctx);
        static void unload_all(EngineContext& ctx);
        static void create_batch(EngineContext& ctx, std::string name);
        static void delete_batch(EngineContext& ctx, const BatchId& id);
        static void assign_entities_to_batch(
            EngineContext& ctx,
            const BatchId& id,
            const std::deque<ecs::Entity>& selection);
    };

    struct AssetActions
    {
        /// @brief Queue an undoable model import via Assimp.
        static void import_model(
            EngineContext& ctx,
            const std::filesystem::path& source_file,
            assets::ImportFlags flags,
            std::string model_name = {},
            std::shared_ptr<std::atomic<bool>> in_flight = {});

        /// @brief Queue an undoable standalone texture import.
        static void import_texture(
            EngineContext& ctx,
            const std::filesystem::path& source_file,
            std::string texture_name = {},
            std::shared_ptr<std::atomic<bool>> in_flight = {});

        /// @brief Create a mock animation graph asset for quick testing.
        static void import_animation_graph_mock(
            EngineContext& ctx,
            std::string graph_name = "UE_Mannequin",
            std::string clip_name = "idle");

        /// @brief Create a piston animation graph template (single clip scrub).
        static void import_animation_graph_piston(
            EngineContext& ctx,
            std::string graph_name = "PistonGraph",
            std::string clip_name = "Piston");

        /// @brief Create an empty TerrainRecipeAsset for authoring terrain cooks.
        static void create_terrain_recipe(
            EngineContext& ctx,
            std::string recipe_name = "TerrainRecipe");

        /**
         * @brief Explicitly cook a terrain recipe into runtime terrain assets.
         *
         * This is intentionally treated as an asset-build action rather than a
         * normal undoable scene edit. Re-cooking overwrites the deterministic
         * output folder owned by the recipe.
         */
        static void cook_terrain_recipe(
            EngineContext& ctx,
            const Guid& recipe_guid);

        /**
         * @brief Remove all generated outputs owned by a terrain recipe.
         *
         * This clears cooked terrain assets and generated terrain chunk batches
         * so the recipe can be re-cooked from a clean state. Like cooking, this
         * is treated as a build/cleanup action rather than an undoable scene edit.
         */
        static void clear_cooked_terrain(
            EngineContext& ctx,
            const Guid& recipe_guid);

        /// @brief Queue an undoable unimport by GUID (serialized on RM strand).
        static void unimport_assets(
            EngineContext& ctx,
            std::vector<Guid> roots);

        /// @brief Restore assets from trash by root GUID (serialized on RM strand).
        static void restore_assets(
            EngineContext& ctx,
            std::vector<Guid> roots);
    };
}
