@echo off

set BUILD_SHADERS=0
set BUILD_UNIT_TESTS=0
set BUILD_INTEGRATION_TESTS=0
set BUILD_PERF_TESTS=1
set BUILD_LIB=1
set BUILD_EXECUTABLE=1
set BUILD_ASSET_LOADER=0

set CompilerFlags=-DPLATFORM_WINDOWS -MT -F16777216 -nologo -Gm- -GR- -EHa -W4 -WX -wd4702 -wd4305 -wd4127 -wd4201 -wd4189 -wd4100 -wd4996 -wd4505 -FC -Z7 -I..\src
set LinkerFlags=-opt:ref -incremental:no

set unity_include_dir=..\thirdparty\unity
set unity_lib=unity.obj

set vulkan_include_dir="%VULKAN_SDK%\Include"
set vulkan_lib="%VULKAN_SDK%\Lib\vulkan-1.lib"

set glfw_include_dir=..\dependencies\glfw\build\install\include
set glfw_lib=..\dependencies\glfw\build\install\lib\glfw3dll.lib

set assimp_include_dir=..\dependencies\assimp\build\install\include
set assimp_lib=..\dependencies\assimp\build\install\lib\assimp-vc142-mt.lib

if not exist build mkdir build
if not exist build\shaders mkdir build\shaders

pushd build

if %BUILD_SHADERS%==1 (
    glslangvalidator ..\src\shaders\mesh.vert.glsl -V -o shaders\mesh.vert.spv
    glslangvalidator ..\src\shaders\mesh.frag.glsl -V -o shaders\mesh.frag.spv
    glslangvalidator ..\src\shaders\skybox.frag.glsl -V -o shaders\skybox.frag.spv
    glslangvalidator ..\src\shaders\fullscreen_quad.vert.glsl -V -o shaders\fullscreen_quad.vert.spv
    glslangvalidator ..\src\shaders\debug_draw.vert.glsl -V -o shaders\debug_draw.vert.spv
    glslangvalidator ..\src\shaders\debug_draw.frag.glsl -V -o shaders\debug_draw.frag.spv
    glslangvalidator ..\src\shaders\post_processing.frag.glsl -V -o shaders\post_processing.frag.spv
)

if %BUILD_UNIT_TESTS%==1 (
    REM Build unity test framework
    cl %CompilerFlags% -Od -I %unity_include_dir% -c ../thirdparty/unity/unity.c

    REM Build unit tests
    cl ../unit_tests/unit_tests.cpp ^
        %CompilerFlags% ^
        -Od ^
        -I %unity_include_dir% ^
        -link %LinkerFlags% ^
        %unity_lib%

    REM Build SIMD path tracer
    cl ../unit_tests/test_simd_path_tracer.cpp ^
        %CompilerFlags% ^
        -Od ^
        -I %unity_include_dir% ^
        -link %LinkerFlags% ^
        %unity_lib%

    REM Run unit tests
    unit_tests.exe
    test_simd_path_tracer.exe
)

if %BUILD_INTEGRATION_TESTS%==1 (
    REM Build integration tests
    cl ../integration_tests/integration_tests.cpp ^
        %CompilerFlags% ^
        -Od ^
        -I %unity_include_dir% ^
        -I %vulkan_include_dir% ^
        -I %glfw_include_dir% ^
        -I %assimp_include_dir% ^
        -link %LinkerFlags% ^
        %glfw_lib% ^
        %assimp_lib% ^
        %vulkan_lib% ^
        %unity_lib%

    REM Run integration tests
    integration_tests.exe
)

if %BUILD_PERF_TESTS%==1 (
    REM Build performance tests
    cl ../perf_tests/perf_tests.cpp ^
        %CompilerFlags% ^
        -O2 ^
        -I %unity_include_dir% ^
        -I %vulkan_include_dir% ^
        -I %glfw_include_dir% ^
        -I %assimp_include_dir% ^
        -link %LinkerFlags% ^
        %glfw_lib% ^
        %assimp_lib% ^
        %vulkan_lib% ^
        %unity_lib%

    REM Run perf tests
    REM perf_tests.exe
)

if %BUILD_ASSET_LOADER%==1 (
    REM Build miniz library
    REM cl %CompilerFlags% -O2 -I..\thirdparty\tinyexr -c ../thirdparty/tinyexr/miniz.c

    REM Build asset loader library
    cl ../src/asset_loader/asset_loader.cpp ^
        %CompilerFlags% ^
        -O2 ^
        -I ..\thirdparty\tinyexr ^
        -LD ^
        -link -DLL ^
        %LinkerFlags% ^
        miniz.obj
)

if %BUILD_LIB%==1 (
    echo WATING FOR PDB > lock.tmp
    del lib_*.pdb > NUL 2> NUL

    cl ../src/lib.cpp ^
        %CompilerFlags% ^
        -O2 ^
        -I./ ^
        -LD ^
        -link ^
        -PDB:lib_%random%.pdb ^
        -DLL ^
        %LinkerFlags%

    del lock.tmp
)

if %BUILD_EXECUTABLE%==1 (
    REM Build executable
    cl ../src/main.cpp ^
        %CompilerFlags% ^
        -O2 ^
        -I./ ^
        -I %vulkan_include_dir% ^
        -I %glfw_include_dir% ^
        -I %assimp_include_dir% ^
        -link %LinkerFlags% ^
        %glfw_lib% ^
        %assimp_lib% ^
        %vulkan_lib% ^
        asset_loader.lib
)

popd
