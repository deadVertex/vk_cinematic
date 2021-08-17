add_subdirectory(shaders)

# TODO: Use target_include and target_link directories
include_directories(${ASSIMP_INCLUDE_DIRS})
link_directories(${ASSIMP_LIBRARY_DIRS})

# Using the concept of a 'unity' build in C++ which is where you #include your
# .cpp files into our main.cpp file to create one giant translation unit which
# compiles much faster than having many separate files. See
# https://en.wikipedia.org/wiki/Unity_build
# TODO: Apparently CMake supports unity builds?!?!?
add_executable(main main.cpp)

target_link_libraries(main glfw ${ASSIMP_LIBRARIES} "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib")
