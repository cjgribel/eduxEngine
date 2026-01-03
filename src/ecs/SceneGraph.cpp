// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "SceneGraph.hpp"
#include "VecTree.h"
#include "MetaInspect.hpp"
#include "ecs/TransformComponent.hpp"

#include <cstdint>
#include <entt/entt.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stdio.h>

using Transform = eeng::ecs::TransformComponent;

namespace eeng::ecs
{
    SceneGraph::SceneGraph()
        : tree(std::make_unique<VecTree<Entity>>())
    {
    }

    SceneGraph::~SceneGraph() = default;

    const VecTree<Entity>& SceneGraph::get_tree() const { return *tree;
     }
    VecTree<Entity>& SceneGraph::get_tree() { return *tree; }

    bool SceneGraph::insert_node(
        const Entity& entity,
        const Entity& parent_entity
    )
    {
        if (!parent_entity.has_id())
        {
            tree->insert_as_root(entity);
            return true;
        }
        else
            return tree->insert(entity, parent_entity);
    }

    bool SceneGraph::erase_node(const Entity& entity)
    {
        // assert(tree.is_leaf(entity));
        if (!tree->is_leaf(entity))
            std::cout << "WARNING: erase_node: non-leaf node erased " << entity.to_integral() << std::endl;

        return tree->erase_branch(entity);
    }

    bool SceneGraph::contains(const Entity& entity)
    {
        return tree->contains(entity);
    }

    bool SceneGraph::is_root(const Entity& entity)
    {
        return tree->is_root(entity);
    }

    bool SceneGraph::is_leaf(const Entity& entity)
    {
        return tree->is_leaf(entity);
    }

    unsigned SceneGraph::get_nbr_children(const Entity& entity)
    {
        return tree->get_nbr_children(entity);
    }

    Entity SceneGraph::get_parent(const Entity& entity)
    {
        assert(!is_root(entity));
        return tree->get_parent(entity);
    }

    bool SceneGraph::is_descendant_of(const Entity& entity, const Entity& parent_entity)
    {
        return tree->is_descendant_of(entity, parent_entity);
    }

    void SceneGraph::reparent(const Entity& entity, const Entity& parent_entity)
    {
        // Do exception here?
        // Which checks?
        //      OnReparent does some checks
        //      Command?
        //      tree will throw if operation is not valid

        assert(tree->contains(entity));

        if (!parent_entity.has_id())
        {
            unparent(entity);
            return;
        }

        assert(tree->contains(parent_entity));
        tree->reparent(entity, parent_entity);
    }

    void SceneGraph::unparent(const Entity& entity)
    {
        tree->unparent(entity);
    }

    size_t SceneGraph::size()
    {
        return tree->size();
    }

    // void SceneGraph::reset()
    // {
    //     // for (auto& node : tree.nodes) node.transform_hnd->global_tfm = m4f_1;
    // }

    void SceneGraph::traverse(std::shared_ptr<entt::registry>& registry)
    {
        // std::cout << "traverse:" << std::endl;
        tree->traverse_depthfirst([&](Entity* entity_ptr, Entity* entity_parent_ptr, size_t, size_t) {
            //tree.traverse_progressive([&](Entity* entity_ptr, Entity* entity_parent_ptr) {
                // + Transform = parent tfm + tfm (+ maybe their aggregate)

                // std::cout << "node " << Editor::get_entity_name(registry, entity, entt::meta_type{});
                // std::cout << ", parent " << Editor::get_entity_name(registry, entity_parent, entt::meta_type{});
                // std::cout << std::endl;

            if (!registry->template all_of<Transform>(*entity_ptr)) return;
            assert(registry->valid(*entity_ptr));
            auto& tfm_node = registry->template get<Transform>(*entity_ptr);

            Transform* parent_tfm = nullptr;
            std::uint32_t parent_version = 0;
            if (entity_parent_ptr && registry->template all_of<Transform>(*entity_parent_ptr))
            {
                assert(registry->valid(*entity_parent_ptr)); // fix
                parent_tfm = &registry->template get<Transform>(*entity_parent_ptr);
                parent_version = parent_tfm->world_version;
            }

            const bool local_changed = (tfm_node.local_matrix_version != tfm_node.local_version);
            if (local_changed)
            {
                tfm_node.rotation = glm::normalize(tfm_node.rotation);
                tfm_node.local_matrix =
                    glm::translate(glm::mat4(1.0f), tfm_node.position) *
                    glm::mat4_cast(tfm_node.rotation) *
                    glm::scale(glm::mat4(1.0f), tfm_node.scale);
                tfm_node.local_matrix_version = tfm_node.local_version;
            }

            if (local_changed || tfm_node.parent_world_version != parent_version)
            {
                if (parent_tfm)
                {
                    tfm_node.world_matrix = parent_tfm->world_matrix * tfm_node.local_matrix;
                    tfm_node.world_rotation = glm::normalize(parent_tfm->world_rotation * tfm_node.rotation);
                }
                else
                {
                    tfm_node.world_matrix = tfm_node.local_matrix;
                    tfm_node.world_rotation = glm::normalize(tfm_node.rotation);
                }

                tfm_node.world_rotation_matrix = glm::mat3_cast(tfm_node.world_rotation);
                tfm_node.parent_world_version = parent_version;
                ++tfm_node.world_version;
                if (tfm_node.world_version == 0)
                    ++tfm_node.world_version;
            }
            });
    }

    SceneGraph::BranchQueue SceneGraph::get_branch_topdown(const Entity& entity)
    {
        BranchQueue stack;

        tree->traverse_breadthfirst(entity, [&](const Entity& entity, size_t index) {
            stack.push_back(entity);
            });

        return stack;
    }

    SceneGraph::BranchQueue SceneGraph::get_branch_bottomup(const Entity& entity)
    {
        BranchQueue stack;

        tree->traverse_breadthfirst(entity, [&](const Entity& entity, size_t index) {
            stack.push_front(entity);
            });

        return stack;
    }

    void SceneGraph::dump_to_cout(
        const std::shared_ptr<const entt::registry>& registry,
        const entt::meta_type meta_type_with_name) const
    {
        //std::cout << "Scene graph nodes:" << std::endl;
        tree->traverse_depthfirst([&](const Entity& entity, size_t index, size_t level)
            {
                //auto entity = node.m_payload;
                auto entity_name = meta::get_entity_name(registry, entity, meta_type_with_name);

                auto [nbr_children, branch_stride, parent_ofs] = tree->get_node_info(entity);

                for (int i = 0; i < level; i++) std::cout << "\t";
                std::cout << " [node " << index << "]";
                std::cout << " " << entity_name //node.m_name
                    << " (children " << nbr_children
                    << ", stride " << branch_stride
                    << ", parent ofs " << parent_ofs << ")\n";
            });
    }
} // namespace eeng::ecs
