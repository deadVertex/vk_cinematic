#pragma once

//#define ENABLE_PROFILING
#define ENABLE_VALIDATION_LAYERS

//#define OVERRIDE_CAMERA_POSITION Vec3(0.159051, 0.454406, 0.823399)
//#define OVERRIDE_CAMERA_ROTATION Vec3(-0.0350002, 0.17, 0)

#define ANALYZE_BROAD_PHASE_TREE 1
#define DRAW_BROAD_PHASE_TREE 1

#define DRAW_ENTITY_AABBS 1

#define COMPUTE_SLOW_ENTITY_AABBS 1

#define TILE_WIDTH 64
#define TILE_HEIGHT 64

#define MAX_THREADS 8

#define MAX_BOUNCES 3

#define SAMPLES_PER_PIXEL 128

#define APPLICATION_MEMORY_LIMIT Megabytes(512)

// Use Moller-Trumbore algorithm (needed for proper UVs but also seems much faster)
#define USE_MT_RAY_TRIANGLE_INTERSECT 1

//#define MIN_VOLUME
