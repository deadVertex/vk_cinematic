#include "unity.h"

#include "platform.h"
#include "math_lib.h"
#include "memory_pool.h"
#include "bvh.h"

#include "simd.h"
#include "aabb.h"
#include "ray_intersection.h"

#include "custom_assertions.h"

#include "memory_pool.cpp"
#include "ray_intersection.cpp"

// Include cpp file for faster unity build
#include "bvh.cpp"

#define MEMORY_ARENA_SIZE Megabytes(1)

MemoryArena memoryArena;

void setUp(void)
{
    // set stuff up here
    ClearToZero(memoryArena.base, (u32)memoryArena.size);
    ResetMemoryArena(&memoryArena);
}

void tearDown(void)
{
    // clean stuff up here
}

void TestCreateBvh()
{
    vec3 aabbMin[] = {Vec3(-0.5)};
    vec3 aabbMax[] = {Vec3(0.5)};

    // Build Bvh
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));
    TEST_ASSERT_NOT_NULL(worldBvh.root);

    //  Check node min and max
    AssertWithinVec3(EPSILON, aabbMin[0], worldBvh.root->min);
    AssertWithinVec3(EPSILON, aabbMax[0], worldBvh.root->max);
}

void TestCreateBvhMultipleNodes()
{
    vec3 aabbMin[] = {Vec3(-0.5), Vec3(1)};
    vec3 aabbMax[] = {Vec3(0.5), Vec3(2)};

    // Build Bvh
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));

    TEST_ASSERT_NOT_NULL(worldBvh.root);
    TEST_ASSERT_NOT_NULL(worldBvh.root->children[0]);
    TEST_ASSERT_NOT_NULL(worldBvh.root->children[1]);

    // Check memory pool for world bvh
    TEST_ASSERT_NOT_NULL(worldBvh.memoryPool.storage);
    TEST_ASSERT_EQUAL_UINT32(sizeof(bvh_Node), worldBvh.memoryPool.objectSize);

    // Check bounding volume for root contains all children
    AssertWithinVec3(EPSILON, Vec3(-0.5f), worldBvh.root->min);
    AssertWithinVec3(EPSILON, Vec3(2), worldBvh.root->max);
}

b32 FindLeafIndex(bvh_Node *node, u32 index)
{
    b32 result = false;

    if (node->leafIndex == index)
    {
        result = true;
    }
    else
    {
        for (u32 i = 0; i < 4; ++i)
        {
            if (node->children[i] != NULL)
            {
                if (FindLeafIndex(node->children[i], index))
                {
                    result = true;
                    break;
                }
            }
        }
    }

    return result;
}

void TestBvhAllLeavesReachable()
{
    vec3 aabbMin[17];
    vec3 aabbMax[17];
    for (u32 i = 0; i < 17; i++)
    {
        vec3 center = Vec3((f32)i, 0, 0);
        aabbMin[i] = center - Vec3(0.25f);
        aabbMax[i] = center + Vec3(0.25f);
    }

    // Build Bvh
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));

    // Check that we can find each leaf index in the tree
    for (u32 i = 0; i < 17; i++)
    {
        TEST_ASSERT_TRUE(FindLeafIndex(worldBvh.root, i));
    }
}

b32 CheckNodeAabbContainsChildNodeAabbsRecursive(bvh_Node *node)
{
    b32 result = true;
    for (u32 i = 0; i < 4; i++)
    {
        if (node->children[i] != NULL)
        {
            if (!AabbContainsAabb(node->min, node->max, node->children[i]->min,
                    node->children[i]->max))
            {
                result = false;
                break;
            }
        }
    }

    for (u32 i = 0; i < 4; i++)
    {
        if (node->children[i] != NULL)
        {
            if (!CheckNodeAabbContainsChildNodeAabbsRecursive(
                    node->children[i]))
            {
                result = false;
                break;
            }
        }
    }


    return result;
}

void GenerateAabbGrid4x4(vec3 *aabbMin, vec3 *aabbMax, u32 count)
{
    f32 size = 0.5f;
    f32 gridMin = -10.0f;
    f32 gridMax = 10.0f;
    for (u32 z = 0; z <= 3; z++)
    {
        for (u32 x = 0; x <= 3; x++)
        {
            vec3 center = {};
            f32 fx = x / 3.0f;
            f32 fz = z / 3.0f;

            center.x = Lerp(gridMin, gridMax, fx);
            center.z = Lerp(gridMin, gridMax, fz);

            u32 index = z * 4 + x;

            aabbMin[index] = center - Vec3(size);
            aabbMax[index] = center + Vec3(size);
        }
    }
}

