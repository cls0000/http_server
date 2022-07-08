aux_source_directory(. SRC_LOG)

add_library(Logger ${SRC_LOG})

target_link_libraries(Logger Utils pthread EventLoopThreadPool)