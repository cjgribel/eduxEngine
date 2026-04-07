// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/CommandAssetHelpers.hpp"
#include "ResourceManager.hpp"

namespace eeng::editor
{
    TaskResult make_task_error(
        TaskResult::TaskType type,
        std::string_view message,
        const Guid& guid)
    {
        TaskResult res;
        res.type = type;
        res.add_result(guid, false, message);
        return res;
    }

    std::shared_future<TaskResult> queue_unimport_task(
        ResourceManager& rm,
        EngineContext& ctx,
        std::vector<Guid> roots)
    {
        // Unimport assets and rescan to keep indices in sync.
        return rm.queue_import_job(
            [roots = std::move(roots)](ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Unimport;

                std::string error;
                if (!rm.unimport_assets(roots, ctx, &error))
                {
                    if (error.empty())
                        error = "Unimport failed.";
                    res.add_result(Guid{}, false, error);
                    return res;
                }

                for (const Guid& root : roots)
                    res.add_result(root, true, "Unimport ok");

                const auto& assets_root = rm.assets_root();
                if (!assets_root.empty())
                    rm.scan_assets_async(assets_root, ctx);
                return res;
            },
            ctx);
    }

    std::shared_future<TaskResult> queue_restore_task(
        ResourceManager& rm,
        EngineContext& ctx,
        std::vector<Guid> roots)
    {
        // Restore assets from trash and rescan if anything changed.
        return rm.queue_import_job(
            [roots = std::move(roots)](ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Restore;

                bool restored_any = false;
                for (const Guid& root : roots)
                {
                    std::string error;
                    if (!rm.restore_from_trash(root, ctx, &error))
                    {
                        if (error.empty())
                            error = "Restore failed.";
                        res.add_result(root, false, error);
                        continue;
                    }

                    restored_any = true;
                    res.add_result(root, true, "Restore ok");
                }

                if (restored_any)
                {
                    const auto& assets_root = rm.assets_root();
                    if (!assets_root.empty())
                        rm.scan_assets_async(assets_root, ctx);
                }

                return res;
            },
            ctx);
    }
}
