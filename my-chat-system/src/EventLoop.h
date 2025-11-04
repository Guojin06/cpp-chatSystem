#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <sys/epoll.h>
#include <map>
#include "Channel.h"

// EventLoop: 事件循环
// 作用：管理epoll，分发事件到各个Channel
class EventLoop {
private:
    int epfd_;  // epoll文件描述符
    bool looping_;  // 是否在循环中
    
    // fd到Channel的映射
    // key: fd, value: Channel*
    std::map<int, Channel*> channels_;
    
    // epoll_wait返回的事件数组
    struct epoll_event events_[128];
    
public:
    EventLoop();
    ~EventLoop();
    
    // 事件循环
    void Loop();
    
    // 添加/删除Channel
    void AddChannel(Channel* ch);
    void RemoveChannel(Channel* ch);
    
    // 更新Channel（修改关注的事件）
    void UpdateChannel(Channel* ch);
};

#endif

