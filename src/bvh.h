// Bounding Volume Hierarchy
#pragma once

#define BVH_CHILDREN_PER_NODE 2
struct bvh_Node
{
    vec3 min;
    vec3 max;
    bvh_Node *children[BVH_CHILDREN_PER_NODE];
    // TODO: Do we need a childCount variable?
    u32 leafIndex;
};

struct bvh_Tree
{
    bvh_Node *root;
    MemoryPool memoryPool;
};
