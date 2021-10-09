# TODO: Not sure about this min version, copied from other personal project
cmake_minimum_required(VERSION 3.13)

project(vk_cinematic)

#enable_testing()

# TODO: find_package for assimp, glfw3, vulkan, etc
find_package(assimp REQUIRED PATHS dependencies/assimp/build/install)
find_package(glfw3 REQUIRED PATHS dependencies/glfw/build/install)
find_package(Vulkan REQUIRED FATAL_ERROR)

#add_subdirectory(dependencies)

if (WIN32)
    include_directories($ENV{VULKAN_SDK}/Include)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPLATFORM_WINDOWS -MT -F16777216 -nologo -Gm- -GR- -EHa -W4 -WX -wd4305 -wd4127 -wd4201 -wd4189 -wd4100 -wd4996 -wd4505 -FC -Z7")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -opt:ref -incremental:no")
elseif (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPLATFORM_LINUX")
endif()

#include(CTest)

add_subdirectory(thirdparty/unity)
add_subdirectory(src)
add_subdirectory(unit_tests)
add_subdirectory(integration_tests)
