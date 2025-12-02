// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"
#include <string>

#pragma once

namespace eeng
{
    // Fallback for types without entity refs.
    // Overload `visit_entity_refs(MyType&, Visitor&&)` in MyType's namespace,
    // then call `visit_entity_refs(obj, visitor);` unqualified so ADL finds it.
    template<typename T, typename Visitor>
    void visit_asset_refs(T&, Visitor&&) 
    {
        // Default: no assets referenced
    }
}

namespace eeng
{
    template<class T>
    struct AssetRef
    {
        static_assert(!std::is_reference_v<T>, "AssetRef<T> cannot use reference types");

        Guid guid;
        Handle<T> handle;

        AssetRef() = default;
        AssetRef(
            Guid guid,
            Handle<T> handle = Handle<T>())
            : guid(guid)
            , handle(handle)
        { }

        bool is_bound() const { return bool(handle); }
        void bind(Handle<T> handle) { this->handle = handle; }
        void unbind() { handle.reset(); }
        
        //Guid get_guid() const { return guid; }
        // Handle<T> get_handle() const { return handle; }
    };
}