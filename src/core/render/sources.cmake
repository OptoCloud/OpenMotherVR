# OpenMotherVR-Provenance: clean
# Sources of src/core/render/; each directory lists its own files.
target_sources(omvr_core PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/alpha_light.cpp
    ${CMAKE_CURRENT_LIST_DIR}/alpha_light.h
    ${CMAKE_CURRENT_LIST_DIR}/dither_fix.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dither_fix.h
    ${CMAKE_CURRENT_LIST_DIR}/dxbc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dxbc.h
    ${CMAKE_CURRENT_LIST_DIR}/dxbc_tokens.h
    ${CMAKE_CURRENT_LIST_DIR}/shader_fix_stats.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shader_fix_stats.h
    ${CMAKE_CURRENT_LIST_DIR}/tuning.h
)
