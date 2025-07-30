//
//  SceneGraph.cpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-05-18.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#include <stdio.h>
#include "SceneGraph.hpp"
//#include "transform.hpp"
#include "CoreComponents.hpp"

bool SceneGraph::insert_node(
    const Entity& entity,
    const Entity& parent_entity
)
{
    if (parent_entity.is_null())
    {
        tree.insert_as_root(entity);
        return true;
    }
    else
        return tree.insert(entity, parent_entity);
}

bool SceneGraph::erase_node(const Entity& entity)
{
    // assert(tree.is_leaf(entity));
    if (!tree.is_leaf(entity))
        std::cout << "WARNING: erase_node: non-leaf node erased " << entity.to_integral() << std::endl;

    return tree.erase_branch(entity);
}

bool SceneGraph::is_root(const Entity& entity)
{
    return tree.is_root(entity);
}

bool SceneGraph::is_leaf(const Entity& entity)
{
    return tree.is_leaf(entity);
}

unsigned SceneGraph::get_nbr_children(const Entity& entity)
{
    return tree.get_nbr_children(entity);
}

Entity SceneGraph::get_parent(const Entity& entity)
{
    assert(!is_root(entity));
    return tree.get_parent(entity);
}

bool SceneGraph::is_descendant_of(const Entity& entity, const Entity& parent_entity)
{
    return tree.is_descendant_of(entity, parent_entity);
}

void SceneGraph::reparent(const Entity& entity, const Entity& parent_entity)
{
    // Do exception here?
    // Which checks?
    //      OnReparent does some checks
    //      Command?
    //      tree will throw if operation is not valid

    assert(tree.contains(entity));

    if (parent_entity.is_null())
    {
        unparent(entity);
        return;
    }

    assert(tree.contains(parent_entity));
    tree.reparent(entity, parent_entity);
}

void SceneGraph::unparent(const Entity& entity)
{
    tree.unparent(entity);
}

size_t SceneGraph::size()
{
    return tree.size();
}

// void SceneGraph::reset()
// {
//     // for (auto& node : tree.nodes) node.transform_hnd->global_tfm = m4f_1;
// }

void SceneGraph::traverse(std::shared_ptr<entt::registry>& registry)
{
    // std::cout << "traverse:" << std::endl;
    tree.traverse_depthfirst([&](Entity* entity_ptr, Entity* entity_parent_ptr, size_t, size_t) {
    //tree.traverse_progressive([&](Entity* entity_ptr, Entity* entity_parent_ptr) {
        // + Transform = parent tfm + tfm (+ maybe their aggregate)

        // std::cout << "node " << Editor::get_entity_name(registry, entity, entt::meta_type{});
        // std::cout << ", parent " << Editor::get_entity_name(registry, entity_parent, entt::meta_type{});
        // std::cout << std::endl;

        if (!registry->template all_of<Transform>(*entity_ptr)) return;
        assert(registry->valid(*entity_ptr));
        auto& tfm_node = registry->template get<Transform>(*entity_ptr);

        if (entity_parent_ptr && registry->template all_of<Transform>(*entity_parent_ptr))
        {
            assert(registry->valid(*entity_parent_ptr)); // fix
            auto& tfm_parent = registry->template get<Transform>(*entity_parent_ptr);
            tfm_node.x_parent = tfm_parent.x_global;
            tfm_node.y_parent = tfm_parent.y_global;
            tfm_node.angle_parent = tfm_parent.angle_global;
        }
        else
        {
            tfm_node.x_parent = 0.0f;
            tfm_node.y_parent = 0.0f;
            tfm_node.angle_parent = 0.0f;
        }

        // // opt sin/cos
        // tfm_node.x_global = tfm_node.x;
        // tfm_node.y_global = tfm_node.y;
        // tfm_node.angle_global = tfm_node.angle;
        tfm_node.x_global = tfm_node.x * cos(tfm_node.angle_parent) - tfm_node.y * sin(tfm_node.angle_parent) + tfm_node.x_parent;
        tfm_node.y_global = tfm_node.x * sin(tfm_node.angle_parent) + tfm_node.y * cos(tfm_node.angle_parent) + tfm_node.y_parent;
        tfm_node.angle_global = tfm_node.angle + tfm_node.angle_parent;
        });
}

SceneGraph::BranchQueue SceneGraph::get_branch_topdown(const Entity& entity)
{
    BranchQueue stack;

    tree.traverse_breadthfirst(entity, [&](const Entity& entity, size_t index) {
        stack.push_back(entity);
        });

    return stack;
}

SceneGraph::BranchQueue SceneGraph::get_branch_bottomup(const Entity& entity)
{
    BranchQueue stack;

    tree.traverse_breadthfirst(entity, [&](const Entity& entity, size_t index) {
        stack.push_front(entity);
        });

    return stack;
}

void SceneGraph::dump_to_cout(
    const std::shared_ptr<const entt::registry>& registry,
    const entt::meta_type meta_type_with_name) const
{
    //std::cout << "Scene graph nodes:" << std::endl;
    tree.traverse_depthfirst([&](const Entity& entity, size_t index, size_t level)
        {
            //auto entity = node.m_payload;
            auto entity_name = Editor::get_entity_name(registry, entity, meta_type_with_name);

            auto [nbr_children, branch_stride, parent_ofs] = tree.get_node_info(entity);

            for (int i = 0; i < level; i++) std::cout << "\t";
            std::cout << " [node " << index << "]";
            std::cout << " " << entity_name //node.m_name
                << " (children " << nbr_children
                << ", stride " << branch_stride
                << ", parent ofs " << parent_ofs << ")\n";
        });
}