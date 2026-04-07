// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ProjectBootstrap.hpp"
#include "BatchRegistry.hpp"
#include "LogMacros.h"
#include "ResourceManager.hpp"

namespace eeng::editor
{
    // Bootstraps a project by loading batches and scanning assets.

    bool bootstrap_project(EngineContext& ctx, const ProjectConfig& config)
    {
        if (!ctx.resource_manager || !ctx.batch_registry)
            return false;

        auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);
        resource_manager.set_assets_root(config.imported_assets_root);

        auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);
        br.load_or_create_index(config.batches_root / "index.json");

        BatchId editor_id{};
        if (ctx.batch_registry->try_get_batch_id_by_name(BatchRegistry::kEditorBatchName, editor_id))
        {
            if (!ctx.batch_registry->is_batch_loaded(editor_id))
                ctx.batch_registry->queue_load(editor_id, ctx);
        }

        BatchId default_id{};
        if (ctx.batch_registry->try_get_batch_id_by_name(BatchRegistry::kDefaultBatchName, default_id))
        {
            if (!ctx.batch_registry->is_batch_loaded(default_id))
                ctx.batch_registry->queue_load(default_id, ctx);
        }

        ctx.resource_manager->scan_assets_async(config.imported_assets_root, ctx);

        EENG_LOG(&ctx, "Project bootstrap: assets=%s batches=%s",
            config.imported_assets_root.string().c_str(),
            config.batches_root.string().c_str());
        return true;
    }
} // namespace eeng::editor
