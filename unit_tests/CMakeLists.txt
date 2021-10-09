include_directories(../thirdparty/unity ../src)

add_executable(unit_tests unit_tests.cpp)

target_link_libraries(unit_tests unity)

#add_test(NAME unit_tests COMMAND test_app WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/test)
