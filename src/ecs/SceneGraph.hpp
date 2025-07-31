//
//  SceneGraph.hpp
//  assimp1
//
//  Created by Carl Johan Gribel on 2021-05-18.
//  Copyright © 2021 Carl Johan Gribel. All rights reserved.
//

#ifndef SceneGraph_hpp
#define SceneGraph_hpp

#include <stdio.h>
#include <sstream>
// #include <entt/entt.hpp>
#include "Entity.hpp"
#include "VecTree.h"
#include "MetaInspect.hpp"

namespace eeng::ecs
{
    class SceneGraph
    {
        // public:

        // private:
            // struct SceneGraphNode
            // {
            //     entt::entity entity;
            //     std::string name;

            //     bool operator==(const SceneGraphNode& node) const
            //     {
            //         return entity == node.entity;
            //     }

            //     SceneGraphNode(entt::entity entity, const std::string& name)
            //         : entity(entity), name(name) {}
            // };

    public: // TODO: don't expose directly
        VecTree<Entity> tree;
        using BranchQueue = std::deque<Entity>;

        SceneGraph() = default;

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
