aux_source_directory(. SRC_EVENTLOOP)

add_library(EventLoop ${SRC_EVENTLOOP})

target_link_libraries(EventLoop Channel Poller Timer Logger Utils EventLoopThreadPool)