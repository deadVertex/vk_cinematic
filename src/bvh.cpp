// TODO: This should be a parameter of the tree structure for iteration
#define BVH_STACK_SIZE 256

inline bvh_Node *bvh_AllocateNode(MemoryPool *pool)
{
    bvh_Node *node = (bvh_Node*)AllocateFromPool(pool, sizeof(bvh_Node));
    return node;
}

struct bvh_FindClosestPartnerNodeResult
{
    bvh_Node *node;
    u32 index;
};

bvh_FindClosestPartnerNodeResult bvh_FindClosestPartnerNode(
    bvh_Node *node, bvh_Node **allNodes, u32 count)
{
    vec3 centroid = (node->max + node->min) * 0.5f;

    f32 closestDistance = F32_MAX;
    bvh_FindClosestPartnerNodeResult result = {};

    // TODO: Spatial hashing
    // Find a node who's centroid is closest to ours
    for (u32 index = 0; index < count; ++index)
    {
        bvh_Node *partnerNode = allNodes[index];

        // Skip ourselves
        if (partnerNode == node)
        {
            continue;
        }

        vec3 partnerCentroid = (partnerNode->max + partnerNode->min) * 0.5f;

        // Find minimal distance via length squared to avoid sqrt
        f32 dist = LengthSq(partnerCentroid - centroid);
        if (dist < closestDistance)
        {
            closestDistance = dist;
            result.node = partnerNode;
            result.index = index;
        }
    }

    return result;
}

struct bvh_NodeIndexDistSqPair
{
    f32 distSq;
    u32 nodeIndex;
};

