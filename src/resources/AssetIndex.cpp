// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "AssetIndex.hpp"
#include "EngineContext.hpp"
#include "MetaSerialize.hpp"
#include <fstream>

namespace eeng
{
    std::vector<AssetMetaData> AssetIndex::scan_meta_files(
        const std::filesystem::path& root, 
        EngineContext& ctx)
    {
        std::vector<AssetMetaData> assets;

        for (auto& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().ends_with(".meta.json"))
            {
                std::ifstream in(entry.path());
                if (!in) continue;

                nlohmann::json json;
                in >> json;
                
                // Convert json to AssetMetaData and push_back
                entt::meta_any any = AssetMetaData {};
                meta::deserialize_any(json, any, Entity{}, ctx);
                assets.push_back(any.cast<AssetMetaData>());
            }
        }

        return assets;
    }
} // namespace eeng
