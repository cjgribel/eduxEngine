// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

// #include <iostream>
#include <entt/entt.hpp>
#include <entt/meta/pointer.hpp>
#include <nlohmann/json.hpp>
#include "config.h"
#include "ComponentMetaReg.hpp"
#include "ResourceTypes.hpp"
#include "MetaLiterals.h"
//#include "Storage.hpp"
#include "MetaInfo.h"
// #include "IResourceManager.hpp" 
//#include "ResourceManager.hpp" // For AssetRef<T>, AssetMetaData, ResourceManager::load<>/unload

namespace eeng {

    namespace
    {
        //
        // Standard component meta functions
        //

#if 0
        // Collect referenced GUIDs for Assets or Components
        template<typename T>
        void collect_guids(T& t, std::unordered_set<Guid>& out_guids)
        {
            visit_assets(t, [&](const auto& asset_ref) {
                out_guids.insert(asset_ref.guid);
                });
        }
#endif

        template<typename T>
        void register_component()
        {
            // constexpr auto alias = entt::type_name<T>::value();  // e.g. "ResourceTest"
            // constexpr auto id    = entt::type_hash<T>::value();

            entt::meta_factory<T>()
                ;

            // AssetRef<T>
            // entt::meta_factory<AssetRef<T>>{}
            // .template data<&AssetRef<T>::guid>("guid"_hs)
            //     .template custom<DataMetaInfo>(DataMetaInfo{ "guid", "Guid", "A globally unique identifier." })
            //     .traits(MetaFlags::read_only)
            //     ;
        }
    } // namespace

#if 0
    namespace
    {
        // Guid to and from json
        void serialize_Guid(nlohmann::json& j, const entt::meta_any& any)
        {
            auto ptr = any.try_cast<Guid>();
            assert(ptr && "serialize_Guid: could not cast meta_any to Guid");
            j = ptr->raw();
        }

        void deserialize_Guid(const nlohmann::json& j, entt::meta_any& any)
        {
            auto ptr = any.try_cast<Guid>();
            assert(ptr && "deserialize_Guid: could not cast meta_any to Guid");
            *ptr = Guid{ j.get<uint64_t>() };
        }
    } // namespace
#endif

    void register_component_meta_types()
    {
        // register_component<>();
    }

} // namespace eeng