// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "editor/MetaFieldPath.hpp"
#include <entt/entt.hpp>

namespace eeng::editor
{
    bool assign_meta_field_recursive(
        entt::meta_any& obj,
        const eeng::editor::MetaFieldPath& path,
        std::size_t idx,
        const entt::meta_any& leaf_value);

    bool assign_meta_field_stackbased(
        entt::meta_any& meta_any,
        const eeng::editor::MetaFieldPath& meta_path,
        const entt::meta_any& value_any);

    } // namespace eeng::editor