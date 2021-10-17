# vk_cinematic

![Build and Test](https://github.com/deadVertex/vk_cinematic/actions/workflows/cmake.yml/badge.svg)

A sandbox project for playing around with computer graphics, experimenting with
rasterization based methods and comparing them against CPU/GPU ray tracing.

![screenshot](/docs/2021-10-17.png)
![screenshot](/docs/2021-08-26.png)

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

Next you will need to build the dependencies for the project.
**Only for Windows**
```
build_dependencies.bat
```

You can then build the project with the standard commands.
```
cmake -B build
cmake --build build
```

To run the unit tests
```
build/tests/test_app
```

Before you can run the project you will need to manually copy the DLL files for
the dependencies to the same directory as the main.exe binary that was built.

Copy /dependencies/glfw/build/install/bin/glfw3.dll to /build/src/Debug
Copy /dependencies/assimp/build/install/bin/*.dll to /build/src/Debug

You can then navigate into the build directory and run the executable.
```
cd build
src/Debug/main.exe
```

## Minimalist Build System
Its also possible to use the old build.bat Handmade Hero style build system.
Although you will still need to use cmake to build the dependencies like in the
main instructions and copy the DLL files to the build directory.

After that initial setup you can then just build the application by running
```
build.bat
```
