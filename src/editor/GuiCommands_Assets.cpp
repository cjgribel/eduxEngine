// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.
// Table of contents:
// - ImportModelCommand: import model assets asynchronously with undo/redo support.
// - UnimportAssetsCommand: remove assets by guid and restore on undo.

#include "GuiCommands.hpp"
#include "editor/CommandAsync.hpp"
#include "editor/CommandAssetHelpers.hpp"
#include "editor/CommandContext.hpp"
#include "ResourceManager.hpp"
#include "ThreadPool.hpp"
#include "assets/importers/AssimpImporter.hpp"
#include "LogMacros.h"

namespace eeng::editor {
    using eeng::EngineContextWeakPtr;
    using eeng::Guid;
    using eeng::ResourceManager;
    using eeng::TaskResult;
    namespace ecs = eeng::ecs;

    ImportModelCommand::ImportModelCommand(
        std::filesystem::path source_file,
        assets::ImportFlags flags,
        std::string model_name,
        EngineContextWeakPtr ctx,
        std::shared_ptr<std::atomic<bool>> in_flight)
        : source_file(std::move(source_file))
        , flags(flags)
        , model_name(std::move(model_name))
        , ctx(std::move(ctx))
        , ui_in_flight(std::move(in_flight))
        , display_name("Import Model")
    {
    }

    CommandStatus ImportModelCommand::execute()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
        {
            set_ui_in_flight(ui_in_flight, false);
            return CommandStatus::Done;
        }

        auto* rm = cmd_ctx.resource_manager(*ctx_sp);
        if (!rm)
        {
            set_ui_in_flight(ui_in_flight, false);
            return CommandStatus::Done;
        }

        if (was_undone && !imported_roots.empty())
        {
            pending_action = PendingAction::Restore;
            future = queue_restore_task(*rm, *ctx_sp, imported_roots);
            in_flight = true;
            set_ui_in_flight(ui_in_flight, true);
            return poll_task_future(future, in_flight, [this](const TaskResult& result)
                {
                    if (result.success)
                        was_undone = false;
                    set_ui_in_flight(ui_in_flight, false);
                    pending_action = PendingAction::None;
                });
        }

        auto* thread_pool = cmd_ctx.thread_pool(*ctx_sp);
        if (!thread_pool)
        {
            EENG_LOG_WARN(ctx_sp.get(), "Asset import skipped: ThreadPool unavailable.");
            set_ui_in_flight(ui_in_flight, false);
            return CommandStatus::Done;
        }

        const auto& assets_root = rm->assets_root();
        if (assets_root.empty())
        {
            EENG_LOG_WARN(ctx_sp.get(), "Asset import skipped: assets root not set.");
            set_ui_in_flight(ui_in_flight, false);
            return CommandStatus::Done;
        }

        assets::AssimpImportOptions opts{};
        opts.assets_root = assets_root;
        opts.source_file = source_file;
        opts.model_name = model_name.empty() ? source_file.stem().string() : model_name;
        opts.flags = flags;

        pending_action = PendingAction::Import;
        set_ui_in_flight(ui_in_flight, true);

        auto promise = std::make_shared<std::promise<TaskResult>>();
        future = promise->get_future().share();
        in_flight = true;

