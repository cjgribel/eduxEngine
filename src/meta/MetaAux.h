//
//  meta_aux.h
//
//  Created by Carl Johan Gribel on 2024-08-08.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef any_aux_h
#define any_aux_h

#include <entt/entt.hpp>
#include <tuple>
#include <utility>
#include <cassert>
#include "MetaLiterals.h"
#include "MetaInfo.h"
#include "meta/TypeIdRegistry.hpp"

namespace internal
{

    // Or maybe try_visit
    template<typename T, typename Callable>
    bool try_apply(entt::meta_any& value, Callable callable)
    {
        if (T* ptr = value.try_cast<T>(); ptr) {
            callable(*ptr);
            return true;
        }
        if (const T* ptr = value.try_cast<const T>(); ptr) {
            callable(*ptr);
            return true;
        }
        return false;
    }

    template<typename T, typename Callable>
    bool try_apply(const entt::meta_any& value, Callable callable)
    {
        if (const T* ptr = value.try_cast<T>(); ptr) {
            callable(*ptr);
            return true;
        }
        if (const T* ptr = value.try_cast<const T>(); ptr) {
            callable(*ptr);
            return true;
        }
        return false;
    }

    template<typename Callable, typename... Types>
    bool any_apply_impl(entt::meta_any& value, Callable callable, std::tuple<Types...>)
    {
        bool result = false;
        (... || (result = try_apply<Types>(value, callable)));
        return result;
    }

    template<typename Callable, typename... Types>
    bool any_apply_impl(const entt::meta_any& value, Callable callable, std::tuple<Types...>)
    {
        bool result = false;
        (... || (result = try_apply<Types>(value, callable)));
        return result;
    }

    using TryCastTypes = std::tuple<
        bool,
        char, unsigned char,
        short, unsigned short,
        int, unsigned int,
        long, unsigned long,
        long long, unsigned long long,
        float, double, long double,

        // string-like types:
        std::string,
        std::string_view
        // const char*
    >;

} // internal

template<typename Callable>
bool try_apply(entt::meta_any& value, Callable callable)
{
    return internal::any_apply_impl(value, callable, internal::TryCastTypes{});
}

/// Tries to convert argument to a primitive type and, if successfull, applies a function for it
template<typename Callable>
bool try_apply(const entt::meta_any& value, Callable callable)
{
    return internal::any_apply_impl(value, callable, internal::TryCastTypes{});
}

namespace eeng::meta
{
    inline bool type_is_registered(entt::meta_type mt)
    {
        return static_cast<bool>(mt);
    }

    template<typename T>
    inline bool type_is_registered()
    {
        return type_is_registered(entt::resolve<T>());
    }

    inline std::string get_meta_type_display_name(entt::meta_type mt)
    {
        assert(mt);
        if (eeng::TypeMetaInfo* info = mt.custom(); info && !info->name.empty())
            return info->name;

        // Fallback: raw type name from entt
        return std::string{ mt.info().name() };
    }

    template<typename T>
    inline std::string get_meta_type_display_name()
    {
        entt::meta_type mt = entt::resolve<T>();
        assert(mt);
        return get_meta_type_display_name(mt);
    }

    inline std::string get_meta_type_id_string(entt::meta_type mt)
    {
        assert(mt);

        eeng::TypeMetaInfo* info = mt.custom();
        assert(info && !info->id.empty());  // Ensure non-empty registered id string

        return info->id;

        // if (eeng::TypeMetaInfo* info = mt.custom(); info && !info->id.empty())
        //     return info->id;                         // "eeng.RegType"

        // // Fallback: raw type name from entt
        // return std::string{ mt.info().name() };
    }

    template<typename T>
    inline std::string get_meta_type_id_string()
    {
        entt::meta_type mt = entt::resolve<T>();
        assert(mt);
        return get_meta_type_id_string(mt);
    }

    // Global map of registered type id strings to entt type ids
    // inline std::unordered_map<std::string, entt::id_type>& type_id_map()
    // {
    //     static std::unordered_map<std::string, entt::id_type> map;
    //     return map;
    // }

