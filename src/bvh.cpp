// TODO: This should be a parameter of the tree structure for iteration
#define BVH_STACK_SIZE 256

inline bvh_Node *bvh_AllocateNode(MemoryPool *pool)
{
    bvh_Node *node = (bvh_Node*)AllocateFromPool(pool, sizeof(bvh_Node));
    return node;
}

bvh_Tree bvh_CreateTree(
    MemoryArena *arena, vec3 *aabbMin, vec3 *aabbMax, u32 count)
{
    bvh_Tree result = {};

    // TODO: Calculate number of nodes to allocate
    u32 nodeCapacity = count * 2;
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

            f32 closestDistance = F32_MAX;
            f32 minVolume = F32_MAX;
            bvh_Node *closestPartnerNode = NULL;
            u32 closestPartnerIndex = 0;

            // TODO: Spatial hashing
            // Find a node who's centroid is closest to ours
            for (u32 partnerIndex = 0; partnerIndex < unmergedNodeCount;
                 ++partnerIndex)
            {
                if (partnerIndex == index)
                {
                    continue;
                }

                bvh_Node *partnerNode = unmergedNodes[partnerIndex];

#ifdef MIN_VOLUME
                vec3 min = Min(node->min, partnerNode->min);
                vec3 max = Max(node->max, partnerNode->max);
                f32 volume = (max.x - min.x) * (max.y - min.y) * (max.z - min.z);
                if (volume < minVolume)
                {
                    minVolume = volume;
                    closestPartnerNode = partnerNode;
                    closestPartnerIndex = partnerIndex;
                }
#else
                vec3 partnerCentroid =
                    (partnerNode->max + partnerNode->min) * 0.5f;

                // Find minimal distance via length squared to avoid sqrt
                f32 dist = LengthSq(partnerCentroid - centroid);
                if (dist < closestDistance)
                {
                    closestDistance = dist;
                    closestPartnerNode = partnerNode;
                    closestPartnerIndex = partnerIndex;
                }
#endif
            }

            if (closestPartnerNode != NULL)
            {
                // Create combined node
                bvh_Node *newNode = bvh_AllocateNode(pool);
                Assert(newNode != NULL);
                newNode->min = Min(node->min, closestPartnerNode->min);
                newNode->max = Max(node->max, closestPartnerNode->max);
                newNode->children[0] = node;
                newNode->children[1] = closestPartnerNode;
                newNode->leafIndex = 0;

                lastAllocatedNode = newNode;

                // Start overwriting the old entries with combined nodes, should
                // be roughly half the length as the original array
                Assert(newUnmergedNodeCount <= index);
                unmergedNodes[newUnmergedNodeCount++] = newNode;

                // Remove partner from unmerged node indices array
                u32 last = unmergedNodeCount - 1;
                unmergedNodes[closestPartnerIndex] = unmergedNodes[last];
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

    u32 stackSizes[2];
    stack[0][0] = tree->root;
    stackSizes[0] = tree->root != NULL ? 1 : 0;
    stackSizes[1] = 0;

    u32 readIndex = 0;
    u32 writeIndex = 1;

    while (stackSizes[readIndex] > 0)
    {
        u32 topIndex = --stackSizes[readIndex];
        bvh_Node *node = stack[readIndex][topIndex];

        f32 t = RayIntersectAabb(node->min, node->max, rayOrigin, rayDirection);
        if (t >= 0.0f)
        {
            if (node->children[0] != NULL)
            {
                // Assuming that this is always true?
                Assert(node->children[1] != NULL);

                Assert(stackSizes[writeIndex] + 2 <= BVH_STACK_SIZE);

                u32 entryIndex = stackSizes[writeIndex];
                stack[writeIndex][entryIndex] = node->children[0];
                stack[writeIndex][entryIndex + 1] = node->children[1];
                stackSizes[writeIndex] += 2;
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
        }

        if (stackSizes[readIndex] == 0)
        {
            SwapU32(&readIndex, &writeIndex);
        }
    }

    return result;
}
