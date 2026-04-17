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

        // Best-effort helper for optional string-array config fields. Invalid
        // shapes are ignored so old project configs keep working.
        std::vector<std::string> load_string_array(
            const nlohmann::json& j,
            const char* key)
        {
            std::vector<std::string> values;
            const auto it = j.find(key);
            if (it == j.end() || !it->is_array())
                return values;

            values.reserve(it->size());
            for (const auto& item : *it)
            {
                if (item.is_string())
                    values.push_back(item.get<std::string>());
            }
            return values;
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

        // Stage 1 Warm Play defaults: use the project's batch index and boot the
        // conventional default batch unless the project opts into something else.
        cfg.strict_play.batch_index = cfg.batches_root / "index.json";
        cfg.strict_play.startup_batches = { "default" };

        if (const auto strict_it = j.find("strict_play");
            strict_it != j.end() && strict_it->is_object())
        {
            const auto& strict = *strict_it;
            cfg.strict_play.batch_index = resolve_path(
                cfg.project_root,
                strict.value("batch_index", cfg.strict_play.batch_index.string()));

            auto startup_batches = load_string_array(strict, "startup_batches");
            if (!startup_batches.empty())
                cfg.strict_play.startup_batches = std::move(startup_batches);
        }

        return cfg;
    }
} // namespace eeng::editor
