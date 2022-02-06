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

    // Total number of cycles spent in sp_RayIntersectScene
    sp_Metric_CyclesElapsed_RayIntersectScene,

    // Total number of cycles spent in broadphase ray intersection tests
    sp_Metric_CyclesElapsed_RayIntersectBroadphase,

    // Total number of cycles spent in sp_RayIntersectMesh
    sp_Metric_CyclesElapsed_RayIntersectMesh,

    // Total number of cycles spent in bvh_IntersectRay for mesh midphase
    sp_Metric_CyclesElapsed_RayIntersectMeshMidphase,

    // Total number of cycles spent in RayIntersectTriangle
    sp_Metric_CyclesElapsed_RayIntersectTriangle,

    // Number of Ray vs AABB intersection tests performed for mesh midphase test
    sp_Metric_RayIntersectMesh_MidphaseAabbTestCount,

    // Number of Ray vs Mesh tests performed
    sp_Metric_RayIntersectMesh_TestsPerformed,

    SP_MAX_METRICS,
};

// Per thread performance metrics
struct sp_Metrics
{
    u64 values[SP_MAX_METRICS];
};
