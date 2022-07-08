/*
@Author: 
@Date: 2021.10
@Desc: 对semaphore的封装
*/

#include "sem.h"

Sem::Sem(long int value) : value(value), wakeups(0){}

void Sem::wait(){
    std::unique_lock<std::mutex> guard(mtx);
    if(--value < 0){
        cond.wait(guard, [&](){return wakeups>0;});
        wakeups--;
    } 
}

void Sem::post(){
    std::lock_guard<std::mutex> guard(mtx);
    if(++value <= 0){
        wakeups++;
        cond.notify_one();
    }
}

long int Sem::getvalue(){
    std::lock_guard<std::mutex> guard(mtx);
    return value;
}

void Sem::reset_value(long int value){
    std::lock_guard<std::mutex> guard(mtx);
    this->value = value;
}