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
        // "BIND COMPONENT / TYPE"
        // -> ResourceManager
        
        // - CALLER collects component closure (e.g. nested references)
        //      Set of meta_any? Safe if comp type is registered?
        // - NO RECURSION in the bind function

        template<typename T>
        void resolve_component_meta(entt::meta_any& any, ResourceManager& rm)
        {
            T& obj = any.cast<T&>();
            rm.resolve_component(obj);
        }
        // Calls ...
        template<typename T>
        void resolve_component(T& component)
        {
            visit_assets(component, [&](auto& ref)
                {
                    using AssetT = typename std::decay_t<decltype(ref)>::asset_type;

                    auto handle = storage_->handle_for_guid<AssetT>(ref.guid);
                    if (!handle)
                        throw std::runtime_error("Unresolved reference: " + ref.guid.to_string());

                    ref.handle = *handle;
                    retain_guid(ref.guid); // ❗Component owns this asset
                });
#endif

            // "FOR ALL COMPONENTS OF AN ENTITY"
            // -> EntityManager
#if 0
            void resolve_all_components_for_entity(entt::registry & registry, entt::entity entity, ResourceManager & rm)
            {
                for (auto&& [id, storage] : registry.storage())
                {
                    if (!storage.contains(entity))
                        continue;

                    entt::meta_type type = entt::resolve(id);
                    if (!type)
                        continue;

                    entt::meta_func resolve_func = type.func("resolve_component"_hs);
                    if (!resolve_func)
                        continue;

                    entt::meta_any instance = type.from_void(storage.value(entity));
                    if (!instance)
                        continue;

                    resolve_func.invoke({}, instance, std::ref(rm));
                }
            }
#endif

#if 0
            // Collect referenced GUIDs for Assets or Components
            template<typename T>
            void collect_guids(T & t, std::unordered_set<Guid>&out_guids)
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
                    // .func<&resolve_component_meta<T>>(hashed_string("resolve_component"))
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