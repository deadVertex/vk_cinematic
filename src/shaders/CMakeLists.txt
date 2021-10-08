# TODO: Find Vulkan

# Based on https://stackoverflow.com/a/68457439
file(GLOB SHADERS *.glsl)

set (SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

foreach(source IN LISTS SHADERS)
    get_filename_component(FILENAME ${source} NAME)

    # Get file name without .glsl extension
    get_filename_component(FILENAME_WLE ${source} NAME_WLE)

    set(shader_binary "${SHADER_BINARY_DIR}/${FILENAME_WLE}.spv")
    add_custom_command(
        COMMAND
            glslangvalidator "${source}" -V -o "${shader_binary}"
        OUTPUT "${shader_binary}"
        DEPENDS "${source}"
        COMMENT "Compiling ${FILENAME}"
    )
    list(APPEND SPV_SHADERS "${shader_binary}")
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})
