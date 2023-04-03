#pragma once
#include"Dispatcher.h"
#include<thread>
#include"Channel.h"
#include<queue>
#include<map>
#include<string>
#include<mutex>
using namespace std;

//处理该节点中的channel方式
enum class ElemType:char{ADD,DELETE,MODIFY};
// 定义任务队列的节点
struct ChannelElement
{
    ElemType type;
    Channel* channel;
};
class Dispatcher;

class EventLoop
{
public:
    EventLoop();
    EventLoop(const string threadName);
    ~EventLoop();
    //启动反应堆模型
    int run();
    //处理激活的文件fd
    int eventActive(int fd,int event);
    //添加任务到任务队列
    int addTask(Channel* channel,ElemType type);
    //处理队列中的任务
    int processTaskQ();
    //处理dispatchaer中的节点
    int add(Channel* channel);
    int remove(Channel* channel);
    int modify(Channel* channel);
    //释放channel
    int freeChannel(Channel* channel);
    int readMessage();
    // 返回线程ID
    inline thread::id getThreadID()
    {
        return m_threadID;
    }
private:
    void taskWakeup();
private:
    bool m_isQuit;
    // 该指针指向了子类的实例 epoll poll select
    Dispatcher* m_dispatcher;
    // 任务队列
    queue<ChannelElement*>m_taskQ;
    // map
    map<int,Channel*> m_channelMap;
    // 线程id，name，mutex
    thread::id m_threadID;
    string m_threadName;
    mutex m_mutex;
    int m_socketPair[2]; //存储本地通信fd 通过socketpair初始化
};