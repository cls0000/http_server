/*
@Author: 
@Date: 2021.10
@Desc: 封装http数据，及其处理函数
*/

#include <sys/epoll.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <sstream>

#include "HttpConn.h"
#include "../Log/Logger.h"
#include "../Utils/Utils.h" // Utils.h头文件include了EventLoop.h头文件，EventLoop.h头文件include了Channel.h头文件，所以成功。否则失败
#include "../EventLoop/EventLoop.h"
#include "../Channel/Channel.h"
#include "../Servlet/Servlet.h"

const int DEFAULT_EXPIRED_TIME = 2; // s
const int DEFAULT_KEEP_ALIVE_TIME = 2 * 60; // 120s
const int MAX_BUFF_SIZE = 1024;

std::map<HTTP_CODE, int> status_code = {
    {FILE_REQUEST, 200},
    {JSON_REQUEST, 200},
    {BAD_REQUEST, 400},
    {FORBIDDEN_REQUEST, 403},
    {NO_RESOURCE, 404},
    {INTERNAL_ERROR, 500},
    {NOT_IMPLEMENTED, 501}
};

std::map<HTTP_CODE, std::string> status_desc = {
    {FILE_REQUEST, "OK"},
    {JSON_REQUEST, "OK"},
    {BAD_REQUEST, "Bad Request"},
    {FORBIDDEN_REQUEST, "Forbidden"},
    {NO_RESOURCE, "Not Found"},
    {INTERNAL_ERROR, "Internal Error"},
    {NOT_IMPLEMENTED, "Not Implemented"}
};

std::map<HTTP_CODE, std::string> response_content = {
    {BAD_REQUEST, "Your request has bad syntax or is inherently impossible to staisfy.\n"},
    {FORBIDDEN_REQUEST, "You do not have permission to get file form this server.\n"},
    {NO_RESOURCE, "The requested file was not found on this server.\n"},
    {INTERNAL_ERROR, "There was an unusual problem serving the request file.\n"},
    {NOT_IMPLEMENTED, "THe request method is not implememted!\n"}
};

std::map<std::string, std::string> mime = {
    {".html", "text/html"},
    {".avi", "video/x-msvideo"},
    {".bmp", "image/bmp"},
    {".c", "text/plain"},
    {".doc", "application/msword"},
    {".json", "application/json"},
    {".gif", "image/gif"},
    {".gz", "application/x-gzip"},
    {".htm", "text/html"},
    {".ico", "image/x-icon"},
    {".jpg", "image/jpeg"},
    {".png", "image/png"},
    {".txt", "text/plain"},
    {".mp3", "audio/mp3"},
    {"default", "text/html"}
};



HttpConn::HttpConn(EventLoop* loop, int conn_fd, std::string&& conn_name, std::string path):
    m_loop(loop),
    m_conn_fd(conn_fd),
    m_channel(new Channel(conn_fd)),
    m_conn_name(conn_name),
    m_read_idx(0),
    m_check_state(CHECK_STATE_REQUESTLINE),
    m_error_exist(false),
    m_file_category("text/html"),
    m_path(path)
{
    if(!is_dir_exists(m_path.c_str())){
        create_dir(m_path.c_str());
    }
    m_channel->setEvents(EPOLLET | EPOLLIN);
    m_channel->setReadHandler(std::bind(&HttpConn::readHandler, this));
    m_channel->setWriteHandler(std::bind(&HttpConn::writeHandler, this));
    m_channel->setCloseHandler(std::bind(&HttpConn::closeHandler, this));
}

HttpConn::~HttpConn(){
    close(m_conn_fd);
}

