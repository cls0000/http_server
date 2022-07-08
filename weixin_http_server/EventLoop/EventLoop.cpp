/*
@Author: 
@Date: 2021.10
@Desc: 事件循环
*/

#include <unistd.h>
#include <sys/epoll.h>
#include <functional>
#include <assert.h>
#include <iostream>

#include "EventLoop.h"
#include "../Utils/Utils.h"
#include "../Channel/Channel.h"
#include "../Poller/Poller.h"
#include "../Timer/Timer.h"
#include "../Log/Logger.h"
#include "../EventLoopThreadPool/EventLoopThread.h"

__thread EventLoop* t_loopInThisThread = nullptr;

EventLoop::EventLoop():
    m_wakeup_fd(create_wakeup_fd()),
    m_wakeup_channel(new Channel(m_wakeup_fd)),
    m_poller(new Poller()),
    thread_id(CurrentThread::tid()), // 系统全局id（lwp id）
    m_looping(false),
    m_doing_pending(false),
    m_timer_queue(new TimerQueue())
{
    // 设置 t_loopInThisThread;
    if(t_loopInThisThread == nullptr){
        t_loopInThisThread = this;
    }  
    else{

    }
    // 绑定wakeupfd的读处理函数，监听的事件
    m_wakeup_channel->setEvents(EPOLLIN | EPOLLET);
    m_wakeup_channel->setReadHandler(std::bind(&EventLoop::readHandler, this));
    // 将channel加入到poller中
    m_poller->addChannel(m_wakeup_channel);
}

EventLoop::~EventLoop(){
    close(m_wakeup_fd);
    // TODO 其他操作？
}

void EventLoop::loop(){
    assertInLoopThread();
    assert(!m_looping);
    m_looping = true;
    std::vector<std::shared_ptr<Channel>> active_channels;
    while(m_looping){
        active_channels.clear();
        m_poller->poll(active_channels);
        for(auto& active_channel : active_channels) 
            active_channel->handleEvents();//执行活跃channel的回调函数
        doPendingFunctors();//处理函数
        handleExpired();//销毁超时的定时器
    }
}

void EventLoop::quit(){
    if(!m_looping) return;
    m_looping = false;
    if(!isInLoopThread()){
        wakeup();
    }
}

void EventLoop::assertInLoopThread(){
    assert(isInLoopThread());
}

bool EventLoop::isInLoopThread(){
    return thread_id == CurrentThread::tid();
}


void EventLoop::queueInLoop(Functor&& func){
    // 由于需要和别的线程互斥访问pendingfunctors这个vector，所以需要加锁
    // 传入右值，调用移动构造函数
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        m_pending_functors.emplace_back(func);
    }
    if(!isInLoopThread() || m_doing_pending) wakeup();// 将计时器加入
}


// 加入到Poller中监听
void EventLoop::addToPoller(std::shared_ptr<Channel> sp_channel, int timeout){
    if(sp_channel){
        if(timeout){
            // 将计时器加入
            std::shared_ptr<HttpConn> sp_conn = sp_channel->getHolder();
            auto callback = std::bind(&HttpConn::closeHandler, sp_conn.get());
            std::shared_ptr<Timer> sp_timer(new Timer(timeout, callback));
            sp_conn->setTimer(sp_timer); // 设置HttpConn的Timer
            m_timer_queue->addTimer(sp_timer); // 将Timer加入到TimerQueue中
        }
        m_poller->addChannel(sp_channel);
    }  
}


// 从Poller中去除
void EventLoop::removeFromPoller(std::shared_ptr<Channel> sp_channel){
    // 1.从epoll中删除对应的fd
    // 2.从m_conns中删除对应的HttpConn对象
    // 以上两个操作都由poller来完成
    // 实际上应该从timerQueue中删除timer，但是使用延迟删除
    if(sp_channel){
        m_poller->delChannel(sp_channel);
    }
}

// 修改监听的事件
void EventLoop::updatePoller(std::shared_ptr<Channel> sp_channel, int timeout){
    // 设置原计时器无效，解除和对应HttpConn对象的关系（这个处理由HttpConn对象处理）
    // 创建新的计时器，绑定到HttpConn对象中，加入到timerQueue中
    // 将Channel修改到epoll中
    if(sp_channel){
        if(timeout){
            // std::cout << "EventLoop::updatePoller() 新计时器" << std::endl;
            std::shared_ptr<HttpConn> sp_conn = sp_channel->getHolder();
            auto callback = std::bind(&HttpConn::closeHandler, sp_conn.get());
            std::shared_ptr<Timer> sp_timer(new Timer(timeout, callback));
            sp_conn->setTimer(sp_timer);
            m_timer_queue->addTimer(sp_timer);
        }
        m_poller->modChannel(sp_channel);
    }
}


// 向eventfd中写
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t ret = write(m_wakeup_fd, &one, sizeof(one));
    if (ret != sizeof(one)) {
        LOG_INFO("EventLoop::wakeup() writes %d bytes instead of 8", ret);
    }

}


void EventLoop::readHandler(){
    uint64_t one = 1;
    ssize_t ret = read(m_wakeup_fd, &one, sizeof(one));
    if(ret != sizeof(one)){
        LOG_INFO("EventLoop::readHandler() reads %d bytes instead of 8.", ret);
    }
}


void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;
    m_doing_pending = true;
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        functors.swap(m_pending_functors);
    }
    LOG_INFO("EventLoop::doPendingFunctors() %d pendingFunctors.", functors.size());
    for(auto& func : functors){
        func();
    }
    m_doing_pending = false;
}


void EventLoop::handleExpired(){
    m_timer_queue->handleExpiredTimers();
}
