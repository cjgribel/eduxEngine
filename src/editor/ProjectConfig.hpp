// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace eeng::editor
{
    struct StrictPlayConfig
    {
        std::filesystem::path batch_index;
        std::vector<std::string> startup_batches;
    };

    struct ProjectConfig
    {
        std::filesystem::path project_root;
        std::filesystem::path assets_root;
        std::filesystem::path imported_assets_root;
        std::filesystem::path batches_root;
        StrictPlayConfig strict_play;

        static std::optional<ProjectConfig> load_from_file(
            const std::filesystem::path& config_path);
    };
} // namespace eeng::editor
