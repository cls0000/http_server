/*
@Author: 
@Date: 2021.10
@Desc: 连接池
*/

#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>

#include "ConnectionPool.h"
#include "../Log/Logger.h" // 错误时输出日志

ConnectionPool* ConnectionPool::pool = nullptr;
std::mutex ConnectionPool::pool_mtx;

ConnectionPool& ConnectionPool::getInstance(){
    if(pool == nullptr){
        std::lock_guard<std::mutex> guard(pool_mtx);
        if(pool == nullptr){
            pool = new ConnectionPool();
        }
    }
    return *pool;
}


// 默认配置
ConnectionPool::ConnectionPool(){
    max_num_conn = 20;
    min_num_conn = 2;
    cur_num_conn = 0;
    // busy_num_conn = 0;

    user = "demo";
    passwd = "demo";
    url = "localhost";
    db_name = "web";
    db_port = 3306;

    running = true;
}

// 初始化参数，创建连接池
void  ConnectionPool::init(std::string url, int port, std::string db_name, std::string user, std::string passwd, 
                  int min_num_conn, int max_num_conn){
    this->url = url;
    this->db_port = port;
    this->db_name = db_name;
    this->user = user;
    this->passwd = passwd;
    this->min_num_conn = min_num_conn;
    this->max_num_conn = max_num_conn;

    expand_pool(min_num_conn);
    // // 初始化 连接池 , 还没有多线程，不用加锁互斥
    // for(int i = 0; i < min_num_conn; i++){
    //     MYSQL* conn = NULL; // 一定要初始化为 NULL
    //     conn = mysql_init(conn);

    //     if(conn == NULL){
    //         LOG_ERROR("MySQL Init Error."); // LOGGER单例模式，不需要单独定义初始化
    //         exit(1); // 不能接受的错误
    //     }

    //     conn = mysql_real_connect(conn, url.c_str(), user.c_str(), passwd.c_str(), db_name.c_str(), port, NULL, 0);
    //     if(conn == NULL){
    //         std::cout << "ERROR" << std::endl;
    //         LOG_ERROR("MySQL Connection Error.");
    //         exit(1);
    //     }

    //     conns.push_back(conn);
    //     cur_num_conn++; // 当前的连接数增加1
    // }
    // // 初始化信号量
    // sem = Sem(cur_num_conn);

    // 启动管理线程
    sp_thread.reset(new std::thread(&ConnectionPool::manage_pool, this));
}



// 管理线程，running为true时才工作, 扩展或者缩小都要重新设定信号量的大小
void ConnectionPool::manage_pool(){ 
    // 使用到 running成员变量
    while(running){
        sleep(DEFAULT_SLEEP_TIME); // 周期检测
        std::lock_guard<std::mutex> guard(conn_mtx);

        int num_wait = -sem.getvalue(); // 得到的剩下的连接数，取反才是等待连接的数量
        // std::cout << "num_wait等待数量" << num_wait << std::endl;
        int busy_num_conn = num_wait >= 0 ? cur_num_conn : cur_num_conn + num_wait; // 正在被使用的连接数
        // 如果等待连接的数量 多于 设定的数量，则增加连接 DEFAULT_CONN_VARY 个
        if(num_wait >= MIN_WAIT_TASK_NUM){
            LOG_INFO("Add %d new SQL connection.", min(DEFAULT_CONN_VARY, max_num_conn - cur_num_conn));
            expand_pool(min(DEFAULT_CONN_VARY, max_num_conn - cur_num_conn));
        }

        // 如果 正在被使用的连接×2 小于全部连接 
        else if(busy_num_conn * 2 < cur_num_conn){
            LOG_INFO("Release %d SQL connection.", min(DEFAULT_CONN_VARY, cur_num_conn - min_num_conn));
            reduce_pool(min(DEFAULT_CONN_VARY, cur_num_conn - min_num_conn));
        }
    }
}

// 销毁连接池, 释放所有的连接。正在被使用的连接怎么处理？
// 非运行时才能够销毁
void ConnectionPool::destroy_pool(){ 
    // 以下只是对空闲的连接销毁。
    if(!running){
        std::lock_guard<std::mutex> guard(conn_mtx);
        reduce_pool(conns.size()); // 在这里关闭连接
        // while(conns.size()){
        //     MYSQL* conn = conns.back();
        //     conns.pop_back();
        //     mysql_close(conn);
        //     cur_num_conn--;
        // }
    }   
}


// 扩大连接池（running为true）
void ConnectionPool::expand_pool(int size){ 
    if(running)
        for(int i = 0; i < size; i++){
            MYSQL* conn = nullptr; // 一定要先初始化为nullptr
            conn = mysql_init(conn);
            if(conn == nullptr){
                LOG_ERROR("MYSQL Init Error.");
                exit(1);
            }

            conn = mysql_real_connect(conn, url.c_str(), user.c_str(), passwd.c_str(), db_name.c_str(), db_port, NULL, 0);
            if(conn == nullptr){
                LOG_ERROR("MYSQL Connection Error.");
                exit(1);
            }
            
            conns.push_back(conn);
            cur_num_conn++; // 注意这里修改了cur_num_conn;
            sem.post(); // 每增加一个连接，sem信号量 + 1
        }
}

// 缩小连接池（running为true）
void ConnectionPool::reduce_pool(int size){ 
    if(running)
        for(int i = 0; i < size; i++){
            MYSQL* conn = conns.back();
            conns.pop_back();
            mysql_close(conn); 
            cur_num_conn--; // 注意这里修改了cur_num_conn;
            sem.wait(); // 每缩减一个连接， 信号量-1
        }
}

// 设置running为false，等待所有连接归还
void ConnectionPool::stop(){
    if(running == false) return;
    running = false;

    // 等待所有连接归还
    // 已经知道当前建立了 cur_num_conn 个连接
    // 信号量的值 sem 代表了剩下的连接个数
    // 通过信号量 阻塞等待 归还

    int cur_num = cur_num_conn;
    for(int i = 0; i < cur_num; i++){
        sem.wait(); // 一开始有剩余的连接，所以不阻塞，后面阻塞等待分配出去的连接归还
    }
    sem.reset_value(cur_num_conn);
}

void ConnectionPool::start(){
    running = true;
}

ConnectionPool::~ConnectionPool(){
    stop(); 
    sp_thread->join(); // 等待管理线程结束
    destroy_pool(); 
}


// 如果running为false，则获取失败
MYSQL* ConnectionPool::getConnection(){
    if(!running) return nullptr;
    sem.wait();
    std::lock_guard<std::mutex> guard(conn_mtx);
    MYSQL* conn = conns.front();
    conns.pop_front();
    return conn;
}


// 归还连接
void ConnectionPool::retConnection(MYSQL* conn){
    // 判断conn是否为NULL
    if(conn == nullptr)
        return;

    std::lock_guard<std::mutex> guard(conn_mtx);
    conns.push_back(conn);
    sem.post(); // post 和 wait 是原子操作
}



ConnectionRAII::ConnectionRAII(MYSQL** conn){
    *conn = ConnectionPool::getInstance().getConnection();
    this->conn = *conn;
}

ConnectionRAII::~ConnectionRAII(){ // 析构时归还
    ConnectionPool::getInstance().retConnection(this->conn);
}