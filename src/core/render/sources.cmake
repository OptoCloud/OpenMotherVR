# OpenMotherVR-Provenance: clean
# Sources of src/core/render/; each directory lists its own files.
target_sources(omvr_core PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/alpha_light.cpp
    ${CMAKE_CURRENT_LIST_DIR}/alpha_light.h
    ${CMAKE_CURRENT_LIST_DIR}/constant_smoke.cpp
    ${CMAKE_CURRENT_LIST_DIR}/constant_smoke.h
    ${CMAKE_CURRENT_LIST_DIR}/dither_fix.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dither_fix.h
    ${CMAKE_CURRENT_LIST_DIR}/dxbc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dxbc.h
    ${CMAKE_CURRENT_LIST_DIR}/dxbc_tokens.h
    ${CMAKE_CURRENT_LIST_DIR}/engine_settings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/engine_settings.h
    ${CMAKE_CURRENT_LIST_DIR}/pixel_shader_fixes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pixel_shader_fixes.h
    ${CMAKE_CURRENT_LIST_DIR}/post_fx.cpp
    ${CMAKE_CURRENT_LIST_DIR}/post_fx.h
    ${CMAKE_CURRENT_LIST_DIR}/shader_fix_stats.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shader_fix_stats.h
    ${CMAKE_CURRENT_LIST_DIR}/tuning.h
    ${CMAKE_CURRENT_LIST_DIR}/types.h
    ${CMAKE_CURRENT_LIST_DIR}/upright_sprites.cpp
    ${CMAKE_CURRENT_LIST_DIR}/upright_sprites.h
)
