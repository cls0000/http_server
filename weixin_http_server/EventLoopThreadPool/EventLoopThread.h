/*
@Author: 
@Date: 2021.10
@Desc: 封装的event线程
*/

#ifndef __EVENT_LOOP_THREAD_H__
#define __EVENT_LOOP_THREAD_H__

#include <stdint.h>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "../EventLoop/EventLoop.h"

namespace CurrentThread{
    extern __thread int t_cachedTid; // 各个线程都有一份
    
    void cacheTid();
    inline int tid(){
        if(__builtin_expect(t_cachedTid == 0, 0)){
            cacheTid();
        }
        return t_cachedTid;
    }
}


class EventLoopThread{
    public:
        EventLoopThread();
        ~EventLoopThread();
        EventLoop* startLoop();
        void stop();
        
    private:
        EventLoop* m_loop;
        bool m_running;
        std::shared_ptr<std::thread> m_sp_thread;
        std::mutex m_mtx;
        std::condition_variable m_cond_var;
        int m_tid;
    
    private:
        void worker_threadFunc();
};

#endif