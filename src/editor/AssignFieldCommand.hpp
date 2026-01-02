//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.

#ifndef EditComponentCommand_hpp
#define EditComponentCommand_hpp

#include "Command.hpp"
#include "EngineContext.hpp"
#include "engineapi/IResourceManager.hpp"
#include "editor/MetaFieldPath.hpp"
#include "ecs/Entity.hpp"
#include <entt/entt.hpp>

namespace eeng::editor
{
    struct FieldTarget
    {
        enum class Kind : uint8_t
        {
            None,
            Component,
            Asset
            // (Later) ObjectPtr, Entity, etc.
        };

        Kind kind = Kind::None;

        // Component target
        std::weak_ptr<entt::registry> registry;
        eeng::ecs::Entity entity{};
        Guid entity_guid{};
        EngineContextWeakPtr ctx;
        entt::id_type component_id{ 0 };

        // Asset target
        std::weak_ptr<IResourceManager> resource_manager;
        Guid asset_guid{};
        std::string asset_type_id_str{};
        //entt::id_type asset_type_id{ 0 }; // type of the asset root
    };

    struct FieldEdit
    {
        FieldTarget target;
        MetaFieldPath meta_path;

        entt::meta_any prev_value;
        entt::meta_any new_value;

        // Optional convenience (not required)
        entt::id_type leaf_data_id{ 0 }; // ? leaf can be an index or a key
        std::string display_name;
    };

    class AssignFieldCommand : public Command
    {
        // std::weak_ptr<entt::registry>   registry;
        // eeng::ecs::Entity               entity;
        // entt::id_type                   component_id = 0;
        // MetaFieldPath                   meta_path{};
        // entt::meta_any                  prev_value{}, new_value{};

        // std::string display_name;
        FieldEdit edit;

        friend class AssignFieldCommandBuilder;

        // void assign_meta_field(entt::meta_any& value_any);

    public:
        CommandStatus execute() override;

        CommandStatus undo() override;

        std::string get_name() const override;
    };

    class AssignFieldCommandBuilder
    {
        AssignFieldCommand command;

        bool validate_meta_path();

        std::string meta_path_to_string();

        AssignFieldCommand build_component_command();

        AssignFieldCommand build_asset_command();

    public:
        AssignFieldCommandBuilder& target_component(EngineContext& ctx, const ecs::Entity& entity, entt::id_type component_id);

        AssignFieldCommandBuilder& target_asset(EngineContext& ctx, std::weak_ptr<IResourceManager> resource_manager, Guid asset_guid, const std::string& asset_type_id_str);

        AssignFieldCommandBuilder& prev_value(const entt::meta_any& value);

        AssignFieldCommandBuilder& new_value(const entt::meta_any& value);

        AssignFieldCommandBuilder& push_path_data(entt::id_type id, const std::string& name);

        AssignFieldCommandBuilder& push_path_index(int index, const std::string& name);

        AssignFieldCommandBuilder& push_path_key(const entt::meta_any& key_any, const std::string& name);

        AssignFieldCommandBuilder& pop_path();

        // AssignComponentFieldCommandBuilder& reset();

        AssignFieldCommand build();
    };

} // namespace eeng::editor

#endif /* EditComponentCommand_hpp */
