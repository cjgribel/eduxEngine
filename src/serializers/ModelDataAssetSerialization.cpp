// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "serializers/ModelDataAssetSerialization.hpp"
#include "serializers/GLMSerialize.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>
#include <vector>

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

#include "MetaLiterals.h"
#include "assets/types/ModelAssets.hpp"

namespace eeng::serializers
{
    namespace
    {
        template<typename T>
        nlohmann::json serialize_asset_ref(const AssetRef<T>& ref)
        {
            return nlohmann::json{ { "guid", ref.guid.raw() } };
        }

        template<typename T>
        AssetRef<T> deserialize_asset_ref(const nlohmann::json& j)
        {
            if (j.is_object() && j.contains("guid"))
            {
                return AssetRef<T>{ Guid{ j["guid"].get<Guid::underlying_type>() } };
            }
            if (j.is_number_integer() || j.is_number_unsigned())
            {
                return AssetRef<T>{ Guid{ j.get<Guid::underlying_type>() } };
            }
            return AssetRef<T>{};
        }

        nlohmann::json serialize_skin_array(const std::vector<assets::SkinData>& values)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            arr.reserve(values.size());
            for (const auto& skin : values)
            {
                nlohmann::json elem;
                elem["bone_indices"] = skin.bone_indices;
                elem["bone_weights"] = skin.bone_weights;
                arr.emplace_back(std::move(elem));
            }
            return j;
        }

        void deserialize_skin_array(const nlohmann::json& j, std::vector<assets::SkinData>& values)
        {
            values.clear();
            if (!j.is_array())
                return;
            values.resize(j.size());
            for (size_t i = 0; i < j.size(); i++)
            {
                const auto& elem = j[i];
                auto& skin = values[i];
                if (elem.contains("bone_indices"))
                    skin.bone_indices = elem["bone_indices"].get<std::array<assets::u32, assets::bones_per_vertex>>();
                if (elem.contains("bone_weights"))
                    skin.bone_weights = elem["bone_weights"].get<std::array<float, assets::bones_per_vertex>>();
            }
        }

