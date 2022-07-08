/*
@Author: 
@Date: 2021.10
@Desc: Channel
*/

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <memory>
#include <functional>

#include "../HttpConn/HttpConn.h"

class Channel: public std::enable_shared_from_this<Channel>{
    public:
        typedef std::function<void()> Callback;

        Channel();
        Channel(int fd);
        ~Channel();

        void setFd(int fd);
        int getFd();

        void setReadHandler(Callback&& read_handler);
        void setWriteHandler(Callback&& write_handler);
        void setCloseHandler(Callback&& close_handler);

        void setHolder(std::shared_ptr<HttpConn> holder);
        std::shared_ptr<HttpConn> getHolder();

        void setEvents(__uint32_t events);
        __uint32_t getEvents();
        void setRevents(__uint32_t revents);
        __uint32_t getRevents();
        void setLastEvents(__uint32_t last_events);
        __uint32_t getLastEvents();

        void handleEvents();

        std::shared_ptr<Channel> getSelf();

    private:
        int m_fd;

        Callback m_read_handler;
        Callback m_write_handler;
        Callback m_close_handler;

        std::weak_ptr<HttpConn>  m_wk_holder; // 弱引用，不是shared_ptr

        __uint32_t m_events;
        __uint32_t m_revents;
        __uint32_t m_last_events; // 上一次关心的事件
    
    private:
        void handleRead();
        void handleWrite();
        void handleClose();
};


#endif