include_directories(../thirdparty/unity ../src)

add_executable(unit_tests unit_tests.cpp)

target_link_libraries(unit_tests unity)

#add_test(NAME unit_tests COMMAND test_app WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/test)

add_executable(test_simd_path_tracer test_simd_path_tracer.cpp)

target_link_libraries(test_simd_path_tracer unity)

add_executable(test_bvh test_bvh.cpp)

target_link_libraries(test_bvh unity)
