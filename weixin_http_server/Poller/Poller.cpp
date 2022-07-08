/*
@Author: 
@Date: 2021.10
@Desc: epoll的封装
*/

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <errno.h> // errno每一线程都有一份
#include <memory>

#include "Poller.h"
#include "../Log/Logger.h"


const int EPOLL_WAIT_TIME = 10000;

Poller::Poller(int max_events):
    max_events(max_events),
    m_epoll_events(max_events),
    m_epoll_fd(epoll_create1(EPOLL_CLOEXEC)){}

Poller::~Poller(){}

void Poller::poll(std::vector<std::shared_ptr<Channel>>& activeChannels){
    int active_num = epoll_wait(m_epoll_fd, &(*m_epoll_events.begin()), max_events, EPOLL_WAIT_TIME);
    
    if(active_num > 0){
        LOG_INFO("Poller:poll()  Epoll Wait: %d channels' events happen.", active_num);
        getActiveChannels(active_num, activeChannels);
    }
    else if(active_num == 0){
        LOG_INFO("Poller:poll()  Epoll Wait Timeout.");
    }
    else{
        if(errno == EINTR){
            LOG_INFO("Poller:poll()  Epoll Wait is Interrupted.");
        }
        else{
            LOG_ERROR("Epoll Wait Error: %s", strerror(errno));
        }
    }
}

void Poller::getActiveChannels(int active_num, std::vector<std::shared_ptr<Channel>>& activeChannels){
    assert(active_num <= max_events);
    for(int i = 0; i < active_num; i++){
        struct epoll_event event = m_epoll_events[i];
        Channel* pChannel = static_cast<Channel*>(event.data.ptr);
        pChannel->setRevents(event.events); // 发生的事件
        activeChannels.push_back(pChannel->getSelf()); // 由于epoll_event只能保存指针，但是vector保存的是shared_ptr,所以需要获取this的shared_ptr;
    }
    // 需要清空m_epoll_events吗？muduo没有.
}

void Poller::addChannel(std::shared_ptr<Channel> channel){
    if(channel){
        struct epoll_event event; // 
        bzero(&event, sizeof(event));
        event.events = channel->getEvents();
        event.data.ptr = channel.get();
        int fd = channel->getFd();
        int ret = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event);
        if(ret < 0){
            LOG_ERROR("Add Channel Error: %s.", strerror(errno));
        }
        // 将Channel所对应的HttpConn的wk_ptr以shared_ptr的形式，加入到m_conns中
        std::shared_ptr<HttpConn> sp_http_conn = channel->getHolder();
        if(sp_http_conn){ // 有对应的HttpConn才加入，像listen_channel 和wakeup_channel没有对应的HttpConn，所以不加入
            m_conns[sp_http_conn->getName()] = sp_http_conn;
        }
    }
}

void Poller::delChannel(std::shared_ptr<Channel> channel){
    if(channel){
        // 从epollfd中删除connfd
        int fd = channel->getFd();
        int ret = epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
        if(ret < 0){
            LOG_ERROR("Poller::delChannel()  Del Channel Error: %s.", strerror(errno));
        }
        // 从m_conns中删除httpConn对象
        std::shared_ptr<HttpConn> sp_http_conn = channel->getHolder();
        if(sp_http_conn){
            m_conns.erase(sp_http_conn->getName());
        }
    }
}

// 主要是为了修改监听的事件
void Poller::modChannel(std::shared_ptr<Channel> channel){
    if(channel){
        // 如果前后想要监听的事件一致，那么跳过
        if(channel->getEvents() != channel->getLastEvents()){
            int fd = channel->getFd();
            struct epoll_event event;
            event.events = channel->getEvents();
            event.data.ptr = channel.get();
            int ret = epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &event);
            if(ret < 0){
                LOG_ERROR("Mod Channel Error: %s.", strerror(errno));
            }
        }
    }
}