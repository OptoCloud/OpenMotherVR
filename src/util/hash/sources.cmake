# Sources of src/util/hash/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/crc32.cpp
    ${CMAKE_CURRENT_LIST_DIR}/crc32.h
    ${CMAKE_CURRENT_LIST_DIR}/md5_block.cpp
    ${CMAKE_CURRENT_LIST_DIR}/md5_block.h
)
