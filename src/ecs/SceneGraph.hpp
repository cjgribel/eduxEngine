// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SceneGraph_hpp
#define SceneGraph_hpp

#include <stdio.h>
#include <sstream>
#include "Entity.hpp"

template <class PayloadType>
    requires requires(PayloadType a, PayloadType b) { { a == b } -> std::convertible_to<bool>; }
class VecTree;

namespace eeng::ecs
{
    class SceneGraph
    {
        std::unique_ptr<VecTree<Entity>> tree;

    public: // TODO: don't expose directly
        using BranchQueue = std::deque<Entity>;

        SceneGraph();
        ~SceneGraph();

        const VecTree<Entity>& get_tree() const;
        VecTree<Entity>& get_tree();

        bool insert_node(
            const Entity&,
            const Entity& parent_entity = Entity{}
        );

        bool erase_node(const Entity& entity);

        bool is_root(const Entity& entity);

        bool is_leaf(const Entity& entity);

        unsigned get_nbr_children(const Entity& entity);

        Entity get_parent(const Entity& entity);

        bool is_descendant_of(const Entity& entity, const Entity& parent_entity);

        void reparent(const Entity& entity, const Entity& parent_entity);

        void unparent(const Entity&);

        size_t size();

        // void reset();

        void traverse(std::shared_ptr<entt::registry>& registry);

        BranchQueue get_branch_topdown(const Entity& entity);

        BranchQueue get_branch_bottomup(const Entity& entity);

        // template<class F> requires std::invocable<F, PayloadType&, size_t, size_t>
        // void traverse_depthfirst(
        //     F&& func)
        //void visit_depthfirst();

        void dump_to_cout(
            const std::shared_ptr<const entt::registry>& registry,
            const entt::meta_type meta_type_with_name) const;
    };
} // namespace eeng::ecs

#endif /* SceneGraph_hpp */
