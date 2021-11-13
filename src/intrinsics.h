#pragma once

#include "platform.h"

inline i32 AtomicExchangeAdd(volatile i32 *addend, i32 value)
{
#ifdef PLATFORM_WINDOWS
    i32 result = _InterlockedExchangeAdd((volatile long *)addend, value);
#elif defined(PLATFORM_LINUX)
    // TODO: Probably not the right memory model - https://gcc.gnu.org/wiki/Atomic/GCCMM/AtomicSync
    i32 result = __atomic_fetch_add(addend, value, __ATOMIC_SEQ_CST);
#else
#error "UNSUPPORTED PLATFORM"
#endif
    return result;
}

inline i64 AtomicExchangeAdd64(volatile i64 *addend, i64 value)
{
#ifdef PLATFORM_WINDOWS
    i64 result = _InterlockedExchangeAdd64(addend, value);
#elif defined(PLATFORM_LINUX)
    // TODO: Probably not the right memory model - https://gcc.gnu.org/wiki/Atomic/GCCMM/AtomicSync
    i32 result = __atomic_fetch_add(addend, value, __ATOMIC_SEQ_CST);
#else
#error "UNSUPPORTED PLATFORM"
#endif
    return result;
}

inline void AtomicIncrement64(i64 *addend)
{
#ifdef PLATFORM_WINDOWS
    _InterlockedIncrement64(addend);
#elif defined(PLATFORM_LINUX)
    // TODO: Probably not the right memory model - https://gcc.gnu.org/wiki/Atomic/GCCMM/AtomicSync
    __atomic_add_fetch(addend, 1, __ATOMIC_SEQ_CST);
#else
#error "UNSUPPORTED PLATFORM"
#endif
}
