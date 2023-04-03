#include<stdio.h>
#include<strings.h>
#include<string.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<dirent.h>
#include<fcntl.h>
#include<unistd.h>
#include"HttpRequest.h"
#include<assert.h>
#include<ctype.h>

int HttpRequest::hexToDec(char c)
{
    if(c>='0' && c<='9')
        return c-'0';
    if(c>='a' && c<='f')
        return c-'a'+10;
    if(c>='A' && c<='F')
        return c-'A'+10;
    return 0;
}

HttpRequest::HttpRequest()
{
    reset();
}

void HttpRequest::reset()
{
    m_curState=PrecessState::ParseReqLine;
    m_method=m_url=m_version="";
    m_reqHeaders.clear();
}

void HttpRequest::addHeader(const string key,const string value)
{
    if(key.empty() || value.empty())
    {
        return;
    }
    m_reqHeaders.insert(make_pair(key,value));
}

string HttpRequest::getHeader(const string key)
{
    auto item=m_reqHeaders.find(key);
    if(item==m_reqHeaders.end())
    {
        return "";
    }
    return item->second;
}

char* HttpRequest::splitRequestLine(const char* start,const char* end,const char* sub,function<void(string)> callback)
{
    char* space=(char*)end;
    if(sub!=nullptr)
    {
        space=(char*)memmem(start,end-start,sub,strlen(sub));
        assert(space!=nullptr);
    }
    int length=space-start;
    callback(string(start,length));
    return space+1;
}

bool HttpRequest::parseRequestLine(Buffer* readBuf)
{
    //读出请求行，保存字符串结束地址
    char* end=readBuf->findCRLF();
    //保存字符串起始地址
    char* start=readBuf->data();
    //请求行总长度
    int lineSize=end-start;
    if(lineSize>0)
    {
        auto methodFunc=bind(&HttpRequest::setMethod,this,placeholders::_1);
        start=splitRequestLine(start,end," ",methodFunc);
        auto urlFunc=bind(&HttpRequest::seturl,this,placeholders::_1);
        start=splitRequestLine(start,end," ",urlFunc);
        auto versionFunc=bind(&HttpRequest::setVersion,this,placeholders::_1);
        splitRequestLine(start,end,nullptr,versionFunc);
        //为解析请求头做准备
        readBuf->readPosIncrease(lineSize+2);
        //修改状态
        setState(PrecessState::ParseReqHeaders);
        return true;
    }  
    return false;
}

bool HttpRequest::parseRequestHeader(Buffer* readBuf)
{
    char* end=readBuf->findCRLF();
    if(end!=nullptr)
    {
        char* start=readBuf->data();
        int lineSize=end-start;
        // 基于: 搜索字符串
        char* middle=(char*)memmem(start,lineSize,": ",2);
        if(middle!=nullptr)
        {
            int keyLen=middle-start;
            int valueLen=end-middle-2;
            if(keyLen>0 && valueLen>0)
            {
                string key(start,keyLen);
                string value(middle+2,valueLen);
                addHeader(key,value);
            }
            //移动读数据的位置
            readBuf->readPosIncrease(lineSize+2);
        }
        else
        {
            // 请求头被解析完了，跳过空行
            readBuf->readPosIncrease(2);
            // 修改解析状态
            // 忽略post请求，按照get请求处理
            setState(PrecessState::ParseReqDone);
        }
        return true;
    }
    return false;
}

bool HttpRequest::parseRequest(Buffer* readBuf,HttpResponse* response,Buffer* sendBuf,int socket)
{
    bool flag=true;
    while(m_curState!=PrecessState::ParseReqDone)
    {
        switch(m_curState)
        {
            case PrecessState::ParseReqLine:
                flag=parseRequestLine(readBuf);
                break;
            case PrecessState::ParseReqHeaders:
                flag=parseRequestHeader(readBuf);
                break;
            case PrecessState::ParseReqBody:
                break;
            default:
                break;
        }
        if(!flag)
        {
            return flag;
        }
        // 判断是否解析完毕了，如果完毕了，需要准备回复的数据
        if(m_curState==PrecessState::ParseReqDone)
        {
            //1.根据解析出的原始数据，对客户端的请求做出处理
            processHttpRequest(response);   
            //2.请求响应数据并发送给客户端
            response->prepareMsg(sendBuf,socket);
        }
    }
    m_curState=PrecessState::ParseReqLine; //状态还原，保证还能继续处理第二条及以后的请求
    return flag;
}

bool HttpRequest::processHttpRequest(HttpResponse* response)
{
    if(strcasecmp(m_method.data(),"get")!=0)
    {
        return -1;
    }
    m_url=decodeMsg(m_url);
    // 处理客户端请求的静态资源（目录或者文件）
    const char* file=NULL;
    if(strcmp(m_url.data(),"/")==0)
    {
        file="./";
    }
    else
    {
        file=m_url.data()+1;
    }
    //获取文件的属性
    struct stat st;
    int ret=stat(file,&st);
    if(ret==-1)
    {
        //文件不存在 -- 回复404
        // sendHeadMsg(cfd,404,"Not Found",getFiletype(".html"),-1);
        // sendFile("404.html",cfd);
        response->setFileName("404.html");
        response->setStatusCode(StatusCode::NotFound);
        //响应头
        response->addHeader("Content-type",getFiletype(".html"));
        response->sendDataFunc=sendFile;
        return 0;
    }
    response->setFileName(file);
    response->setStatusCode(StatusCode::OK);
    //判断文件的类型
    if(S_ISDIR(st.st_mode))
    {
        // 把这个目录中的内容发送给客户端
        // sendHeadMsg(cfd,200,"OK",getFiletype(".html"),-1);
        // sendDir(file,cfd);
        //响应头
        response->addHeader("Content-type",getFiletype(".html"));
        response->sendDataFunc=sendDir;
    }
    else
    {
        // 把文件的内容发送给客户端
        // sendHeadMsg(cfd,200,"OK",getFiletype(file),st.st_size);
        // sendFile(file,cfd);
        //响应头
        response->addHeader("Content-type",getFiletype(file));
        response->addHeader("Content-length",to_string(st.st_size));
        response->sendDataFunc=sendFile;
    }
}

