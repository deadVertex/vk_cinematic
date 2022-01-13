#pragma once

// FIXME: Switch to linked list!
struct WorkQueue
{
    i32 volatile head;
    i32 volatile tail;
    u32 objectSize;
    u32 maxObjects;
    void *buffer;
};

inline WorkQueue CreateWorkQueue(
    MemoryArena *arena, u32 objectSize, u32 maxObjects)
{
    WorkQueue result = {};
    result.buffer = AllocateBytes(arena, objectSize *maxObjects);
    result.objectSize = objectSize;
    result.maxObjects = maxObjects;

    return result;
}

inline b32 WorkQueuePush(WorkQueue *queue, void *object, u32 objectSize)
{
    Assert(objectSize == queue->objectSize);
    // TODO: Check for overflow
    // FIXME: This won't work as a circular buffer!
    CopyMemory(
        (u8 *)queue->buffer + objectSize * queue->tail, object, objectSize);
    queue->tail++;
    return true;
}

inline void* WorkQueuePop(WorkQueue *queue, u32 objectSize)
{
    Assert(objectSize == queue->objectSize);
    Assert(queue->head != queue->tail);

    i32 index = AtomicExchangeAdd(&queue->head, 1);
    void *result = (u8 *)queue->buffer + index * objectSize;
    return result;
}
