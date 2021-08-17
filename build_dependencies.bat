@echo off

if not exist build mkdir build
if not exist build\dependencies mkdir build\dependencies

REM Build glfw
COPY dependencies\glfw.json dependencies\glfw\CMakePresets.json
cmake -S .\dependencies\glfw
cmake --build build\dependencies\glfw
cmake --install build\dependencies\glfw
