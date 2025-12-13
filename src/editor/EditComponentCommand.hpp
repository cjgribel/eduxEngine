//
//  EditComponentCommand.hpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef EditComponentCommand_hpp
#define EditComponentCommand_hpp

#include "Command.hpp"
#include "editor/MetaFieldPath.hpp"
#include "ecs/Entity.hpp"
#include <entt/entt.hpp>

namespace eeng::editor
{
    class ComponentCommand : public Command
    {
        std::weak_ptr<entt::registry>   registry;
        eeng::ecs::Entity               entity;
        entt::id_type                   component_id = 0;
        MetaFieldPath                   meta_path{};
        entt::meta_any                  prev_value{}, new_value{};

        std::string display_name;
        friend class ComponentCommandBuilder;

        void assign_meta_field(entt::meta_any& value_any);

    public:
        void execute() override;

        void undo() override;

        std::string get_name() const override;
    };

    class ComponentCommandBuilder
    {
        ComponentCommand command;

    public:
        ComponentCommandBuilder& registry(std::weak_ptr<entt::registry> registry);

        ComponentCommandBuilder& entity(const ecs::Entity& entity);

        ComponentCommandBuilder& component(entt::id_type id);

        ComponentCommandBuilder& prev_value(const entt::meta_any& value);

        ComponentCommandBuilder& new_value(const entt::meta_any& value);

        ComponentCommandBuilder& push_path_data(entt::id_type id, const std::string& name);

        ComponentCommandBuilder& push_path_index(int index, const std::string& name);

        ComponentCommandBuilder& push_path_key(const entt::meta_any& key_any, const std::string& name);

        ComponentCommandBuilder& pop_path();

        ComponentCommandBuilder& reset();

        ComponentCommand build();
    };

} // namespace eeng::editor

#endif /* EditComponentCommand_hpp */