void HttpConn::readHandler(){
    bool ret = read();
    if(!ret){
        closeHandler(); // 若出错或者对端关闭，则关闭fd
        return;
    }
    // 对数据进行分析处理
    HTTP_CODE analyse_ret = analyseRequest();
    // 对不同的HTTP_CODE进行处理
    if(analyse_ret == NO_REQUEST){ // 数据不够，继续读取
        m_channel->setEvents(EPOLLIN | EPOLLET);
        int timeout = DEFAULT_EXPIRED_TIME;
        if(m_keep_alive)
            timeout = DEFAULT_KEEP_ALIVE_TIME;
        seperateTimer();
        m_loop->updatePoller(m_channel, timeout);
        return;
    } 
    if(analyse_ret != FILE_REQUEST) m_error_exist = true; // 报文分析错误，或者请求的内容错误

    ret = sendResponse(analyse_ret); //写到m_iovec
    if(!ret){
        closeHandler();
        return;
    }
    
    writeHandler(); // 调用writev将缓冲区输出到网络中
}

void HttpConn::writeHandler(){
    // 返回值应该区分：1.写失败，2.全部写完成，3.部分写，还需监听写
    WRITE_RESULT write_result = write(); // 从缓冲区写入网络中
    if(write_result == WRITE_ERROR) closeHandler();
    else if(write_result == WRITE_PART){
        m_channel->setEvents(EPOLLOUT | EPOLLET);
        int timeout = DEFAULT_EXPIRED_TIME; // 什么时候修改定时器？答：调用updatePoler时。所以先要解除原TImer和HttpConn的关系
        if(m_keep_alive)
            timeout = DEFAULT_KEEP_ALIVE_TIME;
        seperateTimer();
        m_loop->updatePoller(m_channel, timeout);
    }
    else{ // DONE: 这里的错误状态要在下一个write可见，不应该用analyse_ret来比较
        // 全部数据写完到网络中，重新设定计时器？
        // 什么时机设置计时器？答：每一次触发就修改超时时间
        if(m_error_exist) 
            closeHandler();
        else // 由于数据写完，所以设置IN事件，开启接收新的请求
        {
            m_channel->setEvents(EPOLLIN | EPOLLET);
            int timeout = DEFAULT_EXPIRED_TIME;
            if(m_keep_alive)
                timeout = DEFAULT_KEEP_ALIVE_TIME;
            seperateTimer();
            reset(); // 重新设置某些参数，开始接收新的请求报文
            m_loop->updatePoller(m_channel, timeout);
        }
    }
}

void HttpConn::closeHandler(){
    // 1.设置timer为deleted状态，并且设置其callback为nullptr
    // 2.设置m_timer为nullptr，解除本对象和对应的timer的关系
    // 3.从epollfd中删除connfd，并且从poller的m_conns中删除对应的http_conn对象
    // 使得删除前，对应的HttpConn对象至少有一个shared_ptr引用
    std::shared_ptr<HttpConn> guard(shared_from_this());
    reset();
    m_loop->removeFromPoller(m_channel);
}



void HttpConn::newConn(){
    // 设置Channel对应的HttpConn
    m_channel->setHolder(shared_from_this());
    // 使用loop的addToPoller加入到poller中
    m_loop->addToPoller(m_channel, DEFAULT_EXPIRED_TIME);
}


void HttpConn::setTimer(std::shared_ptr<Timer> timer){
    m_timer = timer;
}

std::string HttpConn::getName(){
    return m_conn_name;
}

std::shared_ptr<Channel> HttpConn::getChannel(){
    return m_channel;
}


bool HttpConn::read(){
    // // 循环读取数据，直到没有数据
    char buff[MAX_BUFF_SIZE];
    ssize_t nread = 0;
    while(true){
        nread = recv(m_conn_fd, buff, MAX_BUFF_SIZE, 0);
        
        if(nread < 0){
            if(errno == EINTR) continue;
            else if(errno == EAGAIN || errno == EWOULDBLOCK) return true;
            else{
                LOG_ERROR("HttpConn::read Error : %s.", strerror(errno));
                return false;
            }
        }
        else if(nread == 0){ // 对端关闭
            return false;
        }
        else{
            m_inbuff += std::string(buff, buff + nread);
        }
    }
    return true;
}