        auto ctx_wptr = ctx_sp->weak_from_this();
        thread_pool->queue_task([opts = std::move(opts), ctx_wptr, promise]() mutable
            {
                auto ctx_sp = ctx_wptr.lock();
                if (!ctx_sp || !ctx_sp->resource_manager)
                {
                    promise->set_value(make_task_error(
                        TaskResult::TaskType::Import,
                        "Import failed: context expired."));
                    return;
                }

                auto& rm = static_cast<ResourceManager&>(*ctx_sp->resource_manager);
                assets::AssimpImporter importer;
                auto plan = importer.prepare_import_plan(opts, *ctx_sp);
                if (!plan.result.success)
                {
                    const auto error = plan.result.error_message.empty()
                        ? std::string("Import failed.")
                        : plan.result.error_message;
                    const auto res = make_task_error(TaskResult::TaskType::Import, error);
                    rm.queue_import_job(
                        [res, promise](ResourceManager&, EngineContext&) mutable -> TaskResult
                        {
                            promise->set_value(res);
                            return res;
                        },
                        *ctx_sp);
                    return;
                }

                auto plan_ptr = std::make_shared<assets::AssimpImportPlan>(std::move(plan));
                rm.queue_import_job(
                    [plan_ptr, promise](ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
                    {
                        TaskResult res;
                        res.type = TaskResult::TaskType::Import;
                        try
                        {
                            const auto result = assets::AssimpImporter::apply_import_plan(*plan_ptr, ctx);
                            if (!result.success)
                            {
                                const auto error = result.error_message.empty()
                                    ? std::string("Import failed.")
                                    : result.error_message;
                                res.add_result(Guid{}, false, error);
                            }
                            else
                            {
                                const Guid root_guid = result.gpu_model.guid.valid()
                                    ? result.gpu_model.guid
                                    : result.model_guid;
                                res.add_result(root_guid, true, "Import ok");
                                if (!plan_ptr->assets_root.empty())
                                    rm.scan_assets_async(plan_ptr->assets_root, ctx);
                            }
                        }
                        catch (const std::exception& ex)
                        {
                            res.add_result(Guid{}, false, ex.what());
                        }
                        catch (...)
                        {
                            res.add_result(Guid{}, false, "unknown exception in import job");
                        }

                        promise->set_value(res);
                        return res;
                    },
                    *ctx_sp);
            });

        return poll_task_future(future, in_flight, [this](const TaskResult& result)
            {
                if (result.success && pending_action == PendingAction::Import)
                {
                    imported_roots.clear();
                    for (const auto& op : result.results)
                    {
                        if (op.guid.valid())
                            imported_roots.push_back(op.guid);
                    }
                    was_undone = false;
                }
                set_ui_in_flight(ui_in_flight, false);
                pending_action = PendingAction::None;
            });
    }

    CommandStatus ImportModelCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (imported_roots.empty())
            return CommandStatus::Done;

        auto* rm = cmd_ctx.resource_manager(*ctx_sp);
        if (!rm)
            return CommandStatus::Done;

        pending_action = PendingAction::Unimport;
        future = queue_unimport_task(*rm, *ctx_sp, imported_roots);
        in_flight = true;
        return poll_task_future(future, in_flight, [this](const TaskResult& result)
            {
                if (result.success)
                    was_undone = true;
                pending_action = PendingAction::None;
            });
    }

    CommandStatus ImportModelCommand::update()
    {
        return poll_task_future(future, in_flight, [this](const TaskResult& result)
            {
                if (pending_action == PendingAction::Import && result.success)
                {
                    imported_roots.clear();
                    for (const auto& op : result.results)
                    {
                        if (op.guid.valid())
                            imported_roots.push_back(op.guid);
                    }
                    was_undone = false;
                }
                else if (pending_action == PendingAction::Unimport && result.success)
                {
                    was_undone = true;
                }
                else if (pending_action == PendingAction::Restore && result.success)
                {
                    was_undone = false;
                }

                if (pending_action == PendingAction::Import ||
                    pending_action == PendingAction::Restore)
                {
                    set_ui_in_flight(ui_in_flight, false);
                }
                pending_action = PendingAction::None;
            });
    }

    std::string ImportModelCommand::get_name() const
    {
        return display_name;
    }

    // --- UnimportAssetsCommand ----------------------------------------------

    UnimportAssetsCommand::UnimportAssetsCommand(
        std::vector<Guid> roots,
        EngineContextWeakPtr ctx)
        : roots(std::move(roots))
        , ctx(std::move(ctx))
        , display_name("Unimport Assets")
    {
    }

    CommandStatus UnimportAssetsCommand::execute()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (roots.empty())
            return CommandStatus::Done;

        auto* rm = cmd_ctx.resource_manager(*ctx_sp);
        if (!rm)
            return CommandStatus::Done;

        pending_action = PendingAction::Unimport;
        future = queue_unimport_task(*rm, *ctx_sp, roots);
        in_flight = true;
        return poll_task_future(future, in_flight, [this](const TaskResult&)
            {
                pending_action = PendingAction::None;
            });
    }

    CommandStatus UnimportAssetsCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (roots.empty())
            return CommandStatus::Done;

        auto* rm = cmd_ctx.resource_manager(*ctx_sp);
        if (!rm)
            return CommandStatus::Done;

        pending_action = PendingAction::Restore;
        future = queue_restore_task(*rm, *ctx_sp, roots);
        in_flight = true;
        return poll_task_future(future, in_flight, [this](const TaskResult&)
            {
                pending_action = PendingAction::None;
            });
    }

    CommandStatus UnimportAssetsCommand::update()
    {
        return poll_task_future(future, in_flight, [this](const TaskResult&)
            {
                pending_action = PendingAction::None;
            });
    }

    std::string UnimportAssetsCommand::get_name() const
    {
        return display_name;
    }
} // namespace eeng::editor
