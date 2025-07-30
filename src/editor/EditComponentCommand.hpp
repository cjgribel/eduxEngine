//
//  EditComponentCommand.hpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef EditComponentCommand_hpp
#define EditComponentCommand_hpp

#include <entt/entt.hpp>
#include "Command.hpp"

namespace eeng::editor {

    struct MetaPath
    {
        struct Entry
        {
            enum class Type : int { None, Data, Index, Key } type = Type::None;

            entt::id_type data_id {0};  // data field by type id
            int index {-1};             // sequential container by index
            entt::meta_any key_any {};  // associative container by key

            std::string name;           // 
        };
        std::vector<Entry> entries;
    };

    class ComponentCommand : public Command
    {
        std::weak_ptr<entt::registry>   registry;
        ecs::Entity                     entity;
        entt::id_type                   component_id = 0;
        MetaPath                        meta_path{};
        entt::meta_any                  prev_value{}, new_value{};

        std::string display_name;
        friend class ComponentCommandBuilder;

        void traverse_and_set_meta_type(entt::meta_any& value_any);

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
