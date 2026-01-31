//
//  hquad_tree.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2016-06-18.
//
//

#ifndef hquad_tree_h
#define hquad_tree_h

#include <vector>
#include "vec.h"
#include "AABB.h"
#include "collider.h"
#include "contact_constraint.h"
#include "mesh.h"
#include "sat_collision.h"

#define NBRCHILDREN 4
#define LEAFCAPACITY 4

#define MAX(x,y) (x)>(y)?(x):(y)

#define max(a,b) \
({ __typeof__ (a) _a = (a); \
__typeof__ (b) _b = (b); \
_a > _b ? _a : _b; })

constexpr int ARRSIZE = NBRCHILDREN > LEAFCAPACITY ? NBRCHILDREN : LEAFCAPACITY; // std::fmaxf(NBRCHILDREN, LEAFCAPACITY);

struct treeitem_t
{
    AABB_t AABB;
    bool is_leaf;
    int index_start, index_c; // children (node) or colliders (leaf)
};

//struct node_t : public treeitem_t
//{
//    size_t children[4];
//};
//
//struct leaf_t : public treeitem_t
//{
//    int t0, tc;
//};

poly_collider_t polycollider_from_triangle()
{
    return poly_collider_t();
}

class hquadtree_t
{
private:
    
    std::vector<treeitem_t> tree_nodes;
//    std::vector<treeitem_t*> tree;
    
    std::vector<poly_collider_t> colliders;
    std::vector<char> collider_flags;   // reset with std::fill(v.begin(), v.end(), 0);
    
    std::vector<unsigned> tree_leafs;
    
    size_t root; // index to top node
    
    inline size_t add_node(treeitem_t node)
    {
        tree_nodes.push_back(node); return tree_nodes.size()-1;
    }
    
    inline size_t add_leaf(unsigned index)
    {
        tree_leafs.push_back(index); return tree_leafs.size()-1;
    }
    
    inline void emplace_child_nodes(unsigned node, std::vector<treeitem_t>& children)
    {
        tree_nodes[node].index_start = (int)tree_nodes.size();
        tree_nodes[node].index_c = (int)children.size();
        tree_nodes.insert(tree_nodes.end(), children.begin(), children.end());
        tree_nodes[node].is_leaf = false;
    }
    
    inline void emplace_child_leafs(unsigned node, std::vector<unsigned>& leafs)
    {
        tree_nodes[node].index_start = (int)tree_leafs.size();
        tree_nodes[node].index_c = (int)leafs.size();
        tree_leafs.insert(tree_leafs.end(), leafs.begin(), leafs.end());
        tree_nodes[node].is_leaf = true;
    }
    
//    inline size_t add_nodes(treeitem_t* nodes_, int nbr)
//    {
//        nodes.insert(nodes.end(), nodes_, nodes_+nbr);
//        return nodes.size()-nbr;
//    }
    
    void filter_leafs(const std::vector<unsigned>& leaf_pool,
                             const AABB_t& AABB,
                             std::vector<unsigned>& leafs)
    {
        for(unsigned leaf : leaf_pool)
        {
            if ( AABB.intersect(this->colliders[leaf].AABB) )
                leafs.push_back(leaf);
        }
    }

//    inline size_t add_leaf(treeitem_t leaf) { leafs.push_back(node); return nodes.size()-1; }
    
public:
    
    hquadtree_t(mesh_t* mesh)
    {
        // MESH -> COLLIDERS
        // VERTEX SHADE?
        
        // TRIANGLES -> COLLIDERS -> colliders
        
        std::vector<unsigned> leaf_pool; // <- INDICES TO ALL COLLIDERS
        /*create simultaneuouslt */ AABB_t rootAABB; // <- ALL POLYS
        root = add_node( {rootAABB, false, -1,-1} );
        
        build_level(root, leaf_pool);
    }
    
private:
    
    //
    // expand a node with node or leaf children
    //
    void build_level(size_t node, std::vector<unsigned>& leaf_pool)
    {
        if (!leaf_pool.size())
        {
            return;
        }
        if (leaf_pool.size() <= LEAFCAPACITY)
        {
            emplace_child_leafs(node, leaf_pool);

        }
        else
        {
            // Split node's AABB
            AABB_t childAABB[4]; // 8 for octree
            tree_nodes[node].AABB.split4_xz(childAABB);
            
            // Create children
            std::vector<treeitem_t> cnodes; //[NBRCHILDREN];
            for (int i=0; i<NBRCHILDREN; i++)
                cnodes.push_back( { childAABB[i], false, -1, -1 } );
            
            // Add children to tree and set their index on current node
            emplace_child_nodes(node, cnodes);

            // Add colliders to children and build recursively
            for (int i = 0; i < NBRCHILDREN; i++)
            {
                int cnode = tree_nodes[node].index_start + i;
                
                std::vector<unsigned> cleaf_pool;
                filter_leafs(leaf_pool, tree_nodes[cnode].AABB, cleaf_pool);
                
                if (cleaf_pool.size())
                    build_level(cnode, cleaf_pool);
            }
        }
    }

public:
    
    void intersect(body_t* body, collider_t* collider, contact_manifold_t& cm)
    {
        if ( collider->AABB.intersect( tree_nodes[root].AABB ) )
            intersect_level(root, body, collider, cm);
    }
    
private:
    
    void intersect_level(unsigned node, body_t* body, collider_t* collider, contact_manifold_t& cm)
    {
        if ( !tree_nodes[node].index_c ) return;
        
        for (int i=0; i<tree_nodes[node].index_c; i++)
        {
            int child = tree_nodes[node].index_start + i;
            // No child
            if (child < 0) continue;
            // Child not intersecting collider
            if ( ! collider->AABB.intersect( tree_nodes[child].AABB ) ) continue;
            
            if ( tree_nodes[node].is_leaf )
            {
                // Leaf: collider leaf with collider
                poly_collider_t* leaf_collider = &colliders[ tree_leafs[child] ];
                collide_geoms(leaf_collider, collider, /*TODO*/nullptr, body, cm);
            }
            else
            {
                // Node: traverse
                intersect_level(child, body, collider, cm);
            }
        }
    }
    
public:
    
    void render()
    {
        
    }
    
private:
    
    void render_level()
    {
        
    }
};

#endif /* hquad_tree_h */