        nlohmann::json serialize_submeshes(const std::vector<assets::SubMesh>& values)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            arr.reserve(values.size());
            for (const auto& sm : values)
            {
                nlohmann::json elem;
                elem["base_index"] = sm.base_index;
                elem["nbr_indices"] = sm.nbr_indices;
                elem["base_vertex"] = sm.base_vertex;
                elem["nbr_vertices"] = sm.nbr_vertices;
                elem["node_index"] = sm.node_index;
                elem["is_skinned"] = sm.is_skinned;
                elem["material"] = serialize_asset_ref(sm.material);
                arr.emplace_back(std::move(elem));
            }
            return j;
        }

        void deserialize_submeshes(const nlohmann::json& j, std::vector<assets::SubMesh>& values)
        {
            values.clear();
            if (!j.is_array())
                return;
            values.resize(j.size());
            for (size_t i = 0; i < j.size(); i++)
            {
                const auto& elem = j[i];
                auto& sm = values[i];
                sm.base_index = elem.value("base_index", 0u);
                sm.nbr_indices = elem.value("nbr_indices", 0u);
                sm.base_vertex = elem.value("base_vertex", 0u);
                sm.nbr_vertices = elem.value("nbr_vertices", 0u);
                sm.node_index = elem.value("node_index", assets::null_index);
                sm.is_skinned = elem.value("is_skinned", false);
                if (elem.contains("material"))
                    sm.material = deserialize_asset_ref<assets::MaterialAsset>(elem["material"]);
                else
                    sm.material = AssetRef<assets::MaterialAsset>{};
            }
        }

        nlohmann::json serialize_nodetree(const VecTree<assets::SkeletonNode>& tree)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            arr.resize(tree.size());
            tree.traverse_depthfirst([&](const assets::SkeletonNode* node,
                const assets::SkeletonNode*,
                size_t node_index,
                size_t parent_index)
                {
                    nlohmann::json elem;
                    elem["name"] = node->name;
                    elem["local_bind_tfm"] = serialize_mat4(node->local_bind_tfm);
                    elem["bone_index"] = node->bone_index;
                    elem["nbr_meshes"] = node->nbr_meshes;
                    const int parent_out = (parent_index == static_cast<size_t>(VecTree_NullIndex))
                        ? VecTree_NullIndex
                        : static_cast<int>(parent_index);
                    elem["parent_index"] = parent_out;
                    arr[node_index] = std::move(elem);
                });
            return j;
        }

        void deserialize_nodetree(const nlohmann::json& j, VecTree<assets::SkeletonNode>& tree)
        {
            tree.clear();
            if (!j.is_array())
                return;
            const size_t count = j.size();
            std::vector<assets::SkeletonNode> nodes(count);
            std::vector<int> parents(count, VecTree_NullIndex);
            std::vector<std::vector<size_t>> children(count);
            std::vector<size_t> roots;
            for (size_t i = 0; i < count; i++)
            {
                const auto& elem = j[i];
                assets::SkeletonNode node{};
                node.name = elem.value("name", "");
                if (elem.contains("local_bind_tfm"))
                    node.local_bind_tfm = deserialize_mat4(elem["local_bind_tfm"]);
                node.bone_index = elem.value("bone_index", assets::null_index);
                node.nbr_meshes = elem.value("nbr_meshes", 0);
                nodes[i] = std::move(node);
                const int parent_index = elem.value("parent_index", VecTree_NullIndex);
                parents[i] = parent_index;
                if (parent_index == VecTree_NullIndex || parent_index < 0 ||
                    static_cast<size_t>(parent_index) >= nodes.size())
                {
                    roots.push_back(i);
                }
                else
                {
                    children[static_cast<size_t>(parent_index)].push_back(i);
                }
            }

            tree.reserve(count);
            auto insert_subtree = [&](size_t node_index, auto&& self) -> void
            {
                const int parent_index = parents[node_index];
                if (parent_index == VecTree_NullIndex || parent_index < 0 ||
                    static_cast<size_t>(parent_index) >= nodes.size())
                {
                    tree.insert_as_root(nodes[node_index]);
                }
                else
                {
                    const auto parent_idx = static_cast<size_t>(parent_index);
                    assets::SkeletonNode parent{};
                    parent.name = nodes[parent_idx].name;
                    if (!tree.insert(nodes[node_index], parent))
                        throw std::runtime_error("Failed to deserialize node tree");
                }

                // VecTree::insert inserts as the first child, so add children in reverse
                // order to preserve the serialized node order and indices.
                const auto& node_children = children[node_index];
                for (auto it = node_children.rbegin(); it != node_children.rend(); ++it)
                    self(*it, self);
            };

            for (const auto root_index : roots)
                insert_subtree(root_index, insert_subtree);
        }

        nlohmann::json serialize_bones(const std::vector<assets::Bone>& values)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            arr.reserve(values.size());
            for (const auto& bone : values)
            {
                nlohmann::json elem;
                elem["name"] = bone.name;
                elem["node_index"] = bone.node_index;
                elem["inverse_bind_tfm"] = serialize_mat4(bone.inverse_bind_tfm);
                arr.emplace_back(std::move(elem));
            }
            return j;
        }

        void deserialize_bones(const nlohmann::json& j, std::vector<assets::Bone>& values)
        {
            values.clear();
            if (!j.is_array())
                return;
            values.resize(j.size());
            for (size_t i = 0; i < j.size(); i++)
            {
                const auto& elem = j[i];
                auto& bone = values[i];
                bone.name = elem.value("name", "");
                bone.node_index = elem.value("node_index", assets::null_index);
                if (elem.contains("inverse_bind_tfm"))
                    bone.inverse_bind_tfm = deserialize_mat4(elem["inverse_bind_tfm"]);
            }
        }

        nlohmann::json serialize_anim_tracks(const std::vector<assets::AnimTrack>& values)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            arr.reserve(values.size());
            for (const auto& track : values)
            {
                nlohmann::json elem;
                elem["is_used"] = track.is_used;
                elem["pos_keys"] = serialize_vec3_array(track.pos_keys);
                elem["scale_keys"] = serialize_vec3_array(track.scale_keys);
                elem["rot_keys"] = serialize_quat_array(track.rot_keys);
                arr.emplace_back(std::move(elem));
            }
            return j;
        }

        void deserialize_anim_tracks(const nlohmann::json& j, std::vector<assets::AnimTrack>& values)
        {
            values.clear();
            if (!j.is_array())
                return;
            values.resize(j.size());
            for (size_t i = 0; i < j.size(); i++)
            {
                const auto& elem = j[i];
                auto& track = values[i];
                track.is_used = elem.value("is_used", false);
                if (elem.contains("pos_keys"))
                    deserialize_vec3_array(elem["pos_keys"], track.pos_keys);
                if (elem.contains("scale_keys"))
                    deserialize_vec3_array(elem["scale_keys"], track.scale_keys);
                if (elem.contains("rot_keys"))
                    deserialize_quat_array(elem["rot_keys"], track.rot_keys);
            }
        }

        nlohmann::json serialize_animations(const std::vector<assets::AnimClip>& values)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            arr.reserve(values.size());
            for (const auto& clip : values)
            {
                nlohmann::json elem;
                elem["name"] = clip.name;
                elem["duration_ticks"] = clip.duration_ticks;
                elem["ticks_per_second"] = clip.ticks_per_second;
                elem["node_animations"] = serialize_anim_tracks(clip.node_animations);
                arr.emplace_back(std::move(elem));
            }
            return j;
        }

        void deserialize_animations(const nlohmann::json& j, std::vector<assets::AnimClip>& values)
        {
            values.clear();
            if (!j.is_array())
                return;
            values.resize(j.size());
            for (size_t i = 0; i < j.size(); i++)
            {
                const auto& elem = j[i];
                auto& clip = values[i];
                clip.name = elem.value("name", "");
                clip.duration_ticks = elem.value("duration_ticks", 0.0f);
                clip.ticks_per_second = elem.value("ticks_per_second", 25.0f);
                if (elem.contains("node_animations"))
                    deserialize_anim_tracks(elem["node_animations"], clip.node_animations);
            }
        }
    }

    void serialize_ModelDataAsset(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<assets::ModelDataAsset>();
        assert(ptr && "serialize_ModelDataAsset: bad meta_any");
        const auto& model = *ptr;

        j = nlohmann::json::object();
        j["positions"] = serialize_vec3_array(model.positions);
        j["normals"] = serialize_vec3_array(model.normals);
        j["tangents"] = serialize_vec3_array(model.tangents);
        j["binormals"] = serialize_vec3_array(model.binormals);
        j["texcoords"] = serialize_vec2_array(model.texcoords);
        j["skin"] = serialize_skin_array(model.skin);
        j["indices"] = model.indices;
        j["submeshes"] = serialize_submeshes(model.submeshes);
        j["nodetree"] = serialize_nodetree(model.nodetree);
        j["bones"] = serialize_bones(model.bones);
        j["animations"] = serialize_animations(model.animations);
    }

    void deserialize_ModelDataAsset(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<assets::ModelDataAsset>();
        assert(ptr && "deserialize_ModelDataAsset: bad meta_any");
        auto& model = *ptr;

        model.positions.clear();
        model.normals.clear();
        model.tangents.clear();
        model.binormals.clear();
        model.texcoords.clear();
        model.skin.clear();
        model.indices.clear();
        model.submeshes.clear();
        model.nodetree.clear();
        model.bones.clear();
        model.animations.clear();

        if (j.contains("positions"))
            deserialize_vec3_array(j["positions"], model.positions);
        if (j.contains("normals"))
            deserialize_vec3_array(j["normals"], model.normals);
        if (j.contains("tangents"))
            deserialize_vec3_array(j["tangents"], model.tangents);
        if (j.contains("binormals"))
            deserialize_vec3_array(j["binormals"], model.binormals);
        if (j.contains("texcoords"))
            deserialize_vec2_array(j["texcoords"], model.texcoords);
        if (j.contains("skin"))
            deserialize_skin_array(j["skin"], model.skin);
        if (j.contains("indices"))
            model.indices = j["indices"].get<std::vector<assets::u32>>();
        if (j.contains("submeshes"))
            deserialize_submeshes(j["submeshes"], model.submeshes);
        if (j.contains("nodetree"))
            deserialize_nodetree(j["nodetree"], model.nodetree);
        if (j.contains("bones"))
            deserialize_bones(j["bones"], model.bones);
        if (j.contains("animations"))
            deserialize_animations(j["animations"], model.animations);

        if (model.skin.empty() && !model.positions.empty())
            model.skin.resize(model.positions.size());
    }

    void register_modeldataasset_serialization()
    {
        entt::meta_factory<assets::ModelDataAsset>{}
            .func<&serialize_ModelDataAsset>(eeng::literals::serialize_hs)
            .func<&deserialize_ModelDataAsset>(eeng::literals::deserialize_hs);
    }
}
