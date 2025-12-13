// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include <entt/entt.hpp>

namespace eeng::editor 
{
    struct MetaFieldPath
    {
        struct Entry
        {
            enum class Type : int { None, Data, Index, Key } type = Type::None;

            entt::id_type data_id{ 0 };  // data field by type id
            int index{ -1 };             // sequential container by index
            entt::meta_any key_any{};  // associative container by key

            std::string name;           // 
        };
        std::vector<Entry> entries;
    };
} // namespace eeng::editor