#pragma once

//#define ENABLE_PROFILING
#define ENABLE_VALIDATION_LAYERS

//#define OVERRIDE_CAMERA_POSITION Vec3(0.159051, 0.454406, 0.823399)
//#define OVERRIDE_CAMERA_ROTATION Vec3(-0.0350002, 0.17, 0)

#define ANALYZE_BROAD_PHASE_TREE 1
#define DRAW_BROAD_PHASE_TREE 1

#define DRAW_ENTITY_AABBS 1

#define COMPUTE_SLOW_ENTITY_AABBS 1

#define TILE_WIDTH 256
#define TILE_HEIGHT 256

#define MAX_THREADS 8

#define MAX_BOUNCES 3

#define SAMPLES_PER_PIXEL 4

#define APPLICATION_MEMORY_LIMIT Megabytes(512)

// Use Moller-Trumbore algorithm (needed for proper UVs but also seems much faster)
#define USE_MT_RAY_TRIANGLE_INTERSECT 1

//#define MIN_VOLUME

#define RAY_TRACER_WIDTH (1024 / 1)
#define RAY_TRACER_HEIGHT (768 / 1)

// Maximum radiance value to clamp to before tone mapping (this is used to
// reduce fireflies)
#define RADIANCE_CLAMP 10

#define DRAW_DIFFUSE_SAMPLE_PATTERN 1
