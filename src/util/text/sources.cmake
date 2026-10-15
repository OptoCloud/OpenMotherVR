# OpenMotherVR-Provenance: clean
# Sources of src/util/text/; each directory lists its own files.
target_sources(omvr_util PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ascii.h
    ${CMAKE_CURRENT_LIST_DIR}/command_line_split.cpp
    ${CMAKE_CURRENT_LIST_DIR}/command_line_split.h
    ${CMAKE_CURRENT_LIST_DIR}/fixed_format.h
    ${CMAKE_CURRENT_LIST_DIR}/number_parse.cpp
    ${CMAKE_CURRENT_LIST_DIR}/number_parse.h
)
