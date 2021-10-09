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

# TODO: Probably a better way to do this
if (UNIX)
    set(LINUX_LIBRARIES "pthread")
endif()

# cmake is case-sensitive?!?!?!?!?!?! VULKAN_LIBRARIES doesn't work
target_link_libraries(main glfw ${ASSIMP_LIBRARIES} ${Vulkan_LIBRARIES} ${LINUX_LIBRARIES})
