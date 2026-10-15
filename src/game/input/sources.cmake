# OpenMotherVR-Provenance: clean
# Sources of src/game/input/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/input_devices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/input_devices.h
    ${CMAKE_CURRENT_LIST_DIR}/input_manager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/input_manager.h
)
