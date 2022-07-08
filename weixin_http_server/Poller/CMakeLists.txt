aux_source_directory(. SRC_POLLER)

add_library(Poller ${SRC_POLLER})

target_link_libraries(Poller Channel HttpConn Logger)