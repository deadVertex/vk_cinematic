internal AabbTree BuildAabbTree(vec3 *boxMins, vec3 *boxMaxes, u32 count,
    u32 maxNodes, MemoryArena *arena, MemoryArena *tempArena)
{
    AabbTree tree = {};
    tree.nodes = AllocateArray(arena, AabbTreeNode, maxNodes);
    tree.max = maxNodes;

    u32 *unmergedNodeIndices = AllocateArray(tempArena, u32, maxNodes);
    u32 unmergedNodeCount = 0;

    for (u32 leafIndex = 0; leafIndex < count; ++leafIndex)
    {
        Assert(tree.count < tree.max);
        AabbTreeNode *node = tree.nodes + tree.count++;
        node->min = boxMins[leafIndex];
        node->max = boxMaxes[leafIndex];
        node->children[0] = NULL;
        node->children[1] = NULL;
        node->leafIndex = leafIndex;

        unmergedNodeIndices[leafIndex] = leafIndex;
    }
    unmergedNodeCount = count;

    while (unmergedNodeCount > 1)
    {
        u32 newUnmergedNodeCount = 0;

        // Join nodes with their nearest neighbors to build tree
        for (u32 index = 0; index < unmergedNodeCount; ++index)
        {
            u32 nodeIndex = unmergedNodeIndices[index];
            AabbTreeNode *node = tree.nodes + nodeIndex;
            vec3 centroid = (node->max + node->min) * 0.5f;

            f32 closestDistance = F32_MAX;
            f32 minVolume = F32_MAX;
            AabbTreeNode *closestPartnerNode = NULL;
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

                u32 partnerNodeIndex = unmergedNodeIndices[partnerIndex];
                AabbTreeNode *partnerNode = tree.nodes + partnerNodeIndex;

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
                Assert(tree.count < tree.max);
                u32 newNodeIndex = tree.count++;
                AabbTreeNode *newNode = tree.nodes + newNodeIndex;
                newNode->min = Min(node->min, closestPartnerNode->min);
                newNode->max = Max(node->max, closestPartnerNode->max);
                newNode->children[0] = node;
                newNode->children[1] = closestPartnerNode;
                newNode->leafIndex = 0;

                // Start overwriting the old entries with combined nodes, should
                // be roughly half the length as the original array
                Assert(newUnmergedNodeCount <= index);
                unmergedNodeIndices[newUnmergedNodeCount++] = newNodeIndex;

                // Remove partner from unmerged node indices array
                u32 last = unmergedNodeCount - 1;
                unmergedNodeIndices[closestPartnerIndex] =
                    unmergedNodeIndices[last];
                unmergedNodeCount--;
            }
        }

        unmergedNodeCount = newUnmergedNodeCount;
    }

    // Assume the last node created is the root node
    tree.root = tree.nodes + (tree.count - 1);

    return tree;
}

struct StackNode
{
    AabbTreeNode *node;
    u32 depth;
};

internal u32 RayIntersectAabbTree(AabbTree tree, vec3 rayOrigin,
    vec3 rayDirection, u32 *leafIndices, f32 *tValues, u32 maxLeaves)
{
    u32 count = 0;

    StackNode stack[2][STACK_SIZE];
    u32 stackSizes[2];
    stack[0][0].node = tree.root;
    stack[0][0].depth = 0;
    stackSizes[0] = 1;
    stackSizes[1] = 0;

    u32 readIndex = 0;
    u32 writeIndex = 1;

    while (stackSizes[readIndex] > 0)
    {
        u32 topIndex = --stackSizes[readIndex];
        StackNode top = stack[readIndex][topIndex];
        AabbTreeNode *node = top.node;

        f32 t = RayIntersectAabb(node->min, node->max, rayOrigin, rayDirection);
        if (t >= 0.0f)
        {
            if (node->children[0] != NULL)
            {
                // Assuming that this is always true?
                Assert(node->children[1] != NULL);

                Assert(stackSizes[writeIndex] + 2 <= STACK_SIZE);
                StackNode newNodes[2] = {
                    { node->children[0], top.depth + 1},
                    { node->children[1], top.depth + 1}
                };

                u32 entryIndex = stackSizes[writeIndex];
                stack[writeIndex][entryIndex] = newNodes[0];
                stack[writeIndex][entryIndex + 1] = newNodes[1];
                stackSizes[writeIndex] += 2;
            }
            else
            {
                Assert(node->children[1] == NULL);
                if (count < maxLeaves)
                {
                    u32 index = count++;
                    leafIndices[index] = node->leafIndex;
                    tValues[index] = t;
                }
                else
                {
                    break;
                }
            }
        }

        if (stackSizes[readIndex] == 0)
        {
            SwapU32(&readIndex, &writeIndex);
        }
    }

    return count;
}
