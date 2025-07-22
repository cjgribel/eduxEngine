// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"
#include <string>

#pragma once

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
        {
        }

        // Optional helpers
        bool is_loaded() const { return bool(handle); }         // -> is_referant
        void load(Handle<T> handle) { this->handle = handle; }  // -> set_reference
        void unload() { handle.reset(); }                       // -> unreference
        Guid get_guid() const { return guid; }
        Handle<T> get_handle() const { return handle; }
    };
}