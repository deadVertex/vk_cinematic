#pragma once

enum
{
    // Cycles spent in sp_PathTraceTile call
    sp_Metric_CyclesElapsed,

    // Number of light paths traced
    sp_Metric_PathsTraced,

    // Number of rays traced through the scene
    sp_Metric_RaysTraced,

    // Number of rays which hit something in the scene
    sp_Metric_RayHitCount,

    // Number of rays which hit nothing
    sp_Metric_RayMissCount,

    SP_MAX_METRICS,
};

// Per thread performance metrics
struct sp_Metrics
{
    u64 values[SP_MAX_METRICS];
};
