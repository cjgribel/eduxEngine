// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "HeaderComponent.hpp"
#include <format>

namespace eeng::ecs
{
    HeaderComponent::HeaderComponent(
        const std::string& name,
        const std::string& chunk_tag,
        const Guid& guid,
        const Entity& entity_parent)
        : name(name)
        , chunk_tag(chunk_tag)
        , guid(guid)
        , entity_parent(entity_parent)
    {
    }

    // std::string eeng::ecs::Transform::to_string() const
    std::string to_string(const HeaderComponent& h)
    {
        return std::format("HeaderComponent(name = {} ...)", h.name);
    }
}