/*
@Author: 
@Date: 2021.10
@Desc: 服务器类，总体
*/

#ifndef __SERVER_H__
#define __SERVER_H__

#include <memory>

#include "../EventLoop/EventLoop.h"
#include "../Channel/Channel.h"
#include "../EventLoopThreadPool/EventLoopThreadPool.h"
#include "../Config/Configure.h"

class Server{
    public:
        Server(Config config, EventLoop* loop);
        ~Server();
        void start();
        void readHandler();

    private:
        int m_listen_fd;
        std::shared_ptr<Channel> m_listen_channel;
        EventLoop* m_loop;
        bool m_running;
        bool m_is_nolinger;
        std::string m_server_root;
        std::unique_ptr<EventLoopThreadPool> m_thread_pool; // 只有一个
};


#endif