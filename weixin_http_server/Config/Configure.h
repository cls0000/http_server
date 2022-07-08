/*
@Author: 
@Date: 2021.10
@Desc: 配置类，单例模式
*/


#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>


class Config{
    public:
        // 服务器设置相关
        int server_port; // -p
        int num_thread;  // -t
        bool run_backend; // 是否以服务形式运行 //-b
        bool no_linger;
        std::string server_root;

        // 日志系统相关
        bool enable_logging; // -l
        std::string log_file_name; // -n
        int log_mode; // -a

        // 数据库相关
        std::string db_url; // --url
        int db_port; // --db_port
        std::string db_user; // --db_user
        std::string db_passwd; // --passwd
        std::string db_name;
        int min_num_conn; // 数据库连接池最小连接数 // --min_num_conn
        int max_num_conn; // 数据库连接池最大容量连接数 // --max_num_conn

        void parse_args(int argc, char* argv[]); // 设置参数
        
        // static Config& getInstance();

        // Config(const Config& config) = delete;
        // Config& operator=(const Config& config) = delete;
    // private:
        Config(); // 默认方式初始化参数
        // static Config* config;
};



#endif