//
//  GuiCommands.hpp
//
//  Created by Carl Johan Gribel on 2024-12-01.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef GuiCommands_hpp
#define GuiCommands_hpp

//#include "Context.hpp"
#include "EngineContext.hpp"
// #include <entt/entt.hpp>
// #include "Scene.hpp"
#include "Command.hpp"
#include "AssignFieldCommand.hpp"
#include "MetaSerialize.hpp"
#include <atomic>
#include <filesystem>
#include <future>
#include <entt/fwd.hpp>
#include <vector>

namespace eeng::assets
{
    enum class ImportFlags : unsigned int;
}

namespace eeng::editor {

    class CreateEntityCommand : public Command
    {
        ecs::Entity created_entity;
        Guid created_guid;
        BatchId created_batch;
        ecs::Entity parent_entity;
        nlohmann::json entity_json{};
        std::shared_future<ecs::EntityRef> create_future{};
        std::shared_future<bool> attach_future{};
        std::shared_future<bool> destroy_future{};
        EngineContextWeakPtr ctx;
        std::string display_name;
        enum class AsyncStage { None, Create, Attach, Destroy };
        AsyncStage async_stage{ AsyncStage::None };

    public:
        CreateEntityCommand(
            const ecs::Entity& parent_entity,
            EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    // ------------------------------------------------------------------------

    class DestroyEntityCommand : public Command
    {
        ecs::Entity entity;
        Guid entity_guid;
        BatchId entity_batch;
        nlohmann::json entity_json{};
        std::shared_future<bool> destroy_future{};
        std::shared_future<bool> attach_future{};
        EngineContextWeakPtr ctx;
        std::string display_name;
        enum class AsyncStage { None, Destroy, Attach };
        AsyncStage async_stage{ AsyncStage::None };

    public:
        DestroyEntityCommand(
            const ecs::Entity& entity,
            EngineContextWeakPtr ctx
        );

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    // --- DestroyEntityBranchCommand ----------------------------------------

    class DestroyEntityBranchCommand : public Command
    {
        ecs::Entity root_entity;
        Guid root_guid;
        BatchId branch_batch;
        nlohmann::json branch_json{};
        std::vector<std::shared_future<bool>> destroy_futures{};
        std::vector<std::shared_future<bool>> attach_futures{};
        EngineContextWeakPtr ctx;
        std::string display_name;
        enum class AsyncStage { None, Destroy, Attach };
        AsyncStage async_stage{ AsyncStage::None };

    public:
        DestroyEntityBranchCommand(
            const ecs::Entity& entity,
            EngineContextWeakPtr ctx
        );

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    // --- CopyEntityCommand --------------------------------------------------

    class CopyEntityCommand : public Command
    {
        ecs::Entity entity_source;
        Guid source_guid;
        BatchId target_batch;
        nlohmann::json copy_json{};
        std::shared_future<bool> attach_future{};
        std::shared_future<bool> destroy_future{};
        EngineContextWeakPtr ctx;
        std::string display_name;
        enum class AsyncStage { None, Attach, Destroy };
        AsyncStage async_stage{ AsyncStage::None };

    public:
        CopyEntityCommand(
            const ecs::Entity& entity,
            EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    // --- CopyEntityBranchCommand --------------------------------------------

    class CopyEntityBranchCommand : public Command
    {
        ecs::Entity root_entity;
        Guid root_guid;
        BatchId target_batch;
        nlohmann::json branch_json{};
        std::vector<std::shared_future<bool>> attach_futures{};
        std::vector<std::shared_future<bool>> destroy_futures{};
        EngineContextWeakPtr ctx;
        std::string display_name;
        enum class AsyncStage { None, Attach, Destroy };
        AsyncStage async_stage{ AsyncStage::None };

    public:
        CopyEntityBranchCommand(
            const ecs::Entity& entity,
            EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    // --- SpawnEntityBranchCommand ------------------------------------------

    class SpawnEntityBranchCommand : public Command
    {
        nlohmann::json source_json{};
        nlohmann::json branch_json{};
        ecs::Entity parent_entity{};
        Guid parent_guid{};
        BatchId target_batch;
        std::vector<std::shared_future<bool>> attach_futures{};
        std::vector<std::shared_future<bool>> destroy_futures{};
        EngineContextWeakPtr ctx;
        std::string display_name;
        enum class AsyncStage { None, Attach, Destroy };
        AsyncStage async_stage{ AsyncStage::None };
        bool remap_guids{ true };
        bool prepared{ false };

    public:
        SpawnEntityBranchCommand(
            nlohmann::json branch_json,
            const ecs::Entity& parent_entity,
            EngineContextWeakPtr ctx,
            bool remap_guids = true);

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    // --- ReparentEntityBranchCommand ----------------------------------------

    class ReparentEntityBranchCommand : public Command
    {
        ecs::Entity entity;
        Guid entity_guid;
        Guid prev_parent_guid;
        ecs::Entity new_parent_entity;
        Guid new_parent_guid;
        EngineContextWeakPtr ctx;
        std::string display_name;

    public:
        ReparentEntityBranchCommand(
            const ecs::Entity& entity,
            const ecs::Entity& parent_entity,
            EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        std::string get_name() const override;
    };

    // --- UnparentEntityBranchCommand ----------------------------------------

// class UnparentEntityBranchCommand : public Command
// {
//     entt::entity entity = entt::null;
//     entt::entity prev_parent_entity = entt::null;
//     // entt::entity new_parent_entity = entt::null;
//     Context context;
//     std::string display_name;

// public:
//     UnparentEntityBranchCommand(
//         entt::entity entity,
//         entt::entity parent_entity,
//         const Context& context);

//     void execute() override;

//     void undo() override;

//     std::string get_name() const override;
// };

// --- AddComponentToEntityCommand ----------------------------------------

    class AddComponentToEntityCommand : public Command
    {
        ecs::Entity entity;
        Guid entity_guid;
        entt::id_type comp_id;
        EngineContextWeakPtr ctx;
        std::string display_name;

    public:
        AddComponentToEntityCommand(
            const ecs::Entity& entity,
            entt::id_type comp_id,
            EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        std::string get_name() const override;
    };

    // --- RemoveComponentFromEntityCommand -----------------------------------

    class RemoveComponentFromEntityCommand : public Command
    {
        ecs::Entity entity;
        Guid entity_guid;
        entt::id_type comp_id;
        nlohmann::json comp_json{};
        EngineContextWeakPtr ctx;
        std::string display_name;

    public:
        RemoveComponentFromEntityCommand(
            const ecs::Entity& entity,
            entt::id_type comp_id,
            EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        std::string get_name() const override;
    };

    // --- ImportModelCommand --------------------------------------------------

    class ImportModelCommand : public Command
    {
        std::filesystem::path source_file;
        assets::ImportFlags flags{};
        std::string model_name;
        EngineContextWeakPtr ctx;
        std::string display_name;
        std::shared_ptr<std::atomic<bool>> ui_in_flight;

        std::shared_future<TaskResult> future;
        bool in_flight{ false };

        enum class PendingAction : std::uint8_t { None, Import, Unimport, Restore };
        PendingAction pending_action{ PendingAction::None };

        std::vector<Guid> imported_roots;
        bool was_undone{ false };

    public:
        ImportModelCommand(
            std::filesystem::path source_file,
            assets::ImportFlags flags,
            std::string model_name,
            EngineContextWeakPtr ctx,
            std::shared_ptr<std::atomic<bool>> in_flight = {});

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

    // --- UnimportAssetsCommand ----------------------------------------------

    class UnimportAssetsCommand : public Command
    {
        std::vector<Guid> roots;
        EngineContextWeakPtr ctx;
        std::string display_name;

        std::shared_future<TaskResult> future;
        bool in_flight{ false };

        enum class PendingAction : std::uint8_t { None, Unimport, Restore };
        PendingAction pending_action{ PendingAction::None };

    public:
        UnimportAssetsCommand(
            std::vector<Guid> roots,
            EngineContextWeakPtr ctx);

        CommandStatus execute() override;

        CommandStatus undo() override;

        CommandStatus update() override;

        std::string get_name() const override;
    };

} // namespace Editor

#endif /* EditComponentCommand_hpp */
