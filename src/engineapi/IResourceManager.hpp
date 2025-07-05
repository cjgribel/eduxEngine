// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"

#pragma once

namespace eeng
{
    template<class T>
    struct AssetRef
    {
        static_assert(!std::is_reference_v<T>, "AssetRef<T> cannot use reference types");

        Guid guid;
        Handle<T> handle;

        // Optional helpers
        bool is_loaded() const { return bool(handle); }
        void load(Handle<T> handle) { this->handle = handle; }
        void unload() { handle.reset(); }
        Guid get_guid() const { return guid; }
        Handle<T> get_handle() const { return handle; }
    };

    class IResourceManager
    {
    public:
        virtual std::string to_string() const = 0;
        virtual ~IResourceManager() = default;
    };
} // namespace eeng