cmake_minimum_required(VERSION 3.12)
project (VictorServer)
aux_source_directory(. SRC)

# 添加头文件目录
include_directories(./Channel  
./Config 
./ConnectionPool 
./EventLoop 
./EventLoopThreadPool 
./HttpConn 
./Log 
./Poller 
./Server 
./Timer 
./Utils
./Object
./Servlet
./configor
)

# 添加子目录
add_subdirectory(Channel)
add_subdirectory(Config)
add_subdirectory(ConnectionPool)
add_subdirectory(EventLoop)
add_subdirectory(EventLoopThreadPool)
add_subdirectory(HttpConn)
add_subdirectory(Log)
add_subdirectory(Poller)
add_subdirectory(Server)
add_subdirectory(Servlet)
add_subdirectory(Timer)
add_subdirectory(Utils)
add_subdirectory(Object)

# 指定生成的目标
add_executable(VictorServer ${SRC})

# 添加链接库
target_link_libraries(VictorServer Logger Server Configure Utils EventLoop)