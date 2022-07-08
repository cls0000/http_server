aux_source_directory(. SRC_Servlet)

add_library(Servlet ${SRC_Servlet})

target_link_libraries(Servlet Object mysqlclient) 