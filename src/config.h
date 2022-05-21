#pragma once

//#define ENABLE_PROFILING
#define ENABLE_VALIDATION_LAYERS

//#define OVERRIDE_CAMERA_POSITION Vec3(0, 0, 10)
//#define OVERRIDE_CAMERA_ROTATION Vec3(0, 0, 0)

#define ANALYZE_BROAD_PHASE_TREE 1

#define DRAW_ENTITY_AABBS 1

#define COMPUTE_SLOW_ENTITY_AABBS 1

#define TILE_WIDTH 64
#define TILE_HEIGHT 64

// TODO: Shouldn't need this
#define MAX_TILES 4096

#define MAX_THREADS 16

#define MAX_BOUNCES 3

#define SAMPLES_PER_PIXEL 64

#define APPLICATION_MEMORY_LIMIT Megabytes(512)

// Use Moller-Trumbore algorithm (needed for proper UVs but also seems much faster)
#define USE_MT_RAY_TRIANGLE_INTERSECT 1

#define RAY_TRACER_WIDTH (1024 / 1)
#define RAY_TRACER_HEIGHT (768 / 1)

// Maximum radiance value to clamp to before tone mapping (this is used to
// reduce fireflies)
#define RADIANCE_CLAMP 2

//#define DRAW_DIFFUSE_SAMPLE_PATTERN 1

//#define SHOW_SCENE_NORMALS 1

#define LIVE_CODE_RELOADING_TEST_ENABLED 1

#define IRRADIANCE_CUBEMAP_USE_UNIFORM_SAMPLING 1

#define USE_BVH_SIMD_RAY_INTERSECT_AABB 1

#define SP_DEBUG_BROADPHASE_INTERSECTION_COUNT 0
#define SP_DEBUG_SURFACE_NORMAL 0
#define SP_DEBUG_MIDPHASE_INTERSECTION_COUNT 0

#define CAMERA_FOV 50.0f
#define CAMERA_NEAR_CLIP 0.01f

#define FEAT_ENABLE_GPU_PATH_TRACING 0
