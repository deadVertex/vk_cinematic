include_directories(../../thirdparty/tinyexr)

add_library(asset_loader SHARED asset_loader.cpp)
set_property(TARGET asset_loader PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(asset_loader miniz)
