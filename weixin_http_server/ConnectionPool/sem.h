/*
@Author: 
@Date: 2021.10
@Desc: 对semaphore的封装
*/

#ifndef __SEM_H__
#define __SEM_H__

#include <mutex>
#include <condition_variable>

class Sem{
    public:
        Sem(long int value = 0);
        void wait();
        void post();
        long int getvalue();
        void reset_value(long int value);
    private:
        long int value; // 信号量的值
        long int wakeups;  // 能唤醒的名额数
        std::mutex mtx;
        std::condition_variable cond;
};  


#endif