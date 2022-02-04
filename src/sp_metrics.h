#pragma once

// Per thread performance metrics
struct sp_Metrics
{
    u64 cyclesElapsed; // Cycles spent in sp_PathTraceTile call
    u64 pathsTraced; // Number of light paths traced
    u64 raysTraced; // Number of rays traced through the scene
    u64 rayHitCount; // Number of rays which hit something in the scene
    u64 rayMissCount; // Number of rays which hit nothing
};
