@echo off

if not exist build mkdir build
if not exist build\dependencies mkdir build\dependencies

REM CMake bug? configurePreset field in buildPreset is being ignored
REM Build glfw
COPY dependencies\glfw.json dependencies\glfw\CMakePresets.json
pushd dependencies\glfw
cmake --preset=default .
cmake --build build --config=Release 
cmake --install build
popd

REM Build assimp
COPY dependencies\assimp.json dependencies\assimp\CMakePresets.json
pushd dependencies\assimp
cmake --preset=default .
cmake --build build --config=Release 
cmake --install build
popd
