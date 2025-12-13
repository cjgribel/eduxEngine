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
#include "AssignComponentFieldCommand.hpp"
#include "MetaSerialize.hpp"
#include <deque>

namespace eeng::editor {

    class CreateEntityCommand : public Command
    {
        ecs::Entity created_entity;
        ecs::Entity parent_entity;
        std::weak_ptr<EngineContext> ctx;
        std::string display_name;

    public:
        CreateEntityCommand(
            const ecs::Entity& parent_entity,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // ------------------------------------------------------------------------

    class DestroyEntityCommand : public Command
    {
        ecs::Entity entity;
        nlohmann::json entity_json{};
        Context context;
        std::string display_name;

    public:
        DestroyEntityCommand(
            const ecs::Entity& entity,
            const Context& context
        );

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // --- CopyEntityCommand --------------------------------------------------

    class CopyEntityCommand : public Command
    {
        ecs::Entity entity_source;
        ecs::Entity entity_copy;
        Context context;
        std::string display_name;

    public:
        CopyEntityCommand(
            const ecs::Entity& entity,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // --- CopyEntityBranchCommand --------------------------------------------

    class CopyEntityBranchCommand : public Command
    {
        ecs::Entity root_entity;
        std::deque<ecs::Entity> source_entities; // top-down
        std::deque<ecs::Entity> copied_entities; // top-down
        Context context;
        std::string display_name;

    public:
        CopyEntityBranchCommand(
            const ecs::Entity& entity,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // --- ReparentEntityBranchCommand ----------------------------------------

    class ReparentEntityBranchCommand : public Command
    {
        ecs::Entity entity;
        ecs::Entity prev_parent_entity;
        ecs::Entity new_parent_entity;
        Context context;
        std::string display_name;

    public:
        ReparentEntityBranchCommand(
            const ecs::Entity& entity,
            const ecs::Entity& parent_entity,
            const Context& context);

        void execute() override;

        void undo() override;

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
        entt::id_type comp_id;
        Context context;
        std::string display_name;

    public:
        AddComponentToEntityCommand(
            const ecs::Entity& entity,
            entt::id_type comp_id,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // --- RemoveComponentFromEntityCommand -----------------------------------

    class RemoveComponentFromEntityCommand : public Command
    {
        ecs::Entity entity;
        entt::id_type comp_id;
        nlohmann::json comp_json{};
        Context context;
        std::string display_name;

    public:
        RemoveComponentFromEntityCommand(
            const ecs::Entity& entity,
            entt::id_type comp_id,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

} // namespace Editor

#endif /* EditComponentCommand_hpp */
