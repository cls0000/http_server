/*
@Author: 
@Date: 2021.10
@Desc: Channel
*/

#include <sys/epoll.h>
#include <iostream>

#include "Channel.h"
#include "../Log/Logger.h"

Channel::Channel(){
    m_fd = -1;
    m_events = 0;
    m_revents = 0;
    m_last_events = 0;
}

Channel::Channel(int fd){
    m_fd = fd;
    m_events = 0;
    m_revents = 0;
    m_last_events = 0;
}

Channel::~Channel(){}

void Channel::setFd(int fd){
    m_fd = fd;
}

int Channel::getFd(){
    return m_fd;
}

void Channel::setReadHandler(Callback&& read_handler){
    m_read_handler = read_handler; // 移动拷贝
}

void Channel::setWriteHandler(Callback&& write_handler){
    m_write_handler = write_handler;
}



void Channel::setCloseHandler(Callback&& close_handler){
    m_close_handler = close_handler;
}

void Channel::setHolder(std::shared_ptr<HttpConn> holder){
    m_wk_holder = holder;
}

std::shared_ptr<HttpConn> Channel::getHolder(){
    return m_wk_holder.lock();
}


void Channel::setEvents(__uint32_t events){
    m_events = events;
}

__uint32_t Channel::getEvents(){
    return m_events;
}

void Channel::setRevents(__uint32_t revents){
    m_revents = revents;
}

__uint32_t Channel::getRevents(){
    return m_revents;
}

void Channel::setLastEvents(__uint32_t last_events){
    m_last_events = last_events;
}

__uint32_t Channel::getLastEvents(){
    return m_last_events;
}


void Channel::handleRead(){
    if(m_read_handler){
        m_read_handler();
    }
}

void Channel::handleWrite(){
    if(m_write_handler){
        m_write_handler();
    }
}

void Channel::handleClose(){
    if(m_close_handler){
        m_close_handler();
    }
}

void Channel::handleEvents(){
    // 更新m_last_events;
    m_last_events = m_events;
    // 在下面的处理过程中，更新m_events
    m_events = 0; 

    if(m_revents & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)){
        LOG_INFO("Channel::handleEvents() HUP or ERR or RDHUP event happens.");
        handleClose();
        return;
    }
    else{
        if(m_revents & (EPOLLIN | EPOLLPRI)){
            LOG_INFO("Channel::handleEvents() read event happens.");
            handleRead();
        }
        if(m_revents & EPOLLOUT){
            LOG_INFO("Channel::handleEvents() write event happens.");
            handleWrite();
        }
    }
}


std::shared_ptr<Channel> Channel::getSelf(){
    std::shared_ptr<Channel> p = shared_from_this();
    return p;
}

