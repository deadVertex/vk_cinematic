include_directories(../thirdparty/unity ../src)
add_executable(test_app test.cpp)

target_link_libraries(test_app unity)

#add_test(NAME unit_tests COMMAND test_app WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/test)
