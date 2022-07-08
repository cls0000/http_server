aux_source_directory(. SRC_CONN_POOL)

add_library(ConnectionPool ${SRC_CONN_POOL})

target_link_libraries(ConnectionPool Logger mysqlclient pthread)