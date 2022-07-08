aux_source_directory(. SRC_TIMER)

add_library(Timer ${SRC_TIMER})

target_link_libraries(Timer Logger)