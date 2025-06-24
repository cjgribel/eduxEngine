// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IResourceManager.hpp"

namespace eeng
{
    /*
    Avoid exposing Storage or AssetRegistry directly.

    Instead, introduce a facade like ResourceManager or EngineAssets, that:

    Encapsulates:

    Storage

    AssetRegistry

    (maybe) a ThreadPool for loading


    This layer should be thread-safe and asynchronous-aware 
    (i.e. return futures or use task submission when loading/importing).
    */

    class ResourceManager : public IResourceManager
    {
        
    };

#if 0
    // Read
    std::vector<AssetInfo> list_assets_of_type(entt::meta_type);
    AssetInfo get_asset_info(Guid guid);
    bool is_loaded(Guid guid);
    entt::meta_any get_loaded_resource(Guid guid);

    // Write
    bool import_resource(const std::filesystem::path& source);
    bool reimport_resource(Guid guid);
    Guid load_resource(Guid guid);
    void unload_resource(Guid guid);
    bool serialize_resource(Guid guid);
#endif
} // namespace eeng