string HttpRequest::decodeMsg(string msg)
{
    string str="";
    const char* from=msg.data();
    for(;*from!='\0';++from)
    {
        // isxdigit -> 判断字符是不是16进制格式，取值在0-f
        // Linux%E5%86%85%E6%A0%B8.jpg -> Linux内核.jpg
        if(from[0]=='%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            // 将16进制的数 -> 十进制 将这个数只赋值给了字符 int -> char
            // R2==178
            // 将3个字符，变成了一个字符，这个字符就是原始数据
            str.append(1,hexToDec(from[1])*16+hexToDec(from[2]));
            
            // 跳过from[1]和from[2] 因为在当前循环中已经处理过了
            from+=2;
        }
        else
        {
            //字符拷贝，赋值
            str.append(1,*from);
        }
    }
    str.append(1,'\0');
    return str;
}

const string HttpRequest::getFiletype(const string name)
{
    //a.jpg a.mp4 a.html
    //自右向左查找 '.' 字符，如不存在返回NULL
    const char* dot=strrchr(name.data(),'.'); //从右往左找
    if(dot==NULL)
        return "text/plain; charset=utf-8"; //纯文本
    if(strcmp(dot,".html")==0||strcmp(dot,".htm")==0)
        return "text/html; charset=utf-8";
    if(strcmp(dot,".jpg")==0||strcmp(dot,".jpeg")==0)
        return "image/jpeg";
    if(strcmp(dot,".gif")==0)
        return "image/gif";
    if(strcmp(dot,".png")==0)
        return "image/png";
    if(strcmp(dot,".css")==0)
        return "text/css";
    if(strcmp(dot,".au")==0)
        return "audio/basic";
    if(strcmp(dot,".wav")==0)
        return "audio/wav";
    if(strcmp(dot,".avi")==0)
        return "video/x-msvideo";
    if(strcmp(dot,".mov")==0||strcmp(dot,".qt")==0)
        return "video/quicktime";
    if(strcmp(dot,".mpeg")==0||strcmp(dot,".mpe")==0)
        return "video/mpeg";
    if(strcmp(dot,".vrml")==0||strcmp(dot,".wrl")==0)
        return "model/vrml";
    if(strcmp(dot,".midi")==0||strcmp(dot,".mid")==0)
        return "audio/midi";
    if(strcmp(dot,".mp3")==0)
        return "audio/mpeg";
    if(strcmp(dot,".ogg")==0)
        return "application/ogg";
    if(strcmp(dot,".pac")==0)
        return "application/x-ns-proxy-autoconfig";
    return "text/plain; charset=utf-8";
}

void HttpRequest::sendDir(string dirName,Buffer* sendBuf,int cfd)
{
    char buf[4096]={0};
    sprintf(buf,"<html><head><title>%s</title></head><body><table>",dirName.data());
    struct dirent** namelist;
    int num=scandir(dirName.data(),&namelist,NULL,alphasort);
    for(int i=0;i<num;i++)
    {
        //取出文件名 namelist指向的是一个指针数组 struct dirent* tmp[]
        char *name=namelist[i]->d_name;
        struct stat st;
        char subPath[1024]={0};
        sprintf(subPath,"%s/%s",dirName.data(),name);
        stat(subPath,&st);
        if(S_ISDIR(st.st_mode))
        {
            //a标签 <a href="">name</a>
            sprintf(buf+strlen(buf),
                "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                name,name,st.st_size);
        }
        else
        {
            sprintf(buf+strlen(buf),
                "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                name,name,st.st_size);
        }
        // send(cfd,buf,strlen(buf),0);
        sendBuf->appendString(buf);
#ifndef MSG_SEND_AUTO
        sendBuf->sendData(cfd);
#endif
        memset(buf,0,sizeof(buf));
        free(namelist[i]);
    }
    sprintf(buf,"</table></body></html>");
    // send(cfd,buf,strlen(buf),0);
    sendBuf->appendString(buf);
#ifndef MSG_SEND_AUTO
    sendBuf->sendData(cfd);
#endif
    free(namelist);
}

void HttpRequest::sendFile(string fileName,Buffer* sendBuf,int cfd)
{
    //1.打开文件
    int fd=open(fileName.data(),O_RDONLY);
    assert(fd>0);

    while(1)
    {
        char buf[1024];
        int len=read(fd,buf,sizeof(buf));
        if(len>0)
        {
            // send(cfd,buf,len,0);
            sendBuf->appendString(buf,len);
#ifndef MSG_SEND_AUTO
            sendBuf->sendData(cfd);
#endif
        }
        else if(len==0)
        {
            break;
        }
        else
        {
            close(fd);
            perror("read");
        }
    }
    close(fd);
}