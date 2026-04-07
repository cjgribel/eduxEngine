// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ProjectConfig.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace eeng::editor
{
    namespace
    {
        std::filesystem::path resolve_path(
            const std::filesystem::path& root,
            const std::filesystem::path& candidate)
        {
            if (candidate.empty())
                return root;
            return candidate.is_relative() ? (root / candidate) : candidate;
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

        return cfg;
    }
} // namespace eeng::editor
