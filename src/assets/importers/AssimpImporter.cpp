// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "AssimpImporter.hpp"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "AssetMetaData.hpp"
#include "ResourceManager.hpp"
#include "parseutil.h"
#include "stb_image_write.h"
#include "meta/MetaAux.h"

namespace eeng::assets
{
    namespace
    {
        constexpr int no_texture = -1;

        /// @brief Convert Assimp vector to glm::vec3.
        inline glm::vec3 aivec_to_glmvec(const aiVector3D& vec)
        {
            return glm::vec3(vec.x, vec.y, vec.z);
        }

        /// @brief Convert Assimp quaternion to glm::quat.
        inline glm::quat aiquat_to_glmquat(const aiQuaternion& aiq)
        {
            return glm::quat(aiq.w, aiq.x, aiq.y, aiq.z);
        }

        /// @brief Convert Assimp matrix to glm::mat4 (column-major).
        inline glm::mat4 aimat_to_glmmat(const aiMatrix4x4& aim)
        {
            glm::mat4 glmm;
            glmm[0][0] = aim.a1;
            glmm[1][0] = aim.a2;
            glmm[2][0] = aim.a3;
            glmm[3][0] = aim.a4;
            glmm[0][1] = aim.b1;
            glmm[1][1] = aim.b2;
            glmm[2][1] = aim.b3;
            glmm[3][1] = aim.b4;
            glmm[0][2] = aim.c1;
            glmm[1][2] = aim.c2;
            glmm[2][2] = aim.c3;
            glmm[3][2] = aim.c4;
            glmm[0][3] = aim.d1;
            glmm[1][3] = aim.d2;
            glmm[2][3] = aim.d3;
            glmm[3][3] = aim.d4;
            return glmm;
        }

        /// @brief Insert a bone weight into the 4-slot skin data, keeping max weights.
        inline void add_weight(SkinData& data, u32 bone_index, float bone_weight)
        {
            float min_weight = 1.0f;
            u32 min_index = 0;
            for (u32 i = 0; i < bones_per_vertex; i++)
            {
                if (data.bone_weights[i] < min_weight)
                {
                    min_weight = data.bone_weights[i];
                    min_index = i;
                }
            }
            if (bone_weight > min_weight)
            {
                data.bone_weights[min_index] = bone_weight;
                data.bone_indices[min_index] = bone_index;
            }
        }

        /// @brief Ensure a filesystem path ends in a separator (for string concatenation).
        std::string ensure_trailing_separator(std::string path)
        {
            if (!path.empty())
            {
                char last = path.back();
                if (last != '/' && last != '\\')
                {
                    path.push_back(std::filesystem::path::preferred_separator);
                }
            }
            return path;
        }

