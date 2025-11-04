#include "EventLoop.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

EventLoop::EventLoop() 
    : epfd_(-1), looping_(false) {
    epfd_ = epoll_create(1);
    if (epfd_ < 0) {
        perror("epoll_create");
    }
}

EventLoop::~EventLoop() {
    if (epfd_ >= 0) {
        close(epfd_);
    }
}
//AddChannel:把一个Channel注册到EventLoop，让epoll监听它的fd
void EventLoop::AddChannel(Channel* ch) {
    // 步骤：
    // 1. 获取fd和events
    int fd = ch->GetFd();
    int events = ch->GetEvents();

    // 2. 保存到channels_这个map中
    channels_[fd] = ch;//key是fd，value是Channel*
    // 3. 构造epoll_event结构体
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = ch;//把Channel*存到epoll_event的data.ptr中
    // 4. 调用epoll_ctl添加到epoll
    epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    
}
//RemoveChannel:从EventLoop中删除一个Channel
void EventLoop::RemoveChannel(Channel* ch) {
    // 步骤：
    // 1. 获取fd
    int fd = ch->GetFd();
    // 2. 从channels_中删除
    channels_.erase(fd);
    // 3. 调用epoll_ctl从epoll中删除
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, NULL);
}
//UpdateChannel:Channel的关注事件变了（比如从只读变成读+写），更新epoll
void EventLoop::UpdateChannel(Channel* ch) {
    // 作用：Channel的events_变了，需要更新epoll
    // 步骤：
    // 1. 获取fd和新的events
    int fd = ch->GetFd();
    int events = ch->GetEvents();
    // 2. 构造epoll_event结构体
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = ch;
    // 3. 调用epoll_ctl修改（EPOLL_CTL_MOD）
    epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
}
//Loop:事件循环，等待事件发生，分发事件到各个Channel
void EventLoop::Loop() {
    looping_ = true;
    while (looping_) {
        //1.等待事件（阻塞），直到有事件发生
        int n = epoll_wait(epfd_, events_, 128, -1);//参数
        //events：事件数组
        //128：最大事件数
        //-1：等待时间，-1表示永久阻塞，直到有事件
        //返回值：成功返回就绪事件数，失败返回-1
        //n表示就绪事件数
        //如果n小于0，则表示失败
        if (n < 0) {
            perror("epoll_wait");
            break;
        }
        //2.遍历所有就绪的事件
        for (int i = 0; i < n; i++) {
            //3.拿到channel
            Channel* ch = static_cast<Channel*>(events_[i].data.ptr);//static_cast是类型转换，把void*转换为Channel*
            //4.设置revents_,告诉Channel哪些时间发生了
            ch->SetRevents(events_[i].events);
            //5.调用handleEvent
            ch->HandleEvent();
        }
    }
}
