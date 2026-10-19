# OpenMotherVR-Provenance: clean
# Sources of src/game/ui/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/console.cpp
    ${CMAKE_CURRENT_LIST_DIR}/console.h
    ${CMAKE_CURRENT_LIST_DIR}/hud_objects.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hud_objects.h
    ${CMAKE_CURRENT_LIST_DIR}/script_vars.cpp
    ${CMAKE_CURRENT_LIST_DIR}/script_vars.h
)