        /// @brief Resolve a material texture, creating a TextureAsset entry if needed.
        /// @details Embedded textures are referenced by "*<index>" and exported on demand.
        ///          External textures are resolved relative to the model path, with a
        ///          fallback to filename-only if the authored path is missing.
        int load_texture(
            const aiMaterial* material,
            aiTextureType texture_type,
            const std::string& model_dir,
            const aiScene* scene,
            const std::filesystem::path& textures_dir,
            std::vector<TextureAsset>& textures,
            std::unordered_map<std::string, size_t>& texture_index_map,
            std::vector<int>& embedded_asset_indices)
        {
            unsigned nbr_textures = material->GetTextureCount(texture_type);
            if (!nbr_textures)
                return no_texture;
            if (nbr_textures > 1)
            {
                throw std::runtime_error("Multiple textures of type " +
                    std::to_string(texture_type) +
                    ", aborting. Nbr = " + std::to_string(nbr_textures));
            }

            aiString ai_texpath;
            aiTextureMapping ai_texmap;
            unsigned int ai_uvindex;
            float ai_blend;
            aiTextureOp ai_texop;
            aiTextureMapMode ai_texmapmode;
            if (material->GetTexture(texture_type,
                0,
                &ai_texpath,
                &ai_texmap,
                &ai_uvindex,
                &ai_blend,
                &ai_texop,
                &ai_texmapmode) != AI_SUCCESS)
            {
                return no_texture;
            }

            std::string texture_rel_path{ ai_texpath.C_Str() };
            if (texture_rel_path.empty())
                return no_texture;
            if (texture_rel_path.front() == '/')
                texture_rel_path.erase(0, 1);

            int embedded_texture_index = -1;
            // Embedded textures are referenced by "*<index>" per Assimp convention.
            if (std::sscanf(texture_rel_path.c_str(), "*%d", &embedded_texture_index) == 1)
            {
                if (!scene || embedded_texture_index < 0 || static_cast<size_t>(embedded_texture_index) >= scene->mNumTextures)
                    throw std::runtime_error("Embedded texture index out of range: " + std::to_string(embedded_texture_index));
                if (embedded_asset_indices[embedded_texture_index] >= 0)
                    return embedded_asset_indices[embedded_texture_index];

                aiTexture* aitexture = scene->mTextures[embedded_texture_index];
                std::string texture_rawfilename = aitexture->mFilename.C_Str();
                bool is_compressed = aitexture->mHeight == 0;

                // Export embedded textures into the model's texture folder on demand.
                std::string texture_filename = "embedded_" + std::to_string(embedded_texture_index) + ".";
                if (!is_compressed)
                    texture_filename += "png";
                else if (auto hint = std::string(aitexture->achFormatHint); !hint.empty() && hint[0])
                    texture_filename += hint;
                else if (auto ext = get_fileext(texture_rawfilename); !ext.empty())
                    texture_filename += ext;
                else
                    texture_filename += "bin";

                auto export_path = textures_dir / texture_filename;
                if (!export_path.empty() && !std::filesystem::exists(export_path))
                {
                    if (aitexture->mHeight == 0)
                    {
                        std::ofstream out_file(export_path, std::ios::out | std::ios::binary);
                        out_file.write(reinterpret_cast<const char*>(aitexture->pcData), static_cast<std::streamsize>(aitexture->mWidth));
                    }
                    else
                    {
                        stbi_write_png(export_path.string().c_str(),
                            static_cast<int>(aitexture->mWidth),
                            static_cast<int>(aitexture->mHeight),
                            4,
                            aitexture->pcData,
                            static_cast<int>(aitexture->mWidth * sizeof(aiTexel)));
                    }
                }

                TextureAsset texture{};
                texture.source_path = texture_filename;
                const int texture_index = static_cast<int>(textures.size());
                textures.push_back(texture);
                embedded_asset_indices[embedded_texture_index] = texture_index;
                return texture_index;
            }

            std::string texture_filename = get_filename(texture_rel_path);
            std::string texture_abs_path = model_dir + texture_rel_path;
            std::string source_path = texture_rel_path;
            if (!std::filesystem::exists(texture_abs_path))
            {
                // Assimp often stores absolute/authoring paths; fall back to filename.
                texture_abs_path = model_dir + texture_filename;
                source_path = texture_filename;
            }
            if (!std::filesystem::exists(texture_abs_path))
                throw std::runtime_error("Texture not found: " + texture_abs_path);

            auto tex_it = texture_index_map.find(source_path);
            if (tex_it == texture_index_map.end())
            {
                TextureAsset texture{};
                texture.source_path = source_path;
                size_t texture_index = textures.size();
                textures.push_back(texture);
                texture_index_map.emplace(source_path, texture_index);
                return static_cast<int>(texture_index);
            }
            return static_cast<int>(tex_it->second);
        }

        /// @brief Build a flat node tree from Assimp's hierarchy (bind pose).
        void load_node(aiNode* ainode, VecTree<SkeletonNode>& nodetree)
        {
            std::string node_name = std::string(ainode->mName.C_Str());
            const aiNode* parent_node = ainode->mParent;
            std::string parent_name = parent_node ? std::string(parent_node->mName.C_Str()) : "";

            SkeletonNode stnode{};
            stnode.name = node_name;
            stnode.local_bind_tfm = aimat_to_glmmat(ainode->mTransformation);

            if (parent_name.empty())
            {
                nodetree.insert_as_root(stnode);
            }
            else
            {
                SkeletonNode parent_node{};
                parent_node.name = parent_name;
                if (!nodetree.insert(stnode, parent_node))
                {
                    throw std::runtime_error("Node tree insertion failed, hierarchy corrupt");
                }
            }

            for (int i = 0; i < ainode->mNumChildren; i++)
            {
                load_node(ainode->mChildren[i], nodetree);
            }
        }
    }

