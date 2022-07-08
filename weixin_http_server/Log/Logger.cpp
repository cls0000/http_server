
#include <iostream>

#include "Logger.h"
#include "../Utils/Utils.h"
#include "../EventLoopThreadPool/EventLoopThread.h"


bool Logger::init(char* log_file_name, bool run_backend, bool enable_logging, long log_mode, int cnt_split_file){
    this->m_log_mode = log_mode;
    this->m_cnt_split_file = cnt_split_file;
    this->m_run_backend = run_backend;
    this->m_enable_logging = enable_logging;
    if(this->m_enable_logging == false){
        this->m_running = false;
        return true;
    }
    // 获取日期，以日期为文件名，首先得到日期字符串
    time_t now = time(NULL);
    struct tm* local_now = localtime(&now);
    this->m_today = local_now->tm_mday;
    char date_str[256] = {0};
    snprintf(date_str, sizeof(date_str)-1, "%d_%02d_%02d", local_now->tm_year+1900, local_now->tm_mon+1, local_now->tm_mday);
    
    char full_file_name[512] = {0}; // 完整的文件名
    // 判断 传入的文件名是否为空
    if(log_file_name == nullptr){
        strncpy(full_file_name, date_str, sizeof(date_str)-1);
    }
    else{ 
        // 判断是否包含 目录名 和 文件名
        char* ptr = strrchr(log_file_name, '/');
        
        if(ptr == nullptr){ // 如果没有 / ，说明是一个文件名，没有目录
            snprintf(full_file_name, sizeof(full_file_name)-1, "%s_%s", date_str, log_file_name);
        }
        else{
            strncpy(m_dir_name, log_file_name, min(int(ptr-log_file_name+1), int(sizeof(m_dir_name)-1)));
            strncpy(m_file_name, ptr+1, min(strlen(ptr+1), sizeof(m_file_name)-1));
            snprintf(full_file_name, sizeof(full_file_name)-1, "%s%s_%s", m_dir_name, date_str, m_file_name);
            // 判断是否存在这个目录，如果没有则创建
            if(!is_dir_exists(m_dir_name)){
                create_dir(m_dir_name);
            }
        }
    }

    // 如果是异步日志，那么开一个线程
    if(log_mode == LOG_MODE_ASYNC){
        m_sp_thread.reset(new std::thread(std::bind(&Logger::async_write_log, this))); // 成员函数隐式传入一个this指针，所以需要绑定
    }
    
    m_log_file = fopen(full_file_name, "at+");
    if(m_log_file == nullptr) 
        return false;
    else return true;
    
}


void Logger::async_write_log(){ // 线程运行这个函数，可以访问对象的成员
    while(true){ // 一直循环，直到退出
        std::string log_msg;
        {   
            // 搭配条件变量使用，要用unique_lock;
            std::unique_lock<std::mutex> guard(m_queue_mtx);
            while(m_log_queue.empty()){
                if(!m_running) return;
                m_cond_var.wait(guard);
            }

            log_msg.swap(m_log_queue.front());
            m_log_queue.pop_front();
        } // 减小封锁的范围
        fputs(log_msg.c_str(), m_log_file);
    }
}

// 根据日志模式，同步写到文件，或者异步加入到日志队列中
// 多个线程访问共享变量，所以需要加锁
void Logger::append_log(long level, const char* format, ...){
    if(!(m_enable_logging && m_running)) return;
    // 获取时间
    time_t now = time(NULL);
    struct tm* local_now = localtime(&now);
    char time_str[256] = {0}; // 时间字符串
    snprintf(time_str, sizeof(time_str)-1, "[%d-%02d-%02d %02d:%02d:%02d]", local_now->tm_year+1900, local_now->tm_mon+1, local_now->tm_mday, local_now->tm_hour, local_now->tm_min, local_now->tm_sec);

    // 数字 对应 等级 字符
    char level_str[10] = {0};
    switch(level){
        case LOG_LEVEL_DEBUG: strcpy(level_str, "[DEBUG]"); break;
        case LOG_LEVEL_INFO: strcpy(level_str, "[INFO]"); break;
        case LOG_LEVEL_WARNING: strcpy(level_str, "[WARNING]"); break;
        case LOG_LEVEL_ERROR: strcpy(level_str, "[ERROR]"); break;
        default: strcpy(level_str, "INFO");
    }

    // 判断是否需要新开一个文件
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        m_log_cnt++;
        // 跨日期 或者 超出每个日志文件的行数，则新开一个文件
        // printf("测试 cnt = %lld, m_cnt_split = %d, cnt %% split = %lld\n", m_log_cnt, m_cnt_split_file, m_log_cnt % m_cnt_split_file);
        if(local_now->tm_mday != m_today || m_log_cnt > m_cnt_split_file){
            char full_file_name[512] = {0};
            char date_str[256] = {0};
            snprintf(date_str, sizeof(date_str)-1, "%04d_%02d_%02d", local_now->tm_year+1900, local_now->tm_mon+1, local_now->tm_mday);
            if(local_now->tm_mday != m_today){
                m_today = local_now->tm_mday;
                m_split_cnt = 0;
                snprintf(full_file_name, sizeof(full_file_name)-1, "%s%s_%s", m_dir_name, date_str, m_file_name);
            }
            else{
                m_split_cnt++;
                snprintf(full_file_name, sizeof(full_file_name)-1, "%s%s_%s.%d", m_dir_name, date_str, m_file_name, m_split_cnt);
            }
            m_log_cnt = 1;
            // 关闭原来文件，新建一个文件
            fflush(m_log_file);
            fclose(m_log_file);
            m_log_file = fopen(full_file_name, "at+"); // 新文件
        }
    }

    // 格式化日志字符串
    char msg[512] = {0};
    va_list varglist;
    va_start(varglist, format);
    vsnprintf(msg, sizeof(msg)-1, format, varglist);
    va_end(varglist);

    char content[1024] = {0};
    snprintf(content, sizeof(content)-1, "%s %10s [%5d] %s\n", time_str, level_str, CurrentThread::tid(), msg);

    if(m_log_mode == LOG_MODE_SYNC){
        fputs(content, m_log_file);
    }
    else{ // 异步日志，加入到日志队列中
        {   
            std::lock_guard<std::mutex> guard(m_queue_mtx);
            m_log_queue.emplace_back(content); // emplace_back()方式
        }
        m_cond_var.notify_one();
    }

    if(!m_run_backend){
        write(STDOUT_FILENO, content, strlen(content)); // 不带缓冲区输出
    }
    
}

void Logger::stop(){ // 暂停日志系统
    if(m_running == false) return;
    m_running = false;
    if(m_log_mode == LOG_MODE_ASYNC){
        // 通过条件变量告诉 工作线程 停止工作
        m_cond_var.notify_one();
        // 等待工作线程退出
        m_sp_thread->join();
    }
    fflush(m_log_file); // 写入文件
}

Logger::Logger(){
    memset(this->m_dir_name, 0, sizeof(this->m_dir_name));
    memset(this->m_file_name, 0, sizeof(m_file_name));
    this->m_enable_logging = true;
    this->m_running = true;
    this->m_log_cnt = 0;
    this->m_split_cnt = 0;
    this->m_run_backend = false;
}

Logger::~Logger(){
    stop(); // 停止
    fclose(m_log_file); // 关闭
}