WRITE_RESULT HttpConn::write(){
    // DONE: 非阻塞写数据，发送完记得关闭映射区 munmap: 答：返回WRITE_ALL, 由reset()函数负责munmap
    int ret = 0;
    while(true){ // 循环写数据直到不能再写
        ret = writev(m_conn_fd, m_iovec, 2);
        if(ret < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                return WRITE_PART;
            }
            else{
                LOG_ERROR("HttpConn::write() Error.");
                // 是否需要关闭mmap的区域？由调用closeHandler()时调用reset()函数来munmap
                return WRITE_ERROR;
            }
        }
        if(ret < m_iovec[0].iov_len){
            m_outbuff.erase(0, ret);
            m_iovec[0].iov_base = &(*m_outbuff.begin());
            m_iovec[0].iov_len = m_iovec[0].iov_len - ret;
        }
        else if(ret >= m_iovec[0].iov_len && ret < m_iovec[0].iov_len + m_iovec[1].iov_len){
            m_iovec[1].iov_base = static_cast<void*>(static_cast<char*>(m_iovec[1].iov_base) + (ret - m_iovec[0].iov_len));
            m_iovec[1].iov_len = m_iovec[1].iov_len + m_iovec[0].iov_len - ret;
            m_outbuff.clear();
            m_iovec[0].iov_base = nullptr;
            m_iovec[0].iov_len = 0;
        }
        else{
            m_iovec[0].iov_base = nullptr;
            m_iovec[0].iov_len = 0;
            m_iovec[1].iov_base = nullptr;
            m_iovec[1].iov_len = 0;
            m_outbuff.clear(); // 之前忘记清空m_outbuff了
            break;
        }
    }
    return WRITE_ALL;
}

// 将需要发送的数据写入写缓冲区（进程空间）, 文件创建映射区
bool HttpConn::sendResponse(HTTP_CODE code){
    // DONE: 将发送的内容加入到写缓冲：iovec[2]中
    switch(code){
        case INTERNAL_ERROR:
        case BAD_REQUEST:
        case NO_RESOURCE:
        case FORBIDDEN_REQUEST:
        case NOT_IMPLEMENTED:
        case JSON_REQUEST:
        {
            add_status_line(code);
            add_headers(code);
            add_content(code);
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(code);
            add_headers(code);
            //mutipart/处理 FIXME
            // if(m_headers.find("content-type") !=m_headers.end()){
            //     const std::string &Content_Type = m_headers["content-type"];
            //     unsigned int boundaryIdx =0;
            //     if(Content_Type.find("multipart/form-data")!=std::string::npos&&
            //     (boundaryIdx = Content_Type.find("boundary"))!=std::string::npos){
            //         std::string boundary = Content_Type.substr(boundaryIdx+9,Content_Type.size()-(boundaryIdx+9));
            //     }
            // }
                

            // m_iovec[0].iov_base = &(*m_outbuff.begin());
            // m_iovec[0].iov_len = m_outbuff.size();
            // m_iovec[1].iov_base = m_file_ptr;
            // m_iovec[1].iov_len = m_file_size;
            // // DONE: 是否需要记录要写的总的字节数？答：应该不需要
            // return true;
            break;
        }
        default:
            return false;
    }
    m_iovec[0].iov_base = &(*m_outbuff.begin());
    m_iovec[0].iov_len = m_outbuff.size();
    m_iovec[1].iov_base = m_file_ptr; // FIXME: 需要测试
    if(code !=JSON_REQUEST){
        m_iovec[1].iov_len = m_file_size; // FIXME: 需要测试
    }else{
        m_iovec[1].iov_len = 0;
    }
    // DONE: 是否需要记录需要写的总的字节数？答：不需要
    return true;
}

