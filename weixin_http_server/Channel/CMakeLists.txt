aux_source_directory(. SRC_CHANNEL)

add_library(Channel ${SRC_CHANNEL})

target_link_libraries(Channel HttpConn Logger)