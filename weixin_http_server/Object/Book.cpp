#include"Book.h"
bool Tmplate::judge(){
    return this->positionlist.length()>0;
}
bool resource::judge(){
    return this->url.length()>0;
}
bool Page::judge(){
     return this->urllist.size()>0;
}
bool Book::judge(){
    return this->pageidlist.size()>0;
}
void Page::seturllist(std::string str){
    int i=0,len=str.length();
    urllist.clear();
    while(i<len){
        int j = i;
        while(j<len &&str[j] !='/'){++j;}
        urllist.push_back(std::stoi(str.substr(i,j-i)));
        i = j+1;
    }
}