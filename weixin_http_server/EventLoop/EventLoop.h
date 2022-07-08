/*
@Author: 
@Date: 2021.10
@Desc: 事件循环
*/

#ifndef __EVENT_LOOP_H__
#define __EVENT_LOOP_H__


#include <memory>
#include <vector>
#include <mutex>
#include <functional>

#include "../Channel/Channel.h"
#include "../Poller/Poller.h"
#include "../Timer/Timer.h"


class EventLoop{
    public:
        typedef std::function<void(void)> Functor;

        EventLoop();
        ~EventLoop();
        void loop();
        void quit();
        void assertInLoopThread();
        bool isInLoopThread();
        void queueInLoop(Functor&& func);
        // EventLoop 操作的是Channel
        void addToPoller(std::shared_ptr<Channel> sp_channel, int timeout=0);
        void removeFromPoller(std::shared_ptr<Channel> sp_channel);
        void updatePoller(std::shared_ptr<Channel> sp_channel, int timeout=0);

    private:
        int m_wakeup_fd;
        std::shared_ptr<Channel> m_wakeup_channel;

        std::shared_ptr<Poller> m_poller;
        
        const int thread_id; // 对应的线程的id

        std::vector<Functor> m_pending_functors;
        std::mutex m_mtx;

        bool m_looping; // 正在事件循环
        bool m_doing_pending; // 正在处理pending functors

        std::shared_ptr<TimerQueue> m_timer_queue;

    private:
        void wakeup();
        void readHandler();
        void doPendingFunctors();
        void handleExpired();
};

#endif