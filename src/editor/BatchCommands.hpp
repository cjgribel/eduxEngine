// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <future>
#include <optional>
#include <string>
#include "Command.hpp"
#include "EngineContext.hpp"
#include "BatchRegistry.hpp"
#include "engineapi/IResourceManager.hpp"

namespace eeng::editor
{
    class BatchLoadCommand : public Command
    {
        BatchId batch_id{};
        EngineContextWeakPtr ctx;
        std::shared_future<TaskResult> future{};
        bool in_flight{ false };
        std::string display_name;

    public:
        BatchLoadCommand(const BatchId& id, EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    class BatchUnloadCommand : public Command
    {
        BatchId batch_id{};
        EngineContextWeakPtr ctx;
        std::shared_future<TaskResult> future{};
        bool in_flight{ false };
        std::string display_name;

    public:
        BatchUnloadCommand(const BatchId& id, EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    class BatchLoadAllCommand : public Command
    {
        EngineContextWeakPtr ctx;
        std::shared_future<TaskResult> future{};
        bool in_flight{ false };
        std::string display_name;

    public:
        explicit BatchLoadAllCommand(EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    class BatchUnloadAllCommand : public Command
    {
        EngineContextWeakPtr ctx;
        std::shared_future<TaskResult> future{};
        bool in_flight{ false };
        std::string display_name;

    public:
        explicit BatchUnloadAllCommand(EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    class CreateBatchCommand : public Command
    {
        BatchId created_id{};
        std::string name;
        EngineContextWeakPtr ctx;
        std::string display_name;

    public:
        CreateBatchCommand(std::string name, EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        std::string get_name() const override;
    };

    class DeleteBatchCommand : public Command
    {
        BatchId batch_id{};
        std::string display_name;
        EngineContextWeakPtr ctx;
        std::optional<BatchInfo> snapshot;

    public:
        DeleteBatchCommand(const BatchId& id, EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        std::string get_name() const override;
    };
}
