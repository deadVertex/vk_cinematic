#pragma once

#ifdef ENABLE_PROFILING
#define PROFILE_BEGIN_SCOPE(IDENTIFIER) \
{ \
    u32 __idx = g_Profiler.count++; \
    g_Profiler.samples[__idx].identifier = IDENTIFIER; \
    g_Profiler.samples[__idx].type = ProfilerSampleType_Begin; \
    g_Profiler.samples[__idx].timestamp = __rdtsc(); \
}

#define PROFILE_END_SCOPE(IDENTIFIER) \
{ \
    u64 __timestamp = __rdtsc(); \
    u32 __idx = g_Profiler.count++; \
    g_Profiler.samples[__idx].identifier = IDENTIFIER; \
    g_Profiler.samples[__idx].type = ProfilerSampleType_End; \
    g_Profiler.samples[__idx].timestamp = __timestamp; \
}

#define PROFILE_BEGIN_FUNCTION() \
    PROFILE_BEGIN_SCOPE(__FUNCTION__)

#define PROFILE_END_FUNCTION() \
    PROFILE_END_SCOPE(__FUNCTION__)

#define PROFILE_SCOPE(IDENTIFIER) \
    ProfileScope __profileScope##IDENTIFIER(#IDENTIFIER)

#define PROFILE_FUNCTION_SCOPE() \
    ProfileScope __profileScope##__FUNCTION__(__FUNCTION__)
#else
// NULL MACROS
#define PROFILE_BEGIN_SCOPE(IDENTIFIER)
#define PROFILE_END_SCOPE(IDENTIFIER)
#define PROFILE_BEGIN_FUNCTION()
#define PROFILE_END_FUNCTION()
#define PROFILE_SCOPE(IDENTIFIER)
#define PROFILE_FUNCTION_SCOPE()
#endif


struct ProfilerSample
{
    const char *identifier;
    u64 timestamp;
    u32 type;
};

enum
{
    ProfilerSampleType_Begin,
    ProfilerSampleType_End,
};

#define PROFILER_SAMPLE_BUFFER_SIZE Megabytes(512)

struct Profiler
{
    ProfilerSample *samples;
    u32 count;
};

global Profiler g_Profiler;

struct ProfileScope
{
    const char *identifier;
    ProfileScope(const char *_identifier)
    {
        identifier = _identifier;
        PROFILE_BEGIN_SCOPE(identifier);
    }

    ~ProfileScope()
    {
        PROFILE_END_SCOPE(identifier);
    }
};

struct ProfilerResult
{
    const char *identifier;
    u64 totalCyclesElapsed;
    u64 callCount;
    u64 averageCyclesPerCall;
};

#define MAX_PROFILER_RESULTS 1024
struct ProfilerResults
{
    ProfilerResult results[MAX_PROFILER_RESULTS];
    u32 count;
};

inline ProfilerResult *GetResult(ProfilerResults *results, const char *identifier)
{
    ProfilerResult *result = NULL;
    for (u32 i = 0; i < results->count; ++i)
    {
        if (results->results[i].identifier == identifier)
        {
            result = results->results + i;
        }
    }

    if (result == NULL)
    {
        Assert(results->count < MAX_PROFILER_RESULTS);
        result = results->results + results->count++;
        result->identifier = identifier;
    }

    return result;
}

internal void Profiler_ProcessResults(Profiler *profiler, ProfilerResults *results)
{
    results->count = 0;

    for (u32 idx = 0; idx < profiler->count; ++idx)
    {
        ProfilerSample startingSample = profiler->samples[idx];
        if (startingSample.type == ProfilerSampleType_Begin)
        {
            for (u32 endIdx = idx + 1; endIdx < profiler->count; ++endIdx)
            {
                ProfilerSample endingSample = profiler->samples[endIdx];
                if (endingSample.type == ProfilerSampleType_End)
                {
                    if (startingSample.identifier == endingSample.identifier)
                    {
                        u64 cyclesElapsed =
                            endingSample.timestamp - startingSample.timestamp;
                        ProfilerResult *result =
                            GetResult(results, startingSample.identifier);
                        result->totalCyclesElapsed += cyclesElapsed;
                        result->callCount++;
                        break;
                    }
                }
            }
        }
    }

    // TODO: Calculate average cycles per call count for each result
    profiler->count = 0;
}

internal void Profiler_PrintResults(ProfilerResults *results)
{
    for (u32 idx = 0; idx < results->count; ++idx)
    {
        ProfilerResult *result = results->results + idx;
        f64 cyclesPerCall =
            (f64)result->totalCyclesElapsed / (f64)result->callCount;
        LogMessage("%s: %llu %llu %llu", result->identifier,
            result->totalCyclesElapsed, result->callCount, (u64)cyclesPerCall);
    }
}
