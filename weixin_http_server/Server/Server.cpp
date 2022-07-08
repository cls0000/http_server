/*
@Author: 
@Date: 2021.10
@Desc: 服务器类，总体
*/

#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "Server.h"
#include "../Utils/Utils.h"
#include "../Log/Logger.h"
#include "../HttpConn/HttpConn.h"

Server::Server(Config config, EventLoop* loop):
    m_listen_fd(socket_bind_listen(config.server_port)),
    m_listen_channel(new Channel(m_listen_fd)),
    m_loop(loop),
    m_running(false),
    m_is_nolinger(config.no_linger),
    m_thread_pool(new EventLoopThreadPool(loop, config.num_thread)),
    m_server_root(config.server_root)
{   
    if(!set_sock_non_blocking(m_listen_fd)) abort();
    if(!set_sock_reuse_port(m_listen_fd)) abort();

    m_listen_channel->setReadHandler(std::bind(&Server::readHandler, this));
    m_listen_channel->setEvents(EPOLLIN | EPOLLET); // 读事件 和 边缘触发
}

// 应该退出各个loop
// 关闭各个套接字?
Server::~Server(){
    m_running = false;
    m_loop->quit(); // quit来关闭套接字？
    m_thread_pool->stop();
    close(m_listen_fd); // 由谁来关闭？loop中的poller吗？属于谁的，就谁来关
    // TODO
}


void Server::start(){
    if(m_running) return;
    m_running = true;
    m_thread_pool->start(); // 开启线程池
    m_loop->addToPoller(m_listen_channel);
}

void Server::readHandler(){
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int connfd = -1;
    while((connfd = accept(m_listen_fd, (struct sockaddr*)&client_addr, &addr_len)) >= 0){
        LOG_INFO("The Client %s : %d Connected to Server.", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        set_sock_non_blocking(connfd);
        set_sock_nodelay(connfd);
        //if(m_is_nolinger)   set_sock_nolinger(connfd);
        EventLoop* loop = m_thread_pool->getNextLoop();
        std::string conn_name = std::string(inet_ntoa(client_addr.sin_addr)) + ":" + std::to_string(ntohs(client_addr.sin_port));
        std::shared_ptr<HttpConn> http_conn(new HttpConn(loop, connfd, std::move(conn_name), m_server_root));
        // 将这个HttpData加入到poller中，并且将它的channel加入到poller中监听
        loop->queueInLoop(std::bind(&HttpConn::newConn, http_conn));
    }
}
