#include <gtest/gtest.h>

#include "editor/ProjectConfig.hpp"
#include <filesystem>
#include <fstream>

namespace
{
    // Keep the config tests self-contained: write a tiny project.json variant,
    // parse it through the real loader, then clean up the temp file on scope exit.
    class ScopedTempFile
    {
    public:
        explicit ScopedTempFile(std::filesystem::path path)
            : path_(std::move(path))
        {
        }

        ~ScopedTempFile()
        {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }

        const std::filesystem::path& path() const
        {
            return path_;
        }

    private:
        std::filesystem::path path_;
    };
}

TEST(ProjectConfig, LoadsStrictPlayDefaultsFromBatchRoot)
{
    const auto config_path =
        std::filesystem::temp_directory_path() / "edux_project_config_defaults.json";
    ScopedTempFile temp_file(config_path);

    std::ofstream out(config_path);
    ASSERT_TRUE(out.good());
    out << R"json({
  "assets_root": "../../assets",
  "imported_assets_root": "imported_assets",
  "batches_root": "batches"
})json";
    out.close();

    const auto config = eeng::editor::ProjectConfig::load_from_file(config_path);
    ASSERT_TRUE(config.has_value());

    // Missing strict_play block should still produce a usable Warm Play config.
    EXPECT_EQ(config->strict_play.batch_index, config->batches_root / "index.json");
    ASSERT_EQ(config->strict_play.startup_batches.size(), 1u);
    EXPECT_EQ(config->strict_play.startup_batches.front(), "default");
}

TEST(ProjectConfig, LoadsStrictPlayOverrides)
{
    const auto config_path =
        std::filesystem::temp_directory_path() / "edux_project_config_strict_play.json";
    ScopedTempFile temp_file(config_path);

    std::ofstream out(config_path);
    ASSERT_TRUE(out.good());
    out << R"json({
  "assets_root": "../../assets",
  "imported_assets_root": "imported_assets",
  "batches_root": "batches",
  "strict_play": {
    "batch_index": "runtime/index.json",
    "startup_batches": ["boot", "level_01"]
  }
})json";
    out.close();

    const auto config = eeng::editor::ProjectConfig::load_from_file(config_path);
    ASSERT_TRUE(config.has_value());

    // Relative strict-play paths are resolved from the project root, just like
    // the existing asset and batch roots.
    EXPECT_EQ(config->strict_play.batch_index, config->project_root / "runtime/index.json");
    ASSERT_EQ(config->strict_play.startup_batches.size(), 2u);
    EXPECT_EQ(config->strict_play.startup_batches[0], "boot");
    EXPECT_EQ(config->strict_play.startup_batches[1], "level_01");
}
