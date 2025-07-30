//
//  EditComponentCommand.hpp
//
//  Created by Carl Johan Gribel on 2024-12-01.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef GuiCommands_hpp
#define GuiCommands_hpp

#include <deque>
// #include <entt/entt.hpp>
// #include "Scene.hpp"
#include "Command.hpp"
#include "EditComponentCommand.hpp"

#include "MetaSerialize.hpp"
#include "Context.hpp"

namespace Editor {

    class CreateEntityCommand : public Command
    {
        Entity created_entity;
        Entity parent_entity;
        Context context;
        std::string display_name;

    public:
        CreateEntityCommand(
            const Entity& parent_entity,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // ------------------------------------------------------------------------

    class DestroyEntityCommand : public Command
    {
        Entity entity;
        nlohmann::json entity_json{};
        Context context;
        std::string display_name;

    public:
        DestroyEntityCommand(
            const Entity& entity,
            const Context& context
        );

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // --- CopyEntityCommand --------------------------------------------------

    class CopyEntityCommand : public Command
    {
        Entity entity_source;
        Entity entity_copy;
        Context context;
        std::string display_name;

    public:
        CopyEntityCommand(
            const Entity& entity,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // --- CopyEntityBranchCommand --------------------------------------------

    class CopyEntityBranchCommand : public Command
    {
        Entity root_entity;
        std::deque<Entity> source_entities; // top-down
        std::deque<Entity> copied_entities; // top-down
        Context context;
        std::string display_name;

    public:
        CopyEntityBranchCommand(
            const Entity& entity,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // --- ReparentEntityBranchCommand ----------------------------------------

    class ReparentEntityBranchCommand : public Command
    {
        Entity entity;
        Entity prev_parent_entity;
        Entity new_parent_entity;
        Context context;
        std::string display_name;

    public:
        ReparentEntityBranchCommand(
            const Entity& entity,
            const Entity& parent_entity,
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
        Entity entity;
        entt::id_type comp_id;
        Context context;
        std::string display_name;

    public:
        AddComponentToEntityCommand(
            const Entity& entity,
            entt::id_type comp_id,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    // --- RemoveComponentFromEntityCommand -----------------------------------

    class RemoveComponentFromEntityCommand : public Command
    {
        Entity entity;
        entt::id_type comp_id;
        nlohmann::json comp_json{};
        Context context;
        std::string display_name;

    public:
        RemoveComponentFromEntityCommand(
            const Entity& entity,
            entt::id_type comp_id,
            const Context& context);

        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

} // namespace Editor

#endif /* EditComponentCommand_hpp */