LINE_STATE HttpConn::parseLine(){
    char tmp;
    int size = m_inbuff.size();
    // m_read_idx 需要在解析完整一行后置为0
    // m_read_idx 在 LINE_OK 状态下，代表一行结尾的下一个位置
    for(; m_read_idx < size; m_read_idx++){
        tmp = m_inbuff[m_read_idx];
        if(tmp == '\r'){
            if(m_read_idx + 1 >= size){
                return LINE_AGAIN;
            }
            else if(m_inbuff[m_read_idx + 1] == '\n'){
                m_read_idx += 2;
                return LINE_OK;
            }
            else{
                return LINE_BAD;
            }
        }
        else if(tmp == '\n'){
            if(m_read_idx > 0 && m_inbuff[m_read_idx - 1] == '\r'){
                m_read_idx += 1;
                return LINE_OK;
            }
            else return LINE_BAD;
        }
    }
    return LINE_AGAIN;
}


HTTP_CODE HttpConn::analyseRequest(){
    LINE_STATE line_state = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    while((m_check_state == CHECK_STATE_CONTENT && line_state == LINE_OK)
        || ((line_state = parseLine()) == LINE_OK)){
        switch(m_check_state){
            case CHECK_STATE_REQUESTLINE:
                ret = parseRequestLine();
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            case CHECK_STATE_HEADER:
                ret = parseHeaders();
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                else if(ret == GET_REQUEST) // 读取完所有首部了,开始处理
                    return doRequest(); // doRequest返回什么
                break;
            case CHECK_STATE_CONTENT:
                ret = parseContent(); // 两个HTTP报文怎么分割？没有分割符号
                if(ret == GET_REQUEST)
                    return doRequest();
                // 若剩下的数据不够组成content，那么下一次循环会调用parseLine()函数，
                // 调用parseLine导致m_read_idx移动到最后面
                // 下一次数据达到，由于处于CHECK_STATE_CONTENT状态，且line_state = LINE_OK
                // 所以switch到达这个case
                // 获取到合适的content
                // m_inbuff应该裁剪掉相应的已经被使用部分
                // 此时m_read_idx应该被复位
                line_state = LINE_AGAIN;
                break;
            default:
                LOG_ERROR("HttpConn::analyseRequest() Unkown CHECK_STATE Error.");
                return INTERNAL_ERROR;
        }
    }
    if(line_state == LINE_BAD)
        return BAD_REQUEST;
    // 读取的数据不够
    else
        return NO_REQUEST;
}

