#pragma once
#include<string>
#include<vector>
#include "../configor/json.hpp"
using namespace configor;
enum RESOURCE_TYPE{
    TEXT = 0,
    VIDEO,
    PIC,
    AUDIO 
};
enum JSON_TYPE{
    TEMPLATE =0,
    PAGE
};
struct Tmplate
{
    int templateid;//PRIMARYKEY
    int userid;
    std::string positionlist;
    std::string t_updatetime;
    int tp;
    CONFIGOR_BIND(json, Tmplate, OPTIONAL(userid), OPTIONAL(templateid),
    OPTIONAL(t_updatetime),OPTIONAL(positionlist),OPTIONAL(tp));
    bool judge();
};
struct resource
{
    int resourceid;
    int userid;
    std::string url;
    int tp;
    std::string r_updatetime;
    CONFIGOR_BIND(json, resource, OPTIONAL(resourceid), OPTIONAL(userid),
    REQUIRED(url),OPTIONAL(r_updatetime),OPTIONAL(tp));
    bool judge();
};

struct Page:public Tmplate
{
    int pageid;//PRIMARYKEY
    std::vector<int> urllist;
    std::string p_updatetime;
    CONFIGOR_BIND(json, Page, OPTIONAL(userid), OPTIONAL(templateid),
    OPTIONAL(t_updatetime),OPTIONAL(positionlist),OPTIONAL(pageid),OPTIONAL(urllist),
    OPTIONAL(p_updatetime),OPTIONAL(tp));
    bool judge();
    void seturllist(std::string str);
};

struct Book
{
    int bookid;//PRIMARYKEY
    int userid;
    std::vector<int> pageidlist;
    std::string b_updatetime;
    CONFIGOR_BIND(json, Book, OPTIONAL(userid), OPTIONAL(bookid),
    OPTIONAL(b_updatetime),OPTIONAL(pageidlist));
    bool judge();
};
