/*
@Author: 
@Date: 2021.10
@Desc: 连接池，单例模式，使用双检查锁
*/

#ifndef __CONNECTION_POOL_H__
#define __CONNECTION_POOL_H__

#define DEFAULT_SLEEP_TIME 10 // 20 // 管理线程周期检测时间
#define MIN_WAIT_TASK_NUM 1 //5 // 最小等待任务数
#define DEFAULT_CONN_VARY 2 // 每次增减的连接个数


#include <mutex>
#include <list>
#include <thread>
#include <memory>
#include <string>
#include <mysql/mysql.h>

#include "sem.h"



class ConnectionPool{
    public:
        static ConnectionPool& getInstance();
        void init(std::string url, int port, std::string db_name, std::string user, std::string passwd, 
                  int min_num_conn, int max_num_conn);
        // 获取连接
        MYSQL* getConnection();
        // 归还连接
        void retConnection(MYSQL* conn);
        void destroy_pool(); // 销毁连接池
        void stop();
        void start();

    private:
        void manage_pool(); // 管理线程
        void expand_pool(int size); // 扩大连接池
        void reduce_pool(int size); // 缩小连接池
        ~ConnectionPool();
        ConnectionPool();

    private:
        // 单例模式
        static ConnectionPool* pool;
        static std::mutex pool_mtx; // 双检查锁

        // 连接池，使用list实现
        std::list<MYSQL*> conns;
        std::mutex conn_mtx; // 连接池链表的互斥
        Sem sem; // 信号量，由于归还连接一定有空间可以放，而申请连接不一定有，所以只需要一个信号量来控制能不能不能申请连接。暂时先阻塞。 值可能为负数

        // 管理线程 用于定时扩缩连接池
        std::shared_ptr<std::thread> sp_thread;

        // 连接池容量相关
        int max_num_conn; // 最大连接数
        int min_num_conn; // 最小连接数
        int cur_num_conn; // 当前建立的连接数，在running的条件下由管理线程修改
        // int busy_num_conn; // 当前正在被使用的连接数

        // 数据库连接相关
        std::string user;
        std::string passwd;
        std::string url;
        std::string db_name;
        int db_port;

        // 是否使用中
        bool running; 
};

class ConnectionRAII{
    public:
        ConnectionRAII(MYSQL** conn);
        ~ConnectionRAII(); // 析构时归还
    private:
        MYSQL* conn;
};

#endif