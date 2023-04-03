#include<stdio.h>
#include"Channel.h"
#include<stdlib.h>

Channel::Channel(int fd,FDEvent events,handleFunc readFunc,handleFunc writeFunc,handleFunc destoryFunc,void* arg)
{
    m_arg=arg;
    m_fd=fd;
    m_events=(int)events;
    readCallback=readFunc;
    writeCallback=writeFunc;
    destroyCallback=destoryFunc;
}

void Channel::writeEventEnable(bool flag)
{
    if(flag)
    {
        m_events|=(int)FDEvent::WriteEvent;
    }
    else
    {
        m_events=m_events&(~(int)FDEvent::WriteEvent);
    }
}

bool Channel::isWriteEventEnable()
{
    return m_events&(int)FDEvent::WriteEvent;
}