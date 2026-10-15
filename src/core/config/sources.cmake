# Sources of src/core/config/; each directory lists its own files.
target_sources(omvr_core PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/launch_options.cpp
    ${CMAKE_CURRENT_LIST_DIR}/launch_options.h
    ${CMAKE_CURRENT_LIST_DIR}/settings.h
    ${CMAKE_CURRENT_LIST_DIR}/settings_json.cpp
    ${CMAKE_CURRENT_LIST_DIR}/settings_json.h
    ${CMAKE_CURRENT_LIST_DIR}/settings_store.cpp
    ${CMAKE_CURRENT_LIST_DIR}/settings_store.h
    ${CMAKE_CURRENT_LIST_DIR}/settings_table.cpp
    ${CMAKE_CURRENT_LIST_DIR}/settings_table.h
    ${CMAKE_CURRENT_LIST_DIR}/tuning.h
)
