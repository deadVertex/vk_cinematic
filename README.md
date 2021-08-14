# vk_cinematic
A sandbox project for playing around with computer graphics, experimenting with
rasterization based methods and comparing them against CPU/GPU ray tracing.

## Dependencies

### Build Tools
- [CMake](https://cmake.org/)

### Libraries
- [GLFW3 (prebuilt binaries included for windows)](https://github.com/glfw/glfw)
- [Assimp (prebuilt binaries included for windows)](https://github.com/assimp/assimp)
- [Unity Test Project (source files included)](https://github.com/ThrowTheSwitch/Unity)
- [Vulkan SDK](https://vulkan.lunarg.com/)

## How to build
After cloning the source code the project can be built with the standard cmake
commands.
```
cmake -B build
cmake --build build
```
