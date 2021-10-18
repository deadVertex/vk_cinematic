add_library(miniz STATIC miniz.c)
set_target_properties(miniz PROPERTIES LINKER_LANGUAGE C)
set_property(TARGET miniz PROPERTY POSITION_INDEPENDENT_CODE ON)
