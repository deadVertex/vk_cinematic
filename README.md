# vk_cinematic
A sandbox project for playing around with computer graphics, experimenting with
rasterization based methods and comparing them against CPU/GPU ray tracing.

## Dependencies

### Build Tools
- [CMake](https://cmake.org/)

### Libraries
- [GLFW3](https://github.com/glfw/glfw)
- [Assimp](https://github.com/assimp/assimp)
- [Unity Test Project (source files included)](https://github.com/ThrowTheSwitch/Unity)
- [Vulkan SDK](https://vulkan.lunarg.com/)

## Instructions
First clone the repository and its submodules for GLFW3 and Assimp.
```
git clone git@github.com:deadVertex/vk_cinematic.git
git submodule update --init
```

The source code for the project can then be built with the standard cmake
commands.
```
cmake -B build
cmake --build build
```

Once built navigate into the build directory and run the executable.
```
cd build
src/Debug/main.exe
```