    struct AssimpImporter::Impl
    {
        Assimp::Importer importer;
    };

    AssimpImporter::AssimpImporter()
        : impl_(std::make_unique<Impl>())
    {
    }

    AssimpImporter::~AssimpImporter() = default;

    AssimpImportResult AssimpImporter::import_model(
        const AssimpImportOptions& options,
        EngineContext& ctx)
    {
        AssimpImportResult result;
        if (options.source_file.empty())
        {
            result.error_message = "Assimp import failed: source_file is empty";
            return result;
        }

        try
        {
            AssimpParseResult parsed = parse_scene(options.source_file, options, ctx);
            result = build_assets(parsed, options, ctx);
        }
        catch (const std::exception& ex)
        {
            result.success = false;
            result.error_message = ex.what();
        }
        return result;
    }

    AssimpParseResult AssimpImporter::parse_scene(
        const std::filesystem::path& source_file,
        const AssimpImportOptions& options,
        EngineContext&)
    {
        AssimpParseResult parsed{};

        unsigned int assimp_flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices;
        if (options.flags != ImportFlags::None)
        {
            if ((static_cast<unsigned>(options.flags) & static_cast<unsigned>(ImportFlags::GenerateTangents)) != 0)
                assimp_flags |= aiProcess_CalcTangentSpace;
            if ((static_cast<unsigned>(options.flags) & static_cast<unsigned>(ImportFlags::FlipUVs)) != 0)
                assimp_flags |= aiProcess_FlipUVs;
            if ((static_cast<unsigned>(options.flags) & static_cast<unsigned>(ImportFlags::OptimizeMesh)) != 0)
                assimp_flags |= aiProcess_ImproveCacheLocality;
            if ((static_cast<unsigned>(options.flags) & static_cast<unsigned>(ImportFlags::GenerateNormals)) != 0)
                assimp_flags |= aiProcess_GenNormals;
            if ((static_cast<unsigned>(options.flags) & static_cast<unsigned>(ImportFlags::GenerateUVs)) != 0)
                assimp_flags |= aiProcess_GenUVCoords;
            if ((static_cast<unsigned>(options.flags) & static_cast<unsigned>(ImportFlags::SortByPType)) != 0)
                assimp_flags |= aiProcess_SortByPType;
            if ((static_cast<unsigned>(options.flags) & static_cast<unsigned>(ImportFlags::OptimizeGraph)) != 0)
                assimp_flags |= aiProcess_OptimizeGraph;
        }

        const aiScene* scene = impl_->importer.ReadFile(source_file.string(), assimp_flags);
        if (!scene || !scene->mRootNode)
        {
            throw std::runtime_error(impl_->importer.GetErrorString());
        }

        // Assimp scene sanity checks; fail early for unsupported inputs.
        if (!scene->HasMeshes())
            throw std::runtime_error("Scene has no meshes");
        if (!scene->HasMaterials())
            throw std::runtime_error("Scene has no materials");

        auto& model = parsed.model_data;
        const u32 scene_nbr_meshes = scene->mNumMeshes;

        std::string model_dir = ensure_trailing_separator(source_file.parent_path().string());
        const std::string model_folder_name = options.model_name.empty()
            ? source_file.stem().string()
            : options.model_name;
        const auto asset_path = options.assets_root / model_folder_name;
        const auto textures_dir = asset_path / "textures";
        std::filesystem::create_directories(textures_dir);

        std::unordered_map<std::string, size_t> texture_index_map;
        std::vector<int> embedded_asset_indices(scene->mNumTextures, -1);

        model.submeshes.resize(scene_nbr_meshes);

        u32 scene_nbr_vertices = 0;
        u32 scene_nbr_indices = 0;
        for (u32 i = 0; i < scene_nbr_meshes; i++)
        {
            const aiMesh* mesh = scene->mMeshes[i];
            const u32 mesh_nbr_vertices = mesh->mNumVertices;
            const u32 mesh_nbr_indices = mesh->mNumFaces * 3;
            const u32 mesh_nbr_bones = mesh->mNumBones;
            const u32 mesh_mtl_index = mesh->mMaterialIndex;

            SubMesh sm{};
            sm.base_index = scene_nbr_indices;
            sm.nbr_indices = mesh_nbr_indices;
            sm.base_vertex = scene_nbr_vertices;
            sm.nbr_vertices = mesh_nbr_vertices;
            sm.is_skinned = (mesh_nbr_bones > 0);
            if (mesh_mtl_index < scene->mNumMaterials)
            {
                sm.material = AssetRef<MaterialAsset>{ Guid(static_cast<Guid::underlying_type>(mesh_mtl_index + 1)) };
            }
            model.submeshes[i] = sm;

            scene_nbr_vertices += mesh_nbr_vertices;
            scene_nbr_indices += mesh_nbr_indices;
        }

        model.positions.reserve(scene_nbr_vertices);
        model.normals.reserve(scene_nbr_vertices);
        model.tangents.reserve(scene_nbr_vertices);
        model.binormals.reserve(scene_nbr_vertices);
        model.texcoords.reserve(scene_nbr_vertices);
        model.skin.resize(scene_nbr_vertices);
        model.indices.reserve(scene_nbr_indices);

        std::unordered_map<std::string, u32> bone_index_map;

        const aiVector3D v3zero(0.0f, 0.0f, 0.0f);
        for (u32 i = 0; i < scene_nbr_meshes; i++)
        {
            const aiMesh* mesh = scene->mMeshes[i];
            for (u32 j = 0; j < mesh->mNumVertices; j++)
            {
                const aiVector3D* pPos = &(mesh->mVertices[j]);
                const aiVector3D* pNormal = (mesh->HasNormals() ? &(mesh->mNormals[j]) : &v3zero);
                const aiVector3D* pTangent = (mesh->HasTangentsAndBitangents() ? &(mesh->mTangents[j]) : &v3zero);
                const aiVector3D* pBinormal = (mesh->HasTangentsAndBitangents() ? &(mesh->mBitangents[j]) : &v3zero);
                const aiVector3D* pTexCoord = (mesh->HasTextureCoords(0) ? &(mesh->mTextureCoords[0][j]) : &v3zero);

                model.positions.push_back({ pPos->x, pPos->y, pPos->z });
                model.normals.push_back({ pNormal->x, pNormal->y, pNormal->z });
                model.tangents.push_back({ pTangent->x, pTangent->y, pTangent->z });
                model.binormals.push_back({ pBinormal->x, pBinormal->y, pBinormal->z });
                model.texcoords.push_back({ pTexCoord->x, pTexCoord->y });
            }

            for (u32 b = 0; b < mesh->mNumBones; b++)
            {
                std::string bone_name(mesh->mBones[b]->mName.C_Str());
                u32 bone_index = 0;
                auto bone_it = bone_index_map.find(bone_name);
                if (bone_it == bone_index_map.end())
                {
                    bone_index = static_cast<u32>(model.bones.size());
                    bone_index_map[bone_name] = bone_index;
                    Bone bone{};
                    bone.inverse_bind_tfm = aimat_to_glmmat(mesh->mBones[b]->mOffsetMatrix);
                    bone.node_index = null_index;
                    bone.name = bone_name;
                    model.bones.push_back(bone);
                }
                else
                {
                    bone_index = bone_it->second;
                }

                for (u32 w = 0; w < mesh->mBones[b]->mNumWeights; w++)
                {
                    const u32 vertex_id = model.submeshes[i].base_vertex + mesh->mBones[b]->mWeights[w].mVertexId;
                    const float bone_weight = mesh->mBones[b]->mWeights[w].mWeight;
                    add_weight(model.skin[vertex_id], bone_index, bone_weight);
                }
            }

            for (u32 f = 0; f < mesh->mNumFaces; f++)
            {
                const aiFace& face = mesh->mFaces[f];
                model.indices.push_back(face.mIndices[0]);
                model.indices.push_back(face.mIndices[1]);
                model.indices.push_back(face.mIndices[2]);
            }
        }

        load_node(scene->mRootNode, model.nodetree);

        model.nodetree.traverse_depthfirst([&](SkeletonNode& node, size_t i, size_t)
            {
                aiNode* ainode = scene->mRootNode->FindNode(node.name.c_str());
                if (ainode)
                {
                    for (int j = 0; j < ainode->mNumMeshes; j++)
                    {
                        model.submeshes[ainode->mMeshes[j]].node_index = static_cast<i32>(i);
                    }
                    node.nbr_meshes = ainode->mNumMeshes;
                }

                auto boneit = bone_index_map.find(node.name);
                if (boneit != bone_index_map.end())
                {
                    model.bones[boneit->second].node_index = static_cast<i32>(i);
                    node.bone_index = static_cast<i32>(boneit->second);
                }
            });

        for (u32 i = 0; i < scene->mNumAnimations; i++)
        {
            aiAnimation* aianim = scene->mAnimations[i];
            AnimClip anim{};
            anim.name = std::string(aianim->mName.C_Str());
            anim.duration_ticks = static_cast<float>(aianim->mDuration);
            anim.ticks_per_second = static_cast<float>(aianim->mTicksPerSecond);
            anim.node_animations.resize(model.nodetree.size());

            for (u32 j = 0; j < aianim->mNumChannels; j++)
            {
                aiNodeAnim* ainode_anim = aianim->mChannels[j];
                AnimTrack node_anim{};
                node_anim.is_used = true;
                auto name = std::string(ainode_anim->mNodeName.C_Str());

                node_anim.pos_keys.reserve(ainode_anim->mNumPositionKeys);
                node_anim.scale_keys.reserve(ainode_anim->mNumScalingKeys);
                node_anim.rot_keys.reserve(ainode_anim->mNumRotationKeys);
                for (u32 k = 0; k < ainode_anim->mNumPositionKeys; k++)
                    node_anim.pos_keys.push_back(aivec_to_glmvec(ainode_anim->mPositionKeys[k].mValue));
                for (u32 k = 0; k < ainode_anim->mNumScalingKeys; k++)
                    node_anim.scale_keys.push_back(aivec_to_glmvec(ainode_anim->mScalingKeys[k].mValue));
                for (u32 k = 0; k < ainode_anim->mNumRotationKeys; k++)
                    node_anim.rot_keys.push_back(aiquat_to_glmquat(ainode_anim->mRotationKeys[k].mValue));

                SkeletonNode find_node{};
                find_node.name = name;
                auto index = model.nodetree.find_node_index(find_node);
                if (index != VecTree_NullIndex)
                    anim.node_animations[index] = std::move(node_anim);
            }

            model.animations.push_back(std::move(anim));
        }

        parsed.materials.resize(scene->mNumMaterials);
        for (u32 i = 0; i < scene->mNumMaterials; i++)
        {
            const aiMaterial* material = scene->mMaterials[i];
            MaterialAsset mtl{};

            aiColor3D aic;
            if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_AMBIENT, aic))
                mtl.Ka = glm::vec3{ aic.r, aic.g, aic.b };
            if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, aic))
                mtl.Kd = glm::vec3{ aic.r, aic.g, aic.b };
            if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_SPECULAR, aic))
                mtl.Ks = glm::vec3{ aic.r, aic.g, aic.b };
            material->Get(AI_MATKEY_SHININESS, mtl.shininess);

            const int diffuse = load_texture(material, aiTextureType_DIFFUSE, model_dir, scene, textures_dir, parsed.textures, texture_index_map, embedded_asset_indices);
            const int normal = load_texture(material, aiTextureType_NORMALS, model_dir, scene, textures_dir, parsed.textures, texture_index_map, embedded_asset_indices);
            const int specular = load_texture(material, aiTextureType_SPECULAR, model_dir, scene, textures_dir, parsed.textures, texture_index_map, embedded_asset_indices);
            const int opacity = load_texture(material, aiTextureType_OPACITY, model_dir, scene, textures_dir, parsed.textures, texture_index_map, embedded_asset_indices);

            // Assimp sometimes tags OBJ normal maps as HEIGHT; use that as a fallback.
            const int normal_fallback = (normal == no_texture)
                ? load_texture(material, aiTextureType_HEIGHT, model_dir, scene, textures_dir, parsed.textures, texture_index_map, embedded_asset_indices)
                : normal;

            if (diffuse != no_texture)
                mtl.textures[static_cast<size_t>(MaterialTextureSlot::Diffuse)] = AssetRef<TextureAsset>{
                Guid(static_cast<Guid::underlying_type>(diffuse + 1)) };
            if (normal_fallback != no_texture)
                mtl.textures[static_cast<size_t>(MaterialTextureSlot::Normal)] = AssetRef<TextureAsset>{
                Guid(static_cast<Guid::underlying_type>(normal_fallback + 1)) };
            if (specular != no_texture)
                mtl.textures[static_cast<size_t>(MaterialTextureSlot::Specular)] = AssetRef<TextureAsset>{
                Guid(static_cast<Guid::underlying_type>(specular + 1)) };
            if (opacity != no_texture)
                mtl.textures[static_cast<size_t>(MaterialTextureSlot::Opacity)] = AssetRef<TextureAsset>{
                Guid(static_cast<Guid::underlying_type>(opacity + 1)) };

            parsed.materials[i] = std::move(mtl);
            parsed.material_index_map[i] = i;
        }

        return parsed;
    }

    AssimpImportResult AssimpImporter::build_assets(
        const AssimpParseResult& parsed,
        const AssimpImportOptions& options,
        EngineContext& ctx)
    {
        AssimpImportResult result;

        auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);

        const std::string model_folder_name = options.model_name.empty()
            ? options.source_file.stem().string()
            : options.model_name;

        const auto asset_path = options.assets_root / model_folder_name;
        const auto model_path = asset_path / "model";
        const auto mtl_path = asset_path / "materials";
        const auto tex_path = asset_path / "textures";
        const auto gpumodel_path = asset_path / "gpu_model";
        const auto gpumtl_path = asset_path / "gpu_materials";
        const auto gputex_path = asset_path / "gpu_textures";

        std::filesystem::create_directories(model_path);
        std::filesystem::create_directories(mtl_path);
        std::filesystem::create_directories(tex_path);
        std::filesystem::create_directories(gpumodel_path);
        std::filesystem::create_directories(gpumtl_path);
        std::filesystem::create_directories(gputex_path);

        const Guid model_guid = Guid::generate();
        const Guid gpu_model_guid = Guid::generate();

        const size_t texture_count = parsed.textures.size();
        const size_t material_count = parsed.materials.size();

        std::vector<Guid> texture_guids(texture_count);
        std::vector<Guid> gpu_texture_guids(texture_count);
        std::unordered_map<Guid, Guid> texture_to_gpu;
        for (size_t i = 0; i < texture_count; i++)
        {
            texture_guids[i] = Guid::generate();
            gpu_texture_guids[i] = Guid::generate();
            texture_to_gpu.emplace(texture_guids[i], gpu_texture_guids[i]);
        }

        std::vector<Guid> material_guids(material_count);
        std::vector<Guid> gpu_material_guids(material_count);
        std::unordered_map<Guid, Guid> material_to_gpu;
        for (size_t i = 0; i < material_count; i++)
        {
            material_guids[i] = Guid::generate();
            gpu_material_guids[i] = Guid::generate();
            material_to_gpu.emplace(material_guids[i], gpu_material_guids[i]);
        }

        const auto resolve_texture_guid = [&](const AssetRef<TextureAsset>& ref) -> Guid
        {
            if (!ref.guid.valid())
                return Guid::invalid();
            const auto raw = ref.guid.raw();
            if (raw == 0)
                return Guid::invalid();
            const size_t index = static_cast<size_t>(raw - 1);
            if (index >= texture_guids.size())
                return Guid::invalid();
            return texture_guids[index];
        };

        const auto resolve_material_guid = [&](const AssetRef<MaterialAsset>& ref) -> Guid
        {
            if (!ref.guid.valid())
                return Guid::invalid();
            const auto raw = ref.guid.raw();
            if (raw == 0)
                return Guid::invalid();
            const size_t index = static_cast<size_t>(raw - 1);
            if (index >= material_guids.size())
                return Guid::invalid();
            return material_guids[index];
        };

        std::vector<TextureAsset> textures = parsed.textures;
        const auto source_dir = options.source_file.parent_path();
        for (size_t i = 0; i < textures.size(); i++)
        {
            auto rel_path = std::filesystem::path(textures[i].source_path);
            std::filesystem::path src_path;
            if (rel_path.is_absolute())
            {
                src_path = rel_path;
                rel_path = rel_path.filename();
            }
            else
            {
                src_path = source_dir / rel_path;
            }

            const auto dst_path = tex_path / rel_path;
            if (!rel_path.empty() && src_path != dst_path && std::filesystem::exists(src_path))
            {
                std::filesystem::create_directories(dst_path.parent_path());
                if (!std::filesystem::exists(dst_path))
                {
                    std::filesystem::copy_file(src_path, dst_path, std::filesystem::copy_options::overwrite_existing);
                }
            }

            textures[i].source_path = rel_path.string();
        }

        for (size_t i = 0; i < textures.size(); i++)
        {
            const auto tex_guid = texture_guids[i];
            const auto tex_file_base = tex_guid.to_string();
            const auto tex_file_path = tex_path / (tex_file_base + ".json");
            const auto tex_meta_file_path = tex_path / (tex_file_base + ".meta.json");

            resource_manager.import(
                textures[i],
                tex_file_path.string(),
                AssetMetaData{
                    tex_guid,
                    model_guid,
                    model_folder_name + "_Texture" + std::to_string(i),
                    meta::get_meta_type_id_string<TextureAsset>()
                },
                tex_meta_file_path.string());

            const auto gpu_tex_guid = gpu_texture_guids[i];
            const auto gpu_tex_file_base = gpu_tex_guid.to_string();
            const auto gpu_tex_file_path = gputex_path / (gpu_tex_file_base + ".json");
            const auto gpu_tex_meta_file_path = gputex_path / (gpu_tex_file_base + ".meta.json");

            GpuTextureAsset gpu_tex{};
            gpu_tex.texture_ref = AssetRef<TextureAsset>{ tex_guid };
            gpu_tex.state = GpuLoadState::Uninitialized;

            resource_manager.import(
                gpu_tex,
                gpu_tex_file_path.string(),
                AssetMetaData{
                    gpu_tex_guid,
                    tex_guid,
                    model_folder_name + "_GpuTexture" + std::to_string(i),
                    meta::get_meta_type_id_string<GpuTextureAsset>()
                },
                gpu_tex_meta_file_path.string());
        }

        std::vector<MaterialAsset> materials = parsed.materials;
        for (size_t i = 0; i < materials.size(); i++)
        {
            for (auto& tex_ref : materials[i].textures)
            {
                const Guid new_guid = resolve_texture_guid(tex_ref);
                tex_ref = AssetRef<TextureAsset>{ new_guid };
            }

            const auto mtl_guid = material_guids[i];
            const auto mtl_file_base = mtl_guid.to_string();
            const auto mtl_file_path = mtl_path / (mtl_file_base + ".json");
            const auto mtl_meta_file_path = mtl_path / (mtl_file_base + ".meta.json");

            resource_manager.import(
                materials[i],
                mtl_file_path.string(),
                AssetMetaData{
                    mtl_guid,
                    model_guid,
                    model_folder_name + "_Material" + std::to_string(i),
                    meta::get_meta_type_id_string<MaterialAsset>()
                },
                mtl_meta_file_path.string());

            const auto gpu_mtl_guid = gpu_material_guids[i];
            const auto gpu_mtl_file_base = gpu_mtl_guid.to_string();
            const auto gpu_mtl_file_path = gpumtl_path / (gpu_mtl_file_base + ".json");
            const auto gpu_mtl_meta_file_path = gpumtl_path / (gpu_mtl_file_base + ".meta.json");

            GpuMaterialAsset gpu_mtl{};
            gpu_mtl.material_ref = AssetRef<MaterialAsset>{ mtl_guid };
            gpu_mtl.Ka = materials[i].Ka;
            gpu_mtl.Kd = materials[i].Kd;
            gpu_mtl.Ks = materials[i].Ks;
            gpu_mtl.shininess = materials[i].shininess;
            for (size_t t = 0; t < gpu_mtl.textures.size(); t++)
            {
                const Guid tex_guid = materials[i].textures[t].guid;
                auto gpu_it = texture_to_gpu.find(tex_guid);
                if (gpu_it != texture_to_gpu.end())
                    gpu_mtl.textures[t] = AssetRef<GpuTextureAsset>{ gpu_it->second };
            }

            resource_manager.import(
                gpu_mtl,
                gpu_mtl_file_path.string(),
                AssetMetaData{
                    gpu_mtl_guid,
                    mtl_guid,
                    model_folder_name + "_GpuMaterial" + std::to_string(i),
                    meta::get_meta_type_id_string<GpuMaterialAsset>()
                },
                gpu_mtl_meta_file_path.string());
        }

        ModelDataAsset model = parsed.model_data;
        for (auto& sm : model.submeshes)
        {
            const Guid new_guid = resolve_material_guid(sm.material);
            sm.material = AssetRef<MaterialAsset>{ new_guid };
        }

        const auto model_file_base = model_guid.to_string();
        const auto model_file_path = model_path / (model_file_base + ".json");
        const auto model_meta_file_path = model_path / (model_file_base + ".meta.json");

        resource_manager.import(
            model,
            model_file_path.string(),
            AssetMetaData{
                model_guid,
                Guid::invalid(),
                model_folder_name,
                meta::get_meta_type_id_string<ModelDataAsset>()
            },
            model_meta_file_path.string());

        GpuModelAsset gpu_model{};
        gpu_model.model_ref = AssetRef<ModelDataAsset>{ model_guid };
        gpu_model.state = GpuLoadState::Uninitialized;
        gpu_model.submeshes.reserve(model.submeshes.size());
        for (const auto& sm : model.submeshes)
        {
            GpuSubMesh gsm{};
            gsm.index_offset = sm.base_index;
            gsm.index_count = sm.nbr_indices;
            gsm.base_vertex = sm.base_vertex;
            auto gpu_it = material_to_gpu.find(sm.material.guid);
            if (gpu_it != material_to_gpu.end())
                gsm.material = AssetRef<GpuMaterialAsset>{ gpu_it->second };
            gpu_model.submeshes.push_back(gsm);
        }

        const auto gpu_model_file_base = gpu_model_guid.to_string();
        const auto gpu_model_file_path = gpumodel_path / (gpu_model_file_base + ".json");
        const auto gpu_model_meta_file_path = gpumodel_path / (gpu_model_file_base + ".meta.json");

        resource_manager.import(
            gpu_model,
            gpu_model_file_path.string(),
            AssetMetaData{
                gpu_model_guid,
                model_guid,
                model_folder_name + "_GpuModel",
                meta::get_meta_type_id_string<GpuModelAsset>()
            },
            gpu_model_meta_file_path.string());

        result.success = true;
        result.gpu_model = AssetRef<GpuModelAsset>{ gpu_model_guid };
        result.model_guid = model_guid;
        return result;
    }
} // namespace eeng::assets
