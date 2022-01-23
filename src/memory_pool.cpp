internal MemoryPool CreateMemoryPool(
    void *storage, u32 objectSize, u32 capacity)
{
    MemoryPool result = {};
    result.storage = (u8 *)storage;
    result.objectSize = objectSize;
    result.capacity = capacity;

    // Build free list using the memory blocks themselves
    for (u32 i = 0; i < capacity; ++i)
    {
        void *p = result.storage + objectSize * i;
        if (i == capacity - 1)
        {
            // Set last block to U32_MAX to signal end
            *(u32 *)p = U32_MAX;
        }
        else
        {
            // Set index of the next block
            *(u32 *)p = i + 1;
        }
    }
    return result;
}

internal void *AllocateFromPool(MemoryPool *pool, u32 objectSize)
{
    Assert(pool->objectSize == objectSize);

    void *result = NULL;

    u32 index = pool->headIndex;
    if (index != U32_MAX)
    {
        result = pool->storage + pool->objectSize * index;

        // Extract next index from memory block we just allocated
        pool->headIndex = *(u32 *)result;

        ClearToZero(result, pool->objectSize);
    }

    return result;
}

internal void FreeFromPool(MemoryPool *pool, void *p)
{
    // Check that p is valid
    Assert(p >= pool->storage);
    Assert(p < pool->storage + pool->capacity * pool->objectSize);
    Assert(((u8 *)p - pool->storage) % pool->objectSize == 0);

    u32 index =
        SafeTruncateU64ToU32(((u8 *)p - pool->storage) / pool->objectSize);
    *(u32 *)p = pool->headIndex;
    pool->headIndex = index;
}
