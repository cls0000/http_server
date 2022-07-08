/*
@Author: 
@Date: 2021.10
@Desc: 线程池
*/

#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include <vector>
#include <memory>

#include "../EventLoop/EventLoop.h"
#include "EventLoopThread.h"

class EventLoopThreadPool{
    public:
        EventLoopThreadPool(EventLoop* base_loop, int num_threads);
        ~EventLoopThreadPool();
        EventLoop* getNextLoop();
        void start();
        void stop();
    private:
        int m_next; // round robin方式选择下一个loop
        int m_num_threads; 
        std::vector<std::shared_ptr<EventLoopThread>> m_threads;
        std::vector<EventLoop*> m_loops;
        bool m_running; 
        EventLoop* m_base_loop;
};

#endif 