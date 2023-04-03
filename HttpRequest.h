#pragma once
#include"Buffer.h"
#include"HttpResponse.h"
#include<string>
#include<map>
#include<functional>
using namespace std;


// 当前的解析状态
enum class PrecessState:char
{
    ParseReqLine,
    ParseReqHeaders,
    ParseReqBody,
    ParseReqDone
};
// 定义http请求结构体
class HttpRequest
{
public:
    HttpRequest();
    //重置
    void reset();
    //添加请求头
    void addHeader(const string key,const string value);
    //根据key得到请求头的value
    string getHeader(const string key);
    //解析请求行
    bool parseRequestLine(Buffer* readBuf);
    //解析请求头
    bool parseRequestHeader(Buffer* readBuf);
    // 解析http请求协议
    bool parseRequest(Buffer* readBuf,HttpResponse* response,Buffer* sendBuf,int socket);
    // 处理http请求协议
    bool processHttpRequest(HttpResponse* response);
    //解码
    //to存储解码之后的数据，传出参数，from被解码的数据，传入参数
    string decodeMsg(string from);
    const string getFiletype(const string name);
    static void sendDir(string dirName,Buffer* sendBuf,int cfd);
    static void sendFile(string dirName,Buffer* sendBuf,int cfd);
    inline void setMethod(string method)
    {
        m_method=method;
    }
    inline void seturl(string url)
    {
        m_url=url;
    }
    inline void setVersion(string version)
    {
        m_version=version;
    }
    //获取处理状态
    inline PrecessState getState()
    {
        return m_curState;
    }
    inline void setState(PrecessState state)
    {
        m_curState=state;
    }
private:
    char* splitRequestLine(const char* start,const char* end,const char* sub,function<void(string)> callback);
    int hexToDec(char c);
private:
    // 请求行
    string m_method;
    string m_url;
    string m_version;
    map<string,string>m_reqHeaders;
    PrecessState m_curState;
};