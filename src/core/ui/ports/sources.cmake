# Sources of src/core/ui/ports/; each directory lists its own files.
target_sources(omvr_core PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/archive_files.h
    ${CMAKE_CURRENT_LIST_DIR}/dialogs.h
    ${CMAKE_CURRENT_LIST_DIR}/game_signals.h
    ${CMAKE_CURRENT_LIST_DIR}/runtime_state.h
    ${CMAKE_CURRENT_LIST_DIR}/script_variables.h
)
