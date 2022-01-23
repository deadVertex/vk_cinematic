#pragma once

struct MemoryPool
{
    u8 *storage;
    u32 objectSize;
    u32 capacity; // NOTE: This is number of blocks of objectSize
    u32 headIndex;
};