    template<class T>
    entt::id_type register_type()
    {
        return TypeIdRegistry::register_type_from_meta<T>();
    }

    // (deserialize)
    inline entt::meta_type resolve_by_type_id_string(std::string_view id)
    {
        return TypeIdRegistry::resolve(id);

        // auto& map = type_id_map();
        // auto it = map.find(std::string{ id });
        // if (it == map.end())
        //     return {};
        // return entt::resolve(it->second);
    }

    inline entt::id_type get_meta_type_id(entt::meta_type mt)
    {
        assert(mt);
        return mt.id();   // result of .type("...") or type_hash<T>::value()
    }

    template<typename T>
    inline entt::id_type get_meta_type_id()
    {
        entt::meta_type mt = entt::resolve<T>();
        assert(mt);
        return mt.id();
    }
}

#if 0
/// @brief Get inspector-friendly name of a meta_type
/// @param meta_type 
/// @return Name provided as a display name property, or default name
inline auto get_meta_type_name(const entt::meta_type meta_type)
{
    assert(meta_type);
    eeng::TypeMetaInfo* type_info = meta_type.custom();
    if (type_info)
        return type_info->name;
    return std::string(meta_type.info().name());
}

template<typename T>
inline auto get_meta_type_name()
{
    // Or: use entt::type_name<T>(), and don't require type to be registered?

    entt::meta_type meta_type = entt::resolve<T>();
    assert(meta_type);
    return get_meta_type_name(meta_type);
}
#endif

/// @brief Get name of a meta data field
/// @param id Data field id, used as fallback
/// @param data Meta data field
/// @return Name provided as a name property, or string generated from id
inline auto get_meta_data_name(
    const entt::id_type& id,
    const entt::meta_data& meta_data)
{
    // Note: data.type().info().name() gives type name, not the field name

    assert(meta_data);
    eeng::DataMetaInfo* data_info = meta_data.custom();
    if (data_info)
        return data_info->name;
    return std::to_string(id);
}

/// @brief Get nice name of a meta data field
/// @param id Data field id, used as fallback
/// @param data Meta data field
/// @return Name provided as a nice name property, or string generated from id
inline auto get_meta_data_display_name(
    const entt::id_type& id,
    const entt::meta_data& meta_data)
{
    // Note: data.type().info().name() gives type name, not the field name

    assert(meta_data);
    eeng::DataMetaInfo* data_info = meta_data.custom();
    if (data_info)
        return data_info->nice_name;
    return std::to_string(id);
}

inline auto cast_to_underlying_type(const entt::meta_type& meta_type, const entt::meta_any& enum_any)
{
    assert(meta_type.is_enum());
    eeng::TypeMetaInfo* enum_info = meta_type.custom();
    assert(enum_info);
    assert(enum_info->underlying_type);
    return enum_any.allow_cast(enum_info->underlying_type);
}

inline auto gather_meta_enum_entries(const entt::meta_any& enum_any)
{
    entt::meta_type meta_type = entt::resolve(enum_any.type().id());
    assert(meta_type);
    assert(meta_type.is_enum());
    std::vector<std::pair<const std::string, const entt::meta_any>> entries;

    for (auto [id_type, meta_data] : meta_type.data())
    {
        eeng::EnumDataMetaInfo* data_info = meta_data.custom();
        assert(data_info);

        entries.push_back({
            data_info->name,
            cast_to_underlying_type(meta_type, meta_data.get(enum_any))
            });
    }
    return entries;
}

inline std::string enum_value_name(const entt::meta_any& enum_any)
{
    entt::meta_type meta_type = enum_any.type();
    assert(meta_type.is_enum());

    // Linear search for entry with current value
    auto any_conv = cast_to_underlying_type(meta_type, enum_any);
    auto enum_entries = gather_meta_enum_entries(enum_any);
    auto entry = std::find_if(enum_entries.begin(), enum_entries.end(), [&any_conv](auto& e)
        {
            return e.second == any_conv;
        });
    assert(entry != enum_entries.end());

    return entry->first;
}

#endif /* meta_aux */
