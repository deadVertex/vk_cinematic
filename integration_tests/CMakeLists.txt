include_directories(../thirdparty/unity ../src)

include_directories(${ASSIMP_INCLUDE_DIRS})
link_directories(${ASSIMP_LIBRARY_DIRS})

add_executable(integration_tests integration_tests.cpp)

target_link_libraries(integration_tests glfw unity ${ASSIMP_LIBRARIES} ${Vulkan_LIBRARIES})
