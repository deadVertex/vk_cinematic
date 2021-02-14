@echo off

set CompilerFlags=-Od -MT -F16777216 -nologo -Gm- -GR- -EHa -W4 -WX -wd4305 -wd4127 -wd4201 -wd4189 -wd4100 -wd4996 -wd4505 -FC -Z7 -I..\src
set LinkerFlags=-opt:ref -incremental:no

if not exist build mkdir build

pushd build

glslangvalidator ..\src\shaders\mesh.vert.glsl -V -o mesh.vert.spv
glslangvalidator ..\src\shaders\mesh.frag.glsl -V -o mesh.frag.spv
glslangvalidator ..\src\shaders\fullscreen_quad.vert.glsl -V -o fullscreen_quad.vert.spv
glslangvalidator ..\src\shaders\fullscreen_quad.frag.glsl -V -o fullscreen_quad.frag.spv

REM Build executable
cl %CompilerFlags% -I./ -I "%VULKAN_SDK%\Include" -I..\windows-dependencies\glfw3\include -I..\windows-dependencies\assimp\include ../src/main.cpp -link %LinkerFlags% ..\windows-dependencies\glfw3\lib\glfw3dll.lib ..\windows-dependencies\assimp\lib-vc2017\assimp-vc141-mt.lib "%VULKAN_SDK%\Lib\vulkan-1.lib"
popd
