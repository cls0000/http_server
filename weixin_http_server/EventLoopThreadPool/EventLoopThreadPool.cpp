/*
@Author: 
@Date: 2021.10
@Desc: 线程池
*/

#include <assert.h>
#include <iostream>

#include "EventLoopThreadPool.h"
#include "../Log/Logger.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* base_loop, int num_threads){
    if(num_threads < 0){
        LOG_ERROR("Num Threads < 0");
        abort();
    }
    m_base_loop = base_loop;
    m_num_threads = num_threads;
    m_running = false;
    m_next = 0;
    LOG_INFO("Create EventLoopThreadPool. Total %d threads", m_num_threads);
}

EventLoopThreadPool::~EventLoopThreadPool(){
    stop();
}


void EventLoopThreadPool::stop(){
    if(!m_running) return;
    else{
        LOG_INFO("Stop EventLoopThreadPool");
        m_running = false;
        for(int i = 0; i < m_threads.size(); i++){
            m_threads[i]->stop();
        }
    }
}

// 如果没有，返回base_loop
EventLoop* EventLoopThreadPool::getNextLoop(){
    m_base_loop->assertInLoopThread();
    assert(m_running);
    EventLoop* ret_loop = m_base_loop;
    if(m_loops.size()){
        ret_loop = m_loops[m_next];
        m_next = (m_next + 1) % m_num_threads;
    }
    return ret_loop;
}


void EventLoopThreadPool::start(){
    m_base_loop->assertInLoopThread();
    if(m_running) return;
    m_running = true;
    for(int i = 0; i < m_num_threads; i++){
        std::shared_ptr<EventLoopThread> sp_thread(new EventLoopThread());
        m_threads.push_back(sp_thread);
        m_loops.push_back(sp_thread->startLoop());
    }
    LOG_INFO("EventLoopThreadPool Starts.");
}

