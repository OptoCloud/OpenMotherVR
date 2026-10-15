# Sources of src/platform/audio/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/constants.h
    ${CMAKE_CURRENT_LIST_DIR}/endpoint_redirect.cpp
    ${CMAKE_CURRENT_LIST_DIR}/endpoint_redirect.h
)
