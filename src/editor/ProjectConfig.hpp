// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <filesystem>
#include <optional>

namespace eeng::editor
{
    struct ProjectConfig
    {
        std::filesystem::path project_root;
        std::filesystem::path assets_root;
        std::filesystem::path imported_assets_root;
        std::filesystem::path batches_root;

        static std::optional<ProjectConfig> load_from_file(
            const std::filesystem::path& config_path);
    };
} // namespace eeng::editor
