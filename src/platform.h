#pragma once

#include <cstdint>
#include <cstdio>

#include <intrin.h>

#define Assert(X)                              \
    if (!(X)) {                                \
        LogMessage("%s:%d: Assertion failed!\n\t" #X, __FILE__, __LINE__); \
        *(volatile int *)0 = 0;                         \
    }
#define InvalidCodePath() \
    Assert(!"Invalid code path!")

#define OffsetOf(MEMBER, STRUCT) (&(((STRUCT*)0)->MEMBER))
#define ArrayCount(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))
#define Bit(N) (1 << N)

#define Kilobytes(X) ((X) * 1024)
#define Megabytes(X) (Kilobytes(X) * 1024)
#define Gigabytes(X) (Megabytes(X) * 1024LL)

#define U32_MAX 0xFFFFFFFF
#define U8_MAX 0xFF

typedef uint8_t u8;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef u32 b32;

#define internal static
#define global static

#define DebugLogMessage(NAME) void NAME(const char *fmt, ...)
typedef DebugLogMessage(DebugLogMessageFunction);

global DebugLogMessageFunction *LogMessage;

inline void ClearToZero(void *memory, u32 length)
{
    u8 *data = (u8 *)memory;
    for (u32 i = 0; i < length; ++i)
    {
        data[i] = 0;
    }
}

#undef CopyMemory // Clashes with a windows macro
inline void CopyMemory(void *dst, const void *src, u32 length)
{
    for (u32 i = 0; i < length; ++i)
    {
        ((u8 *)dst)[i] = ((u8 *)src)[i];
    }
}

inline u32 SafeTruncateU64ToU32(u64 x)
{
    Assert(x < U32_MAX);
    u32 result = (u32)x;
    return result;
}

inline u8 SafeTruncateU32ToU8(u32 x)
{
    Assert(x < U8_MAX);
    u8 result = (u8)x;
    return result;
}

struct MemoryArena
{
    void *base;
    u64 size;
    u64 capacity;
};

inline void InitializeMemoryArena(
    MemoryArena *arena, void *buffer, u64 capacity)
{
    arena->base = buffer;
    arena->capacity = capacity;
    arena->size = 0;
}

inline void ResetMemoryArena(MemoryArena *arena)
{
    arena->size = 0;
}

inline void *AllocateBytes(MemoryArena *arena, u64 length)
{
    Assert(arena->size + length <= arena->capacity);
    void *result = (u8 *)arena->base + arena->size;
    arena->size += length;

    return result;
}

inline MemoryArena SubAllocateArena(MemoryArena *parent, u64 size)
{
    MemoryArena result = {};
    void *memory = AllocateBytes(parent, size);
    InitializeMemoryArena(&result, memory, size);

    return result;
}

#define AllocateStruct(ARENA, TYPE) \
    (TYPE *)AllocateBytes(ARENA, sizeof(TYPE))

#define AllocateArray(ARENA, TYPE, LENGTH) \
    (TYPE *)AllocateBytes(ARENA, sizeof(TYPE) * LENGTH)

inline bool IsWhitespace(char c) { return (c == ' ' || c == '\t' || c == '\n'); }

inline bool IsNumeric(char c)
{
    return ((c >= '0' && c <= '9') || (c == '-' || c == '+' || c == '.'));
}

