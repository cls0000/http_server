#include"Servlet.h"
#include "../ConnectionPool/ConnectionPool.h"
#include "../Log/Logger.h"
bool UserServlet::login(user User){
    MYSQL* mysql = nullptr;
    auto mysql_raii = ConnectionRAII(&mysql);
    std::string sql = "SELECT count(*) FROM user WHERE username='" + User.userName + "' and passwd='" + User.passWord + "'";
    if(mysql_query(mysql, sql.c_str())){
        LOG_ERROR("HttpConn::doRequest() SQL select Error.");
        return false;
    }
    // 从表中检索完整的结果集
    MYSQL_RES* result = mysql_store_result(mysql);
    MYSQL_ROW row = mysql_fetch_row(result);
    return bool(atoi(row[0]));
}
bool UserServlet::regis(user User){
    MYSQL* mysql = nullptr;
    auto mysql_raii = ConnectionRAII(&mysql);
    std::string insert_sql = "INSERT INTO user (username,passwd) VALUES('" + User.userName + "', '" + User.passWord + "')";
    if(mysql_query(mysql, insert_sql.c_str())){
        LOG_ERROR("HttpConn::doRequest() SQL select Error.");
        return false;
    }
    return true;
}
UserServlet& UserServlet::getInstance(){
    static UserServlet instance;
    return instance;
}
BookServlet& BookServlet::getInstance(){
    static BookServlet instance;
    return instance;
}
void BookServlet::getBookInfo(Page &page){
    MYSQL* mysql = nullptr;
    auto mysql_raii = ConnectionRAII(&mysql);
    std::string sql = "SELECT * FROM ";
    if(page.tp ==TEMPLATE){
        sql +="template WHERE templateid = '" + std::to_string(page.templateid) +"'";
    }else if(page.tp ==PAGE){
        sql +="page WHERE pageid = '" + std::to_string(page.pageid) +"'";
    }
    if(mysql_query(mysql, sql.c_str())){
        LOG_ERROR("HttpConn::doRequest() SQL select Error.");
        return ;
    }
    // 从表中检索完整的结果集
    MYSQL_RES* result = mysql_store_result(mysql);
    MYSQL_ROW row;
    int num = mysql_num_fields(result);
    if(page.tp ==PAGE){
        while((row = mysql_fetch_row(result))){
            page.userid = std::stoi(std::string(row[1]));
            page.templateid = std::stoi(std::string(row[2]));
            page.seturllist(std::string(row[3]));
            page.p_updatetime = row[4];
        }
    }else if(page.tp ==TEMPLATE){
        while((row = mysql_fetch_row(result))){
            page.userid = std::stoi(std::string(row[1]));
            page.positionlist = row[2];
            page.p_updatetime = row[3];
        }
    }
}