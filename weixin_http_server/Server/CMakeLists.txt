aux_source_directory(. SRC_SERVER)

add_library(Server ${SRC_SERVER})

target_link_libraries(Server EventLoop Channel EventLoopThreadPool Configure Utils Logger HttpConn)