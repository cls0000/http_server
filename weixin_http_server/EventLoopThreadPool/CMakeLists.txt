aux_source_directory(. SRC_THREAD)

add_library(EventLoopThreadPool ${SRC_THREAD})

target_link_libraries(EventLoopThreadPool EventLoop Utils Logger pthread)