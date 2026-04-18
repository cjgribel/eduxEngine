// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ProjectConfig.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace eeng::editor
{
    namespace
    {
        // Resolve project-relative paths once during config load so runtime code
        // can consume absolute or project-rooted content paths consistently.
        std::filesystem::path resolve_path(
            const std::filesystem::path& root,
            const std::filesystem::path& candidate)
        {
            if (candidate.empty())
                return root;
            return candidate.is_relative() ? (root / candidate) : candidate;
        }

        // Required string-array helper for strict project config fields.
        // Returning nullopt keeps config errors loud instead of papering over
        // omissions with implicit defaults.
        std::optional<std::vector<std::string>> load_required_string_array(
            const nlohmann::json& j,
            const char* key)
        {
            std::vector<std::string> values;
            const auto it = j.find(key);
            if (it == j.end() || !it->is_array())
                return std::nullopt;

            values.reserve(it->size());
            for (const auto& item : *it)
            {
                if (!item.is_string())
                    return std::nullopt;
                values.push_back(item.get<std::string>());
            }

            if (values.empty())
                return std::nullopt;

            return values;
        }

        std::optional<StrictPlayConfig> load_required_strict_play_config(
            const std::filesystem::path& project_root,
            const nlohmann::json& j)
        {
            const auto strict_it = j.find("strict_play");
            if (strict_it == j.end() || !strict_it->is_object())
                return std::nullopt;

            const auto& strict = *strict_it;

            const auto batch_index_it = strict.find("batch_index");
            if (batch_index_it == strict.end() || !batch_index_it->is_string())
                return std::nullopt;

            const std::filesystem::path batch_index =
                resolve_path(project_root, batch_index_it->get<std::string>());
            if (batch_index.empty())
                return std::nullopt;

            auto startup_batches = load_required_string_array(strict, "startup_batches");
            if (!startup_batches)
                return std::nullopt;

            return StrictPlayConfig{
                .batch_index = std::move(batch_index),
                .startup_batches = std::move(*startup_batches)
            };
        }
    }

    std::optional<ProjectConfig> ProjectConfig::load_from_file(
        const std::filesystem::path& config_path)
    {
        std::ifstream in(config_path);
        if (!in)
            return std::nullopt;

        nlohmann::json j;
        in >> j;

        ProjectConfig cfg{};
        cfg.project_root = config_path.parent_path();

        if (j.contains("project_root"))
        {
            cfg.project_root = j.value("project_root", cfg.project_root.string());
        }

        const std::filesystem::path assets_root =
            j.value("assets_root", std::string("assets"));
        const std::filesystem::path imported_assets_root =
            j.value("imported_assets_root", std::string("imported_assets"));
        const std::filesystem::path batches_root =
            j.value("batches_root", std::string("batches"));

        cfg.assets_root = resolve_path(cfg.project_root, assets_root);
        cfg.imported_assets_root = resolve_path(cfg.project_root, imported_assets_root);
        cfg.batches_root = resolve_path(cfg.project_root, batches_root);

        // Warm Play boot is now explicit project config. Missing or malformed
        // strict_play data is treated as an invalid project config.
        auto strict_play = load_required_strict_play_config(cfg.project_root, j);
        if (!strict_play)
            return std::nullopt;
        cfg.strict_play = std::move(*strict_play);

        return cfg;
    }
} // namespace eeng::editor
