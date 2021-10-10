@echo off

set BUILD_SHADERS=0
set BUILD_UNIT_TESTS=0
set BUILD_INTEGRATION_TESTS=0
set BUILD_EXECUTABLE=1

set CompilerFlags=-DPLATFORM_WINDOWS -MT -F16777216 -nologo -Gm- -GR- -EHa -W4 -WX -wd4702 -wd4305 -wd4127 -wd4201 -wd4189 -wd4100 -wd4996 -wd4505 -FC -Z7 -I..\src
set LinkerFlags=-opt:ref -incremental:no

if not exist build mkdir build
if not exist build\shaders mkdir build\shaders

pushd build

if %BUILD_SHADERS%==1 (
    glslangvalidator ..\src\shaders\mesh.vert.glsl -V -o shaders\mesh.vert.spv
    glslangvalidator ..\src\shaders\mesh.frag.glsl -V -o shaders\mesh.frag.spv
    glslangvalidator ..\src\shaders\fullscreen_quad.vert.glsl -V -o shaders\fullscreen_quad.vert.spv
    glslangvalidator ..\src\shaders\fullscreen_quad.frag.glsl -V -o shaders\fullscreen_quad.frag.spv
    glslangvalidator ..\src\shaders\debug_draw.vert.glsl -V -o shaders\debug_draw.vert.spv
    glslangvalidator ..\src\shaders\debug_draw.frag.glsl -V -o shaders\debug_draw.frag.spv
)

if %BUILD_UNIT_TESTS%==1 (
    REM Build unity test framework
    cl %CompilerFlags% -Od -I..\thirdparty\unity -c ../thirdparty/unity/unity.c

    REM Build unit tests
    cl %CompilerFlags% -Od -I..\thirdparty\unity ../unit_tests/unit_tests.cpp -link %LinkerFlags% unity.obj
    unit_tests.exe
)

if %BUILD_INTEGRATION_TESTS%==1 (
    REM Build integration tests
    cl %CompilerFlags% -Od -I..\thirdparty\unity -I..\dependencies\assimp\build\install\include ../integration_tests/integration_tests.cpp -link %LinkerFlags% ..\dependencies\assimp\build\install\lib\assimp-vc142-mt.lib unity.obj
    integration_tests.exe
)

if %BUILD_EXECUTABLE%==1 (
    REM Build executable
    cl %CompilerFlags% -O2 -I./ -I "%VULKAN_SDK%\Include" -I..\dependencies\glfw\build\install\include -I..\dependencies\assimp\build\install\include ../src/main.cpp -link %LinkerFlags% ..\dependencies\glfw\build\install\lib\glfw3dll.lib ..\dependencies\assimp\build\install\lib\assimp-vc142-mt.lib "%VULKAN_SDK%\Lib\vulkan-1.lib"
)

popd
