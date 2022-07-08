/*
@Author: 
@Date: 2021.10
@Desc: 工具函数声明
*/

#ifndef __UTILS_H__
#define __UTILS_H__

#include <unistd.h>
#include <sys/types.h>
#include <syscall.h>

#include "../Config/Configure.h"
#include "../EventLoop/EventLoop.h"

typedef void(*SA_Handler)(int sig);

extern EventLoop* g_base_loop; // 声明

int socket_bind_listen(int port); // 创建socket，绑定地址，调用listen函数
bool set_sock_non_blocking(int fd);
bool set_sock_nodelay(int fd);
bool set_sock_nolinger(int fd);
bool set_sock_reuse_port(int fd);
void register_sigaction(int sig, SA_Handler handler, bool restart=false); // restart 参数：被信号中断的系统调用是否自行重启
void daemon_run(); // 以守护进程方式运行
void start_log(Config config);
void create_connection_pool(Config config);
pid_t gettid();
bool is_dir_exists(const char* dirname);
bool create_dir(const char* dirname);
void setup_server(Config config);
void sigterm_handler(int sig); // SIGTERM的信号处理函数
int create_wakeup_fd();

#endif