void TestBvhIntermediateNodesContainChildNodes()
{
    // Given a grid of 4x4 AABBs
    vec3 aabbMin[16] = {};
    vec3 aabbMax[16] = {};

    GenerateAabbGrid4x4(aabbMin, aabbMax, ArrayCount(aabbMin));

    // When I build a BVH
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));

    // Then all child nodes are contained within root node
    TEST_ASSERT_TRUE(
        CheckNodeAabbContainsChildNodeAabbsRecursive(worldBvh.root));
}

void TestBvhRayIntersectGrid()
{
    // Given a BVH of a grid of 4x4 AABBs
    vec3 aabbMin[16] = {};
    vec3 aabbMax[16] = {};

    GenerateAabbGrid4x4(aabbMin, aabbMax, ArrayCount(aabbMin));

    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));

    bvh_Node *intersectedNodes[4];

    // When I intersect a ray against one of the grid rows
    bvh_IntersectRayResult result = bvh_IntersectRay(&worldBvh, 
            Vec3(-20, 0, -10), Vec3(1, 0, 0), intersectedNodes, 4);

    // Then I get 4 intersections and no error occurred
    TEST_ASSERT_FALSE(result.errorOccurred);
    TEST_ASSERT_EQUAL_UINT32(4, result.count);
}

// TODO: Better name
void TestCreateBvhMultipleNodes2()
{
    // Given a grid of 4x4 AABBs
    vec3 aabbMin[16] = {};
    vec3 aabbMax[16] = {};

    // FIXME: Don't duplicate these constants
    f32 size = 0.5f;
    f32 gridMin = -10.0f;
    f32 gridMax = 10.0f;

    GenerateAabbGrid4x4(aabbMin, aabbMax, ArrayCount(aabbMin));

    // When we build a BVH
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));

    // Then the root node contains all child nodes?
    bvh_Node *root = worldBvh.root;
    TEST_ASSERT_NOT_NULL(root);
    AssertWithinVec3(
        EPSILON, Vec3(gridMin - size, -size, gridMin - size), root->min);
    AssertWithinVec3(
            EPSILON, Vec3(gridMax + size, size, gridMax + size), root->max);
}

void TestIntersectEmptyBvh()
{
    // Given an empty bvh
    bvh_Tree tree = {};

    bvh_Node *intersectedNodes[4] = {};

    vec3 rayOrigin = Vec3(0, 0, 10);
    vec3 rayDirection = Vec3(0, 0, -1);

    // When we test ray intersection against it
    bvh_IntersectRayResult result = bvh_IntersectRay(&tree, rayOrigin,
        rayDirection, intersectedNodes, ArrayCount(intersectedNodes));

    // Check that it returns no intersections (and doesn't crash)
    TEST_ASSERT_EQUAL_UINT32(0, result.count);
}

void TestBvh()
{
    vec3 aabbMin[] = {Vec3(-0.5f, -0.5f, -0.5f), Vec3(-0.5f, -0.5f, -1.5f)};
    vec3 aabbMax[] = {Vec3(0.5f, 0.5f, 0.5f), Vec3(0.5f, 0.5f, -0.5f)};

    // Build Bvh
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));

    bvh_Node *intersectedNodes[4] = {};

    vec3 rayOrigin = Vec3(0, 0, 10);
    vec3 rayDirection = Vec3(0, 0, -1);

    // Test ray intersection against it
    bvh_IntersectRayResult result = bvh_IntersectRay(&worldBvh, rayOrigin,
        rayDirection, intersectedNodes, ArrayCount(intersectedNodes));

    // Check that it returns the expected objects
    TEST_ASSERT_EQUAL_UINT32(2, result.count);
    TEST_ASSERT_FALSE(result.errorOccurred);

    // Test oversubscription
    bvh_IntersectRayResult result2 = bvh_IntersectRay(&worldBvh, rayOrigin,
        rayDirection, intersectedNodes, 1);
    TEST_ASSERT_EQUAL_UINT32(1, result2.count);
    TEST_ASSERT_TRUE(result2.errorOccurred);
}

void TestBvhFindClosestPartnerNode()
{
    bvh_Node nodes[3];
    for (u32 x = 0; x < 3; ++x)
    {
        vec3 origin = Vec3(-5, -5, 0);
        vec3 p = origin + Vec3((f32)x, 0, 0) * 3.0f;

        Aabb aabb =
            TransformAabb(Vec3(-0.5), Vec3(0.5), p, Quat(), Vec3(1));

        nodes[x].min = aabb.min;
        nodes[x].max = aabb.max;
        nodes[x].leafIndex = x;
    }

    bvh_Node *unmergedNodes[3] = {nodes + 0, nodes + 1, nodes + 2};
    bvh_FindClosestPartnerNodeResult result =
        bvh_FindClosestPartnerNode(unmergedNodes[0], unmergedNodes, 3);
    TEST_ASSERT_EQUAL_UINT32(1, result.index);
    TEST_ASSERT_EQUAL_PTR(unmergedNodes[1], result.node);
}

