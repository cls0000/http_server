/*
@Author: 
@Date: 2021.10
@Desc: epoll的封装
*/

#ifndef __POLLER_H__
#define __POLLER_H__

#include <vector>
#include <sys/epoll.h>
#include <map>

#include "../Channel/Channel.h"
#include "../HttpConn/HttpConn.h"

class Poller{
    public:
        // 不负责管理计时器
        // 负责Channel和HttpConn
        Poller(int max_events=4096);
        ~Poller();
        void poll(std::vector<std::shared_ptr<Channel>>& activeChannels);
        void getActiveChannels(int active_num, std::vector<std::shared_ptr<Channel>>& activeChannels);
        void addChannel(std::shared_ptr<Channel> channel); // 将 HttpConn 的shared_ptr加入到 m_conns 中
        void delChannel(std::shared_ptr<Channel> channel);
        void modChannel(std::shared_ptr<Channel> channel);
    private:
        std::vector<epoll_event> m_epoll_events;
        int m_epoll_fd; 
        const int max_events; // m_epoll_events数组的大小
        // poller持有HttpConn对象的shared_ptr
        std::map<std::string, std::shared_ptr<HttpConn>> m_conns;
};


#endif