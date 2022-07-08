aux_source_directory(. SRC_UTILS)

add_library(Utils ${SRC_UTILS})

target_link_libraries(Utils Logger Configure ConnectionPool EventLoop)