void TestBvhFindNeighbours()
{
    // Given a list of 5 nodes
    bvh_Node nodes[5];
    for (u32 x = 0; x < 5; ++x)
    {
        vec3 origin = Vec3(-5, -5, 0);
        vec3 p = origin + Vec3((f32)x, 0, 0) * 5.0f;

        Aabb aabb =
            TransformAabb(Vec3(-0.5), Vec3(0.5), p, Quat(), Vec3(1));

        nodes[x].min = aabb.min;
        nodes[x].max = aabb.max;
        nodes[x].leafIndex = x;
    }

    // When we search for 3 neighbouring nodes
    bvh_Node *unmergedNodes[5] = {
        nodes + 0, nodes + 1, nodes + 2, nodes + 3, nodes + 4};

    bvh_NodeIndexDistSqPair pairs[5] = {};

    u32 neighbourIndices[3] = {};
    u32 foundCount = bvh_FindNeighbours(
        4, unmergedNodes, pairs, 5, neighbourIndices, 3);

    // Then we find the closest 3
    TEST_ASSERT_EQUAL_UINT32(3, foundCount);
    TEST_ASSERT_EQUAL_UINT32(3, neighbourIndices[0]);
    TEST_ASSERT_EQUAL_UINT32(2, neighbourIndices[1]);
    TEST_ASSERT_EQUAL_UINT32(1, neighbourIndices[2]);
}

// Don't think this test is valid anymore if we change to 4-node BVH tree
#if 0
void TestBvhDuplicateIntersectionsBug()
{
    vec3 aabbMin[3];
    vec3 aabbMax[3];

    for (u32 x = 0; x < 3; ++x)
    {
        vec3 origin = Vec3(-5, -5, 0);
        vec3 p = origin + Vec3((f32)x, 0, 0) * 3.0f;

        Aabb aabb =
            TransformAabb(Vec3(-0.5), Vec3(0.5), p, Quat(), Vec3(1));

        aabbMin[x] = aabb.min;
        aabbMax[x] = aabb.max;
    }

    // Build Bvh
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));
    /*    [root]
     *    /    \
     *   []    [2]
     *  /  \
     * [0] [1]
     */
    TEST_ASSERT_EQUAL_UINT32(U32_MAX, worldBvh.root->leafIndex);
    TEST_ASSERT_NOT_NULL(worldBvh.root->children[1]);
    bvh_Node *branchNode = worldBvh.root->children[1];
    TEST_ASSERT_EQUAL_UINT32(0, branchNode->children[0]->leafIndex);
    TEST_ASSERT_EQUAL_UINT32(1, branchNode->children[1]->leafIndex);
    TEST_ASSERT_EQUAL_UINT32(2, worldBvh.root->children[0]->leafIndex);

    bvh_Node *intersectedNodes[3] = {};

    vec3 rayOrigin = Vec3(-10, -5, 0);
    vec3 rayDirection = Vec3(1, 0, 0);

    // Test ray intersection against it
    bvh_IntersectRayResult result = bvh_IntersectRay(&worldBvh, rayOrigin,
        rayDirection, intersectedNodes, ArrayCount(intersectedNodes));

    // Check that it returns the expected objects
    TEST_ASSERT_EQUAL_UINT32(3, result.count);
    TEST_ASSERT_FALSE(result.errorOccurred);

    TEST_ASSERT_TRUE(
        intersectedNodes[0]->leafIndex != intersectedNodes[1]->leafIndex &&
        intersectedNodes[0]->leafIndex != intersectedNodes[2]->leafIndex);
}
#endif


int main()
{
    InitializeMemoryArena(
        &memoryArena, calloc(1, MEMORY_ARENA_SIZE), MEMORY_ARENA_SIZE);

    UNITY_BEGIN();
    RUN_TEST(TestCreateBvh);
    RUN_TEST(TestCreateBvhMultipleNodes);
    RUN_TEST(TestCreateBvhMultipleNodes2);
    RUN_TEST(TestIntersectEmptyBvh);
    RUN_TEST(TestBvh);
    RUN_TEST(TestBvhAllLeavesReachable);
    RUN_TEST(TestBvhIntermediateNodesContainChildNodes);
    RUN_TEST(TestBvhRayIntersectGrid);
    RUN_TEST(TestBvhFindClosestPartnerNode);
    //RUN_TEST(TestBvhDuplicateIntersectionsBug);

    RUN_TEST(TestBvhFindNeighbours);

    free(memoryArena.base);

    return UNITY_END();
}
