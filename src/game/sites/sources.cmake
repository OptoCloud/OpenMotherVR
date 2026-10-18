# OpenMotherVR-Provenance: clean
# Sources of src/game/sites/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/controls.cpp
    ${CMAKE_CURRENT_LIST_DIR}/controls.h
    ${CMAKE_CURRENT_LIST_DIR}/engine_settings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/engine_settings.h
    ${CMAKE_CURRENT_LIST_DIR}/game_behaviour.cpp
    ${CMAKE_CURRENT_LIST_DIR}/game_behaviour.h
    ${CMAKE_CURRENT_LIST_DIR}/post_fx_effects.cpp
    ${CMAKE_CURRENT_LIST_DIR}/post_fx_effects.h
    ${CMAKE_CURRENT_LIST_DIR}/render_fixes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/render_fixes.h
    ${CMAKE_CURRENT_LIST_DIR}/stereo_path.cpp
    ${CMAKE_CURRENT_LIST_DIR}/stereo_path.h
    ${CMAKE_CURRENT_LIST_DIR}/ui_set.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_set.h
)
