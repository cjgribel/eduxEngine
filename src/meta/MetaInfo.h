// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef MetaInfo_h
#define MetaInfo_h

#include <iostream>
#include <entt/entt.hpp>

namespace eeng
{
    struct TypeMetaInfo
    {
        std::string id;
        std::string name;
        std::string tooltip;

        // For enum types
        entt::meta_type underlying_type;
    };

    struct DataMetaInfo
    {
        std::string name;
        std::string nice_name;
        std::string tooltip;
    };

    // struct EnumTypeMetaInfo
    // {
    //     std::string name;
    //     std::string tooltip;
    //     entt::meta_type underlying_type;
    // };

    struct EnumDataMetaInfo
    {
        std::string name;
        std::string tooltip;
    };

    struct FuncMetaInfo
    {
        std::string name;
        std::string tooltip;
    };

    enum class MetaFlags : std::uint16_t
    {
        none = 0,
        read_only = 1 << 0,
        hidden = 1 << 1,
    };

    constexpr MetaFlags operator|(MetaFlags lhs, MetaFlags rhs)
    {
        return static_cast<MetaFlags>(
            static_cast<std::uint16_t>(lhs) |
            static_cast<std::uint16_t>(rhs)
            );
    }

    constexpr MetaFlags operator&(MetaFlags lhs, MetaFlags rhs)
    {
        return static_cast<MetaFlags>(
            static_cast<std::uint16_t>(lhs) &
            static_cast<std::uint16_t>(rhs)
            );
    }

    constexpr bool any(MetaFlags flags)
    {
        return static_cast<std::uint16_t>(flags) != 0;
    }

    constexpr bool has_flag(MetaFlags flags, MetaFlags flag)
    {
        return (flags & flag) != MetaFlags::none;
    }

} // namespace eeng

#endif // MetaInfo_h