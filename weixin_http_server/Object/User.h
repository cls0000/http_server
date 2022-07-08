#pragma once
#include<string>
#include<sstream>
#include<iostream>
#include "../configor/json.hpp"
using namespace configor;
struct user{
    int id;
    std::string userName;
    std::string passWord;
    user():id(-1),userName("none"),passWord(""){}
    CONFIGOR_BIND(json, user, OPTIONAL(id), OPTIONAL(userName),OPTIONAL(passWord)); 
    bool isLegal();   
};