inline i32 CompareNodeIndexDistSqPair(const void *p1, const void *p2)
{
    bvh_NodeIndexDistSqPair a = *(bvh_NodeIndexDistSqPair*)p1;
    bvh_NodeIndexDistSqPair b = *(bvh_NodeIndexDistSqPair*)p2;

    if (a.distSq < b.distSq)
    {
        return -1;
    }
    else if (a.distSq > b.distSq)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// TODO: Spatial hashing
u32 bvh_FindNeighbours(u32 nodeIndex, bvh_Node **allNodes,
    bvh_NodeIndexDistSqPair *pairs, u32 count, u32 *neighbourIndices, u32 maxNeighbors)
{
    bvh_Node *node = allNodes[nodeIndex];
    vec3 centroid = (node->max + node->min) * 0.5f;

    // Compute dist^2 to node for all nodes
    for (u32 i = 0; i < count; i++)
    {
        bvh_Node *neighbor = allNodes[i];
        vec3 neighborCentroid = (neighbor->max + neighbor->min) * 0.5f;

        vec3 d = neighborCentroid - centroid;
        pairs[i].distSq = LengthSq(d);
        pairs[i].nodeIndex = i;
    }

    // Sort by dist^2
    qsort(pairs, count, sizeof(pairs[0]), CompareNodeIndexDistSqPair);

    // Take the top N
    u32 result = 0;
    for (u32 i = 0; i < maxNeighbors + 1; ++i)
    {
        bvh_NodeIndexDistSqPair pair = pairs[i];
        if (pair.nodeIndex == nodeIndex)
        {
            continue;
        }

        Assert(result < maxNeighbors);
        neighbourIndices[result++] = pair.nodeIndex;
    }

    return result;
}

bvh_Tree bvh_CreateTree(
    MemoryArena *arena, vec3 *aabbMin, vec3 *aabbMax, u32 count)
{
    bvh_Tree result = {};

    // TODO: Calculate number of nodes to allocate
    u32 nodeCapacity = count * 10;
    bvh_Node *nodeStorage = AllocateArray(arena, bvh_Node, nodeCapacity);

    result.memoryPool =
        CreateMemoryPool(nodeStorage, sizeof(bvh_Node), nodeCapacity);

    MemoryPool *pool = &result.memoryPool;

    bvh_Node **unmergedNodes = AllocateArray(arena, bvh_Node*, nodeCapacity);

    for (u32 leafIndex = 0; leafIndex < count; ++leafIndex)
    {
        bvh_Node *node = bvh_AllocateNode(pool);
        Assert(node != NULL);

        node->min = aabbMin[leafIndex];
        node->max = aabbMax[leafIndex];
        node->children[0] = NULL;
        node->children[1] = NULL;
        node->leafIndex = leafIndex;

        unmergedNodes[leafIndex] = node;
    }

    u32 unmergedNodeCount = count;

    // Assign to unmerged node to handle case when we only have a single leaf
    // node tree
    bvh_Node *lastAllocatedNode = unmergedNodes[0];

    // TODO: This needs refactored and tested more thoroughly
    while (unmergedNodeCount > 1)
    {
        u32 newUnmergedNodeCount = 0;

        // Join nodes with their nearest neighbors to build tree
        for (u32 index = 0; index < unmergedNodeCount; ++index)
        {
            bvh_Node *node = unmergedNodes[index];
            vec3 centroid = (node->max + node->min) * 0.5f;

            bvh_FindClosestPartnerNodeResult closest =
                bvh_FindClosestPartnerNode(
                    node, unmergedNodes, unmergedNodeCount);

            if (closest.node != NULL)
            {
                // Create combined node
                bvh_Node *newNode = bvh_AllocateNode(pool);
                Assert(newNode != NULL);
                newNode->min = Min(node->min, closest.node->min);
                newNode->max = Max(node->max, closest.node->max);
                newNode->children[0] = node;
                newNode->children[1] = closest.node;
                newNode->leafIndex = U32_MAX;

                lastAllocatedNode = newNode;

                // Start overwriting the old entries with combined nodes, should
                // be roughly half the length as the original array
                Assert(newUnmergedNodeCount <= index);
                unmergedNodes[newUnmergedNodeCount++] = newNode;

                // Remove partner from unmerged node indices array
                u32 last = unmergedNodeCount - 1;
                unmergedNodes[closest.index] = unmergedNodes[last];
                unmergedNodeCount--;
            }
        }

        unmergedNodeCount = newUnmergedNodeCount;
    }

    FreeFromMemoryArena(arena, unmergedNodes);

    result.root = lastAllocatedNode;

    return result;
}

// TODO: tmax
bvh_IntersectRayResult bvh_IntersectRay(bvh_Tree *tree, vec3 rayOrigin,
    vec3 rayDirection, bvh_Node **intersectedNodes, u32 maxIntersections)
{
    bvh_IntersectRayResult result = {};

    // TODO: This should be a parameter of the tree structure for iteration
    bvh_Node *stack[2][BVH_STACK_SIZE];
    u32 stackSizes[2] = {};

    u32 readIndex = 0;
    u32 writeIndex = 1;

    vec3 invRayDirection = Inverse(rayDirection);

    bvh_Node *root = tree->root;
    if (root != NULL)
    {
        u32 mask = simd_RayIntersectAabb4(
            &root->min, &root->max, rayOrigin, invRayDirection);

        // NOTE: We're only testing 1 box so its just checking if 0th bit is set
        if ((mask & 0x1) == 0x1)
        {
            stack[0][0] = root;
            stackSizes[0] = 1;
        }
    }

    while (stackSizes[readIndex] > 0)
    {
        u32 topIndex = --stackSizes[readIndex];
        bvh_Node *node = stack[readIndex][topIndex];

        if (node->children[0] != NULL)
        {
            // Assuming that this is always true?
            Assert(node->children[1] != NULL);

            result.aabbTestCount++;

            vec3 boxMins[2] = {node->children[0]->min, node->children[1]->min};
            vec3 boxMaxes[2] = {node->children[0]->max, node->children[1]->max};

            // Test each child node
            // Use simd_RayIntersectAabb4 to test multiple AABBs at once
            u32 mask = simd_RayIntersectAabb4(
                boxMins, boxMaxes, rayOrigin, invRayDirection);
            if ((mask & 0x1) == 0x1)
            {
                u32 entryIndex = stackSizes[writeIndex];
                Assert(entryIndex <= BVH_STACK_SIZE);
                stack[writeIndex][entryIndex] = node->children[0];
                stackSizes[writeIndex] += 1;
            }
            if ((mask & 0x2) == 0x2)
            {
                u32 entryIndex = stackSizes[writeIndex];
                Assert(entryIndex <= BVH_STACK_SIZE);
                stack[writeIndex][entryIndex] = node->children[1];
                stackSizes[writeIndex] += 1;
            }
        }
        else
        {
            Assert(node->children[1] == NULL);
            if (result.count < maxIntersections)
            {
                u32 index = result.count++;
                intersectedNodes[index] = node;
            }
            else
            {
                // Insufficient space to store all results, set error bit
                result.errorOccurred = true;
                break;
            }
        }

        if (stackSizes[readIndex] == 0)
        {
            SwapU32(&readIndex, &writeIndex);
        }
    }

    return result;
}
