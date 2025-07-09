// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "Handle.h"
#include "Guid.h"
#include <nlohmann/json.hpp>

namespace eeng
{
    class AssetIndex
    {
        // Maps asset type to a file location: a) templated or b) entt::meta_type
        // asset_index.serialize<T>(t, guid, ctx?);
        void serialize()
        {
            /*
            {
                "guid": "acdb01b9-f34e-4c68-818a-98eabc22f54e",
                "resource" : {
                    "name": "TestModel",
                    "meshes" : [
                        { "guid": "mesh1" },
                        { "guid": "mesh2" }
                    ]
                }
                }
            }
*/
        }

        // Maps asset type to a file location: a) templated or b) entt::meta_type
        void deserialize()
        {
#if 0
            // Deserialize file
            Guid guid = json["guid"];
            Model model = deserialize<Model>(json["resource"]);

            // Wrap in AssetRef
            AssetRef<Model> model_ref{ guid, {} };

            // Register resource to storage
            model_ref.handle = storage.insert(model, guid);
#endif
        }
    };

#if 0
    class AssetIndex
    {
    public:
        void add_asset(Guid guid, const AssetMetadata& meta)
        {
            std::lock_guard lock(mutex_);
            assets_[guid] = meta;
        }

        // template<typename T>
        // handle<T> load(Guid guid)
        // {
        //     if (auto existing = storage.get_handle<T>(guid))
        //         return existing;

        //     auto serialized_data = deserialize_from_disk<T>(guid); // returns object with only GUIDs
        //     handle<T> h = storage.insert<T>(guid, std::move(serialized_data));

        //     resolve_handles(h); // 🔥 critical step
        //     return h;
        // }
        std::optional<AssetMetadata> get_asset(Guid guid) const
        {
            std::shared_lock lock(mutex_);
            if (auto it = assets_.find(guid); it != assets_.end())
                return it->second;
            return {};
        }

    private:
        mutable std::shared_mutex mutex_;
        std::unordered_map<Guid, AssetMetadata> assets_;
    };
#endif

#if 0
    class AssetIndex
    {
    public:
        // Initialization
        void initialize(const fs::path& meta_folder);

        // Asset Management (Add/Remove)
        template<typename T>
        Guid add_asset(const std::string& original_name, const T& asset_data, const ImportSettings& settings);

        bool remove_asset(const Guid& guid);

        // Querying Assets
        std::optional<Guid> find_guid_by_name(const std::string& asset_name, ResourceType type) const;
        std::vector<AssetEntry> list_assets(ResourceType type) const;

        // Loading/Deserialization
        template<typename T>
        std::optional<T> load_asset(const Guid& guid) const;

        // Batch operations (future concurrency support)
        template<typename T>
        std::vector<std::optional<T>> load_assets(const std::vector<Guid>& guids) const;
    };
#endif

#if 0
    class AssetDatabase {
        fs::path meta_root;

    public:
        AssetDatabase(const fs::path& project_path)
            : meta_root(project_path / "meta")
        {
        }

        template<typename T>
        Guid import_and_serialize(const std::string& original_name, const T& resource) {
            Guid guid = create_guid();
            fs::path type_folder = meta_root / resource_type_folder<T>();
            fs::create_directories(type_folder);

            fs::path resource_path = type_folder / (guid.to_string() + resource_extension<T>());
            fs::path meta_path = resource_path;
            meta_path += ".meta";

            serialize_to_disk(resource, resource_path);

            nlohmann::json meta_json = {
                {"guid", guid.to_string()},
                {"original_name", original_name}
            };
            std::ofstream meta_file(meta_path);
            meta_file << meta_json.dump(4);

            return guid;
        }

        template<typename T>
        std::optional<T> deserialize_resource(const Guid& guid) const {
            fs::path resource_path = meta_root / resource_type_folder<T>() / (guid.to_string() + resource_extension<T>());
            return deserialize_from_disk<T>(resource_path);
        }

    private:
        template<typename T>
        std::string resource_type_folder() const; // specialize per type

        template<typename T>
        std::string resource_extension() const;   // specialize per type

        Guid create_guid() const {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            uint64_t high = gen(), low = gen();
            return Guid(high, low);
        }

        template<typename T>
        void serialize_to_disk(const T& resource, const fs::path& path) {
            std::ofstream ofs(path, std::ios::binary);
            resource.serialize(ofs);
        }

        template<typename T>
        std::optional<T> deserialize_from_disk(const fs::path& path) const {
            if (!fs::exists(path)) return std::nullopt;
            std::ifstream ifs(path, std::ios::binary);
            T resource;
            resource.deserialize(ifs);
            return resource;
        }
    };
#endif

} // namespace eeng