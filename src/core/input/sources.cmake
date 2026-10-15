# Sources of src/core/input/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/action_tiers.cpp
    ${CMAKE_CURRENT_LIST_DIR}/action_tiers.h
    ${CMAKE_CURRENT_LIST_DIR}/control_mapper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/control_mapper.h
    ${CMAKE_CURRENT_LIST_DIR}/control_scheme.cpp
    ${CMAKE_CURRENT_LIST_DIR}/control_scheme.h
    ${CMAKE_CURRENT_LIST_DIR}/controller_mapping.cpp
    ${CMAKE_CURRENT_LIST_DIR}/controller_mapping.h
    ${CMAKE_CURRENT_LIST_DIR}/glyph_ids.h
    ${CMAKE_CURRENT_LIST_DIR}/glyphs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/glyphs.h
    ${CMAKE_CURRENT_LIST_DIR}/interaction_state.cpp
    ${CMAKE_CURRENT_LIST_DIR}/interaction_state.h
    ${CMAKE_CURRENT_LIST_DIR}/remap.h
    ${CMAKE_CURRENT_LIST_DIR}/touchpad.cpp
    ${CMAKE_CURRENT_LIST_DIR}/touchpad.h
    ${CMAKE_CURRENT_LIST_DIR}/tuning.h
    ${CMAKE_CURRENT_LIST_DIR}/types.h
    ${CMAKE_CURRENT_LIST_DIR}/virtual_pad.cpp
    ${CMAKE_CURRENT_LIST_DIR}/virtual_pad.h
)
