include_directories(../thirdparty/unity ../src)

include_directories(${ASSIMP_INCLUDE_DIRS})
link_directories(${ASSIMP_LIBRARY_DIRS})

add_executable(test_app test.cpp)

target_link_libraries(test_app unity ${ASSIMP_LIBRARIES})

#add_test(NAME unit_tests COMMAND test_app WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/test)
