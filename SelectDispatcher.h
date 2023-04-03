#pragma once
#include"Dispatcher.h"
#include<sys/select.h>
#include<string>
using namespace std;

class SelectDispatcher:public Dispatcher
{
public:
    SelectDispatcher(EventLoop* evloop);
    ~SelectDispatcher();
    // 添加
    int add() override;
    // 删除
    int remove() override;
    // 修改
    int modify() override;
    // 事件检测
    int dispatch(int timeout=2) override; //单位 s
private:
    void setFdset();
    void clearFdset();
private:
    fd_set m_readSet;
    fd_set m_writeSet;
    const int m_maxSize=1024;
};