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
        readonly_inspection = 1 << 0,  // no editing / TODO -> no cloning (const meta_any -> inspect_*)
        no_inspection = 1 << 1,        // skip inspection
        no_serialize = 1 << 2,         // skip serialization for all purposes
        no_serialize_file = 1 << 3,    // skip serialization for file persistence
        no_serialize_undo = 1 << 4,    // skip serialization for undo/redo
        no_serialize_display = 1 << 5, // skip serialization for display
    };

    namespace meta
    {
        enum class SerializationPurpose : std::uint8_t
        {
            generic = 0,
            file,
            undo,
            display,
        };
    } // namespace meta

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
