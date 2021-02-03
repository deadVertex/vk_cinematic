@echo off

set CompilerFlags=-Od -MT -F16777216 -nologo -Gm- -GR- -EHa -W4 -WX -wd4305 -wd4127 -wd4201 -wd4189 -wd4100 -wd4996 -wd4505 -FC -Z7 -I..\src
set LinkerFlags=-opt:ref -incremental:no

if not exist build mkdir build

pushd build

REM Build executable
cl %CompilerFlags% -I./ -I "%VULKAN_SDK%\Include" -I..\windows-dependencies\glfw3\include ../src/main.cpp -link %LinkerFlags% ..\windows-dependencies\glfw3\lib-vc2017\glfw3dll.lib "%VULKAN_SDK%\Lib\vulkan-1.lib"
popd