HTTP_CODE HttpConn::parseRequestLine(){
    std::string line = m_inbuff.substr(0, m_read_idx);
    m_inbuff.erase(0, m_read_idx);
    m_read_idx = 0;

    int first_sep_pos = line.find(' ');
    if(first_sep_pos == std::string::npos){
        return BAD_REQUEST;
    }
    // 解析请求方法
    std::string method = line.substr(0, first_sep_pos);
    if(strcasecmp(method.c_str(), "GET") == 0) // strcasecmp忽略大小写比较两个字符串，相等返回0
        m_method = GET;
    else if(strcasecmp(method.c_str(), "HEAD") == 0)
        m_method = HEAD;
    else if(strcasecmp(method.c_str(), "POST") == 0)
        m_method = POST;
    else{
        m_method = OTHERS;
        return NOT_IMPLEMENTED;
    }
    
    int second_sep_pos = line.find(' ', first_sep_pos + 1);
    if(second_sep_pos == std::string::npos){
        return BAD_REQUEST;
    }

    // 解析URI
    m_filename = line.substr(first_sep_pos + 1, second_sep_pos - first_sep_pos - 1);
    if(m_filename == "/")
        m_filename += "index.html";
    else{
        int pos = m_filename.find('?');
        if(pos != std::string::npos)
            m_filename.erase(pos);
    }
 
    // 解析HTTP版本
    std::string version = line.substr(second_sep_pos, 8);
    if(strcasecmp(version.c_str(), "HTTP/1.0")){
        m_version = HTTP10;
    }
    else if(strcasecmp(version.c_str(), "HTTP/1.1")){
        m_version = HTTP11;
    }
    else{
        return BAD_REQUEST;
    }
    // 转移状态
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTP_CODE HttpConn::parseHeaders(){
    std::string header = m_inbuff.substr(0, m_read_idx - 2);
    m_inbuff.erase(0, m_read_idx);
    m_read_idx = 0;

    // 如果是空行
    if(header == ""){
        m_keep_alive = false;
        if(m_headers.find("connection") != m_headers.end()){
            m_keep_alive = (m_headers["connection"] == "close" ? false : true);
        }
        if(m_method == POST){ // 如果是POST方法，还需要接收content
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        else return GET_REQUEST; // 否则得到一个完整的请求
    }
    else{
        int colon_pos = header.find(':');
        if(colon_pos == std::string::npos)
            return BAD_REQUEST;
        std::string key = header.substr(0, colon_pos);
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        std::string value = header.substr(colon_pos + 2);
        transform(value.begin(), value.end(), value.begin(), ::tolower);
        m_headers[key] = value;
        return NO_REQUEST;
    }
}

HTTP_CODE HttpConn::parseContent(){
    if(m_headers.find("content-length") == m_headers.end())
        return BAD_REQUEST;
    int content_len = stoi(m_headers["content-length"]);
    if(m_inbuff.size() >= content_len){
        m_content = m_inbuff.substr(0, content_len);
        m_inbuff.erase(0, content_len); // 清楚前面已经处理的部分
        m_read_idx = 0;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


HTTP_CODE HttpConn::doRequest(){
    // DONE: 根据GET、POST、HEAD请求，做出相应的操作，返回相应的结果，GET、HEAD主要判断文件是否存在
    struct stat st;
    std::string full_filename = m_path;

    if(m_method == POST){
        if(m_filename == "/login.cgi" || m_filename == "/register.cgi"){
            std::stringstream ss(m_content);
            user cUser;
            ss>>json::wrap(cUser);
            if(!cUser.isLegal())
                return BAD_REQUEST;
            // 分为login和register处理
            m_file_category = "text/html";
            if(m_filename == "/login.cgi"){
                if(UserServlet::getInstance().login(cUser)){
                    m_filename = "/success.html";
                }
                else{
                    m_filename = "/login_error.html";
                }
            }
            else if(m_filename == "/register.cgi"){
                if(UserServlet::getInstance().login(cUser)){
                    m_filename = "/register_error.html";
                }
                else{
                    if(UserServlet::getInstance().regis(cUser)){
                        m_filename = "/success.html";   
                    }
                    else{
                        LOG_ERROR("HttpConn::doRequest() SQL insert Error.");
                    }
                }
            }
            full_filename += m_filename;
            int ret = stat(full_filename.c_str(), &st);//根据文件名获取文件状态
            if(ret < 0) return NO_RESOURCE;
            if(!(st.st_mode & S_IROTH)) return FORBIDDEN_REQUEST; // 没有权限
            if(S_ISDIR(st.st_mode)) return BAD_REQUEST; // 请求的是一个目录
            m_file_size = st.st_size;
        }
        else if(m_filename == "/getBookInfo"){
            //Bookservlet
            m_version = HTTP11;
            m_file_category = mime[".json"];
            std::stringstream ss(m_content);
            Page page;
            ss>>json::wrap(page);
            if(!page.judge())
                return BAD_REQUEST;
            BookServlet::getInstance().getBookInfo(page);
            if(!page.judge())
                return BAD_REQUEST;
            ss.str("");
            ss << json::wrap(page);
            m_content_json = ss.str()+"\r\n";
            return JSON_REQUEST;
        }
        else return BAD_REQUEST;
    }
    else if(m_method == GET || m_method == HEAD){
        full_filename += m_filename;  
        
        int ret = stat(full_filename.c_str(), &st);
        if(ret < 0) return NO_RESOURCE; // 不存在这个文件
        if(!(st.st_mode & S_IROTH)) return FORBIDDEN_REQUEST; // 没有权限
        if(S_ISDIR(st.st_mode)) return BAD_REQUEST; // 请求的是一个目录

        // DONE: 正常的请求, HEAD方法需要Content-Len吗？应该要？
        m_file_size = 0;
        if(m_method == HEAD) return FILE_REQUEST;

        m_file_size = st.st_size;

        int dot_pos = m_filename.rfind(".");
        if(dot_pos == std::string::npos){
            m_file_category = mime["default"];
        }
        else{
            m_file_category = mime[m_filename.substr(dot_pos)];
        }
    }
    else{
        return NOT_IMPLEMENTED;
    }
    
    int fd = open(full_filename.c_str(), O_RDONLY);
    if(fd < 0){
        LOG_ERROR("HttpConn::doRequest() open file Error.");
        return INTERNAL_ERROR;
    } 
    m_file_ptr = static_cast<char*>(mmap(0, m_file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if(m_file_ptr == (char*)(-1)){
        munmap((void*)m_file_ptr, m_file_size);
        m_file_ptr = nullptr; // 避免野指针
        m_file_size = 0;
        LOG_ERROR("HttpConn::doRequest() mmap Error.");
        return INTERNAL_ERROR;
    }
    close(fd);
    return FILE_REQUEST;
}

// 设置对应的Timer的delete标志为true，设置对应的Callback为nullptr
// 设置m_timer成员reset
void HttpConn::reset(){
    // 其他需要在一次请求的完成后清空的内容，注意是请求完成后
    m_filename.clear();
    m_headers.clear();
    m_keep_alive = false; // DONE: 哪里能够设置这个？以及文件类型？ keep_alive在读完header后分析， 文件类型在do_request()处设置
    m_file_category = "text/html";
    m_version = HTTP11; 
    m_method = GET; 
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_content.clear();
    m_error_exist = false;
    if(m_file_ptr != nullptr){ // munmap？
        munmap(m_file_ptr, m_file_size);
        m_file_ptr = nullptr;
        m_file_size = 0;
    }
    m_file_size = 0;
    m_iovec[0].iov_base = nullptr;
    m_iovec[0].iov_len = 0;
    m_iovec[1].iov_base = nullptr;
    m_iovec[1].iov_len = 0;
    // DONE: 可能还需要添加
    seperateTimer(); // 分离计时器
}

void HttpConn::seperateTimer(){
    if(m_timer){
        LOG_INFO("HttpConn::seperateTimer() Seperate the timer from httpConn.");
        m_timer->setDeleted();
        m_timer.reset();
    }
}


void HttpConn::add_status_line(HTTP_CODE code){
    m_outbuff += (m_version == HTTP10 ? "HTTP/1.0 " : "HTTP/1.1 ")
                 + std::to_string(status_code[code]) + " " + status_desc[code] + "\r\n";
}


void HttpConn::add_headers(HTTP_CODE code){
    if(code == JSON_REQUEST){
        m_outbuff += "Content-Length: " + std::to_string(m_content_json.length()) + 
                 "\r\nContent-Type: " + m_file_category + "\r\nConnection: " +
                 (m_keep_alive ? "keep-alive" : "close") + 
                 "\r\nServer: VictorServer\r\n\r\n";
    }else if(code ==FILE_REQUEST){ // 记得空行
        m_outbuff += "Content-Length: " + std::to_string(m_file_size) + 
                 "\r\nContent-Type: " + m_file_category + "\r\nConnection: " +
                 (m_keep_alive ? "keep-alive" : "close") + 
                 "\r\nServer: VictorServer\r\n\r\n";
    }else{
        m_outbuff += "Content-Length: " + std::to_string(response_content[code].size()) +
                "\r\nContent-Type: text/html\r\nConnection: close\r\nServer: VictorServer\r\n\r\n";

    }
}


void HttpConn::add_content(HTTP_CODE code){
    if(code ==JSON_REQUEST){
        //这里提供对返回json数据的支持
        m_outbuff +=m_content_json;
        return;
    }else{
        m_outbuff += response_content[code];
    }
}



