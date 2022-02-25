// TODO: This should be a parameter of the tree structure for iteration
#define BVH_STACK_SIZE 256

inline bvh_Node *bvh_AllocateNode(MemoryPool *pool)
{
    bvh_Node *node = (bvh_Node*)AllocateFromPool(pool, sizeof(bvh_Node));
    ClearToZero(node, sizeof(*node));
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
    u32 searchCount = MinU32(count, maxNeighbors + 1);
    for (u32 i = 0; i < searchCount; ++i)
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

inline u32 bvh_FindNodeIndex(bvh_Node **nodes, u32 count, bvh_Node *node)
{
    u32 index = U32_MAX;
    for (u32 i = 0; i < count; i++)
    {
        if (nodes[i] == node)
        {
            index = i;
            break;
        }
    }

    return index;
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

    // TODO: Could do this from a single allocation and use manual indexing
    bvh_Node **unmergedNodes[2];
    unmergedNodes[0] = AllocateArray(arena, bvh_Node *, count);
    unmergedNodes[1] = AllocateArray(arena, bvh_Node *, count);

    bvh_NodeIndexDistSqPair *pairs =
        AllocateArray(arena, bvh_NodeIndexDistSqPair, count);

    u32 unmergedNodeCount[2] = {count, 0};
    u32 readIndex = 0;
    u32 writeIndex = 1;

    // Populate the read buffer
    for (u32 leafIndex = 0; leafIndex < count; ++leafIndex)
    {
        bvh_Node *node = bvh_AllocateNode(pool);
        Assert(node != NULL);

        node->min = aabbMin[leafIndex];
        node->max = aabbMax[leafIndex];
        node->children[0] = NULL;
        node->children[1] = NULL;
        node->children[2] = NULL;
        node->children[3] = NULL;
        node->leafIndex = leafIndex;

        unmergedNodes[readIndex][leafIndex] = node;
    }

    // Assign to unmerged node to handle case when we only have a single leaf
    // node tree
    bvh_Node *lastAllocatedNode = unmergedNodes[readIndex][0];

    // TODO: This needs refactored and tested more thoroughly
    while (unmergedNodeCount[readIndex] > 1)
    {
        unmergedNodeCount[writeIndex] = 0;

        // Join nodes with their nearest neighbors to build tree
        for (u32 index = 0; index < unmergedNodeCount[readIndex]; index++)
        {
            bvh_Node *node = unmergedNodes[readIndex][index];
            vec3 centroid = (node->max + node->min) * 0.5f;

            u32 neighbourIndices[3];
            u32 neighborCount =
                bvh_FindNeighbours(index, unmergedNodes[readIndex], pairs,
                    unmergedNodeCount[readIndex], neighbourIndices, 3);
            Assert(neighborCount <= 3);

            if (neighborCount > 0)
            {
                bvh_Node *neighbors[3];
                for (u32 i = 0; i < neighborCount; i++)
                {
                    neighbors[i] =
                        unmergedNodes[readIndex][neighbourIndices[i]];
                }

                // Create combined node
                bvh_Node *newNode = bvh_AllocateNode(pool);
                Assert(newNode != NULL);
                newNode->leafIndex = U32_MAX;
                newNode->min = node->min;
                newNode->max = node->max;
                newNode->children[0] = node;
                for (u32 i = 0; i < neighborCount; i++)
                {
                    newNode->min = Min(newNode->min, neighbors[i]->min);
                    newNode->max = Max(newNode->max, neighbors[i]->max);
                    newNode->children[i+1] = neighbors[i];
                }

                lastAllocatedNode = newNode;

                // Write new node into our write buffer
                Assert(unmergedNodeCount[writeIndex] <= index);
                u32 newNodeIndex = unmergedNodeCount[writeIndex]++;
                unmergedNodes[writeIndex][newNodeIndex] = newNode;

                // Remove neighbors from unmerged node indices array
                for (u32 i = 0; i < neighborCount; i++)
                {
                    u32 last = unmergedNodeCount[readIndex] - 1;

                    // NOTE: Can't use our indices here because they will
                    // change as soon as we start modifying the unmergedNodes
                    // array
                    u32 foundIndex = bvh_FindNodeIndex(unmergedNodes[readIndex],
                        unmergedNodeCount[readIndex], neighbors[i]);
                    Assert(foundIndex != U32_MAX);

                    // Swap and decrement read array size
                    unmergedNodes[readIndex][foundIndex] =
                        unmergedNodes[readIndex][last];
                    unmergedNodeCount[readIndex]--;
                }
            }
        }

        // Swap read a write buffers
        u32 temp = readIndex;
        readIndex = writeIndex;
        writeIndex = temp;
    }

    // NOTE: This frees both unmergedNodes buffers and the pairs array
    FreeFromMemoryArena(arena, unmergedNodes[0]);

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

            vec3 boxMins[4];
            vec3 boxMaxes[4];
            u32 childCount = 0;
            for (u32 i = 0; i < 4; i++)
            {
                // TODO: Could do this better
                if (node->children[i] != NULL)
                {
                    boxMins[i] = node->children[i]->min;
                    boxMaxes[i] = node->children[i]->max;
                    childCount++;
                }
            }

#if 1
            // FIXME: Remove this, we've confirmed that its the BVH
            // construction that is broken!
            for (u32 i = 0; i < childCount; i++)
            {
                f32 t = RayIntersectAabb(
                    boxMins[i], boxMaxes[i], rayOrigin, rayDirection);
                if (t >= 0.0f)
                {
                    u32 entryIndex = stackSizes[writeIndex];
                    Assert(entryIndex <= BVH_STACK_SIZE);
                    stack[writeIndex][entryIndex] = node->children[i];
                    stackSizes[writeIndex] += 1;
                }
            }
#else
            // Test each child node with single call simd_RayIntersectAabb4
            // which will test multiple AABBs at once
            u32 mask = simd_RayIntersectAabb4(
                boxMins, boxMaxes, rayOrigin, invRayDirection);
            for (u32 i = 0; i < childCount; i++)
            {
                if ((mask & (1<<i)) != 0)
                {
                    u32 entryIndex = stackSizes[writeIndex];
                    Assert(entryIndex <= BVH_STACK_SIZE);
                    stack[writeIndex][entryIndex] = node->children[i];
                    stackSizes[writeIndex] += 1;
                }
            }
#endif
        }
        else
        {
            if (result.count < maxIntersections)
            {
                u32 index = result.count++;
                intersectedNodes[index] = node;
            }
            else
            {
                // Insufficient space to store all results, set error bit
                result.errorOccurred = true;
                //Assert(!"Set error flag");
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
