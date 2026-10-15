# Sources of src/util/sync/; each directory lists its own files.
target_sources(omvr_util PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/change_latch.h
    ${CMAKE_CURRENT_LIST_DIR}/frame_period.h
    ${CMAKE_CURRENT_LIST_DIR}/once_latch.h
    ${CMAKE_CURRENT_LIST_DIR}/seqlock.h
)
