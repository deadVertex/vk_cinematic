include_directories(../../thirdparty/tinyexr)

add_library(asset_loader SHARED asset_loader.cpp)

target_link_libraries(asset_loader miniz)
