/*
@Author: 
@Date: 2021.10
@Desc: 定时器设计
*/

#include <iostream>

#include "Timer.h"
#include "../Log/Logger.h"

Timer::Timer(int timeout, Callback&& cb_func):
            m_cb_func(cb_func),
            m_deleted(false)
{
    time_t now = time(nullptr);
    m_expired_time = now + timeout;
}

Timer::~Timer(){}


bool Timer::isExpired(time_t now){
    if(m_expired_time <= now) return true;
    else return false;
}


bool Timer::isDeleted(){
    return m_deleted;
}

// 延迟删除
void Timer::setDeleted(){
    m_cb_func = nullptr;
    m_deleted = true;
}


void Timer::tick(){
    if(m_cb_func){
        LOG_INFO("Timer::tick() Timer is expired, invoking callback function.");
        m_cb_func();
        m_cb_func = nullptr;
    }    
}


time_t Timer::getExpiredTime(){
    return m_expired_time;
}



void TimerQueue::addTimer(SP_Timer sp_timer){
    if(sp_timer)
        m_timer_queue.push(sp_timer);
}

void TimerQueue::delTimer(SP_Timer sp_timer){
    if(sp_timer)
        sp_timer->setDeleted();
}


void TimerQueue::handleExpiredTimers(){
    LOG_INFO("TimerQueue::handleExpiredTimers() TimerQueue has %d timers.", m_timer_queue.size());
    time_t now = time(NULL);
    while(m_timer_queue.size()){
        SP_Timer timer = m_timer_queue.top();
        if(timer->isDeleted()){//定时器已被删除
            LOG_INFO("TimerQueue::handleExpiredTimers()  delete a invalid timer.");
            m_timer_queue.pop();
        }
        else if(timer->isExpired(now)){//定时器已过期
            LOG_INFO("TimerQueue::handleExpiredTimers()  delete a expired timer.");
            timer->tick();//绑定的回调函数
            m_timer_queue.pop();
        }
        else break;
    }
}