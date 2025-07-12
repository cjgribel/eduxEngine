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
        bool is_loaded() const { return bool(handle); }
        void load(Handle<T> handle) { this->handle = handle; }
        void unload() { handle.reset(); }
        Guid get_guid() const { return guid; }
        Handle<T> get_handle() const { return handle; }
    };

    template<typename T, typename Visitor>
    void visit_asset_refs(T&, Visitor&&)
    {
        // No-op for asset types with no AssetRef dependencies
    }

    struct AssetMetaData
    {
        Guid guid;
        Guid guid_parent;

        std::string name;               // Tank.fbx
        //std::string type_display_name;  // "Model", "Mesh", "Texture2D", etc.
        std::string type_name; // meta_type.info().name() / entt::type_name<T>. E.g., "eeng::mock::Model"
        std::string file_path;

        // bool is_collection; ???

        AssetMetaData() = default;
        AssetMetaData(
            Guid guid,
            Guid guid_parent,
            std::string name,
            // std::string type_display_name,
            std::string type_name,
            std::string file_path)
            : guid(guid)
            , guid_parent(guid_parent)
            , name(std::move(name))
            // , type_display_name(std::move(type_display_name))
            , type_name(std::move(type_name))
            , file_path(std::move(file_path))
        {
        }   
        //     eeng::LogGlobals::log("AssetMetadata created: guid=%s, name=%s, type=%s, filepath=%s",
        //         guid.raw(), name.c_str(), type.c_str(), filepath.c_str());
        // }
    };
}

namespace eeng
{
    class IResourceManager
    {
    public:
        virtual std::string to_string() const = 0;
        virtual ~IResourceManager() = default;
    };
} // namespace eeng