@echo off

set CompilerFlags=-MT -F16777216 -nologo -Gm- -GR- -EHa -W4 -WX -wd4305 -wd4127 -wd4201 -wd4189 -wd4100 -wd4996 -wd4505 -FC -Z7 -I..\src
set LinkerFlags=-opt:ref -incremental:no

if not exist build mkdir build
if not exist build\shaders mkdir build\shaders

pushd build

glslangvalidator ..\src\shaders\mesh.vert.glsl -V -o shaders\mesh.vert.spv
glslangvalidator ..\src\shaders\mesh.frag.glsl -V -o shaders\mesh.frag.spv
glslangvalidator ..\src\shaders\fullscreen_quad.vert.glsl -V -o shaders\fullscreen_quad.vert.spv
glslangvalidator ..\src\shaders\fullscreen_quad.frag.glsl -V -o shaders\fullscreen_quad.frag.spv
glslangvalidator ..\src\shaders\debug_draw.vert.glsl -V -o shaders\debug_draw.vert.spv
glslangvalidator ..\src\shaders\debug_draw.frag.glsl -V -o shaders\debug_draw.frag.spv

REM Build unit tests
REM cl %CompilerFlags% -Od -I..\thirdparty\unity -c ../thirdparty/unity/unity.c
REM cl %CompilerFlags% -Od -I..\thirdparty\unity -I..\windows-dependencies\assimp\include ../src/test.cpp -link %LinkerFlags% ..\windows-dependencies\assimp\lib-vc2017\assimp-vc141-mt.lib unity.obj
REM test.exe

REM Build executable
cl %CompilerFlags% -O2 -I./ -I "%VULKAN_SDK%\Include" -I..\dependencies\glfw\build\install\include -I..\dependencies\assimp\build\install\include ../src/main.cpp -link %LinkerFlags% ..\dependencies\glfw\build\install\lib\glfw3dll.lib ..\dependencies\assimp\build\install\lib\assimp-vc142-mt.lib "%VULKAN_SDK%\Lib\vulkan-1.lib"
popd
