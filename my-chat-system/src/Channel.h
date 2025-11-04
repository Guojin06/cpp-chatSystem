#ifndef CHANNEL_H
#define CHANNEL_H

#include <sys/epoll.h>
#include <functional>

// Channel: 封装fd + 关注的事件 + 事件回调
// 作用：把fd、事件类型、处理逻辑绑定在一起
class Channel {
private:
    int fd_;        // 这个Channel负责的文件描述符
    int events_;    // 我们想关注什么事件（EPOLLIN/EPOLLOUT）
    int revents_;   // 实际发生了什么事件（epoll_wait返回的）
    
    // 回调函数：当事件发生时要执行的操作
    // std::function可以存储普通函数、lambda表达式等
    std::function<void()> readCallback_;   // fd可读时调用
    std::function<void()> writeCallback_;  // fd可写时调用
    std::function<void()> closeCallback_;  // fd关闭时调用
    std::function<void()> errorCallback_;  // fd出错时调用
    
public:
    Channel(int fd);
    ~Channel();
    
    // 获取信息
    int GetFd() const { return fd_; }
    int GetEvents() const { return events_; }
    void SetRevents(int revents) { revents_ = revents; }
    
    // 设置回调函数
    // 参数cb: callback的缩写，就是回调函数
    // 用法：ch.SetReadCallback([]() { 处理可读事件 });
    void SetReadCallback(std::function<void()> cb) { readCallback_ = cb; }
    void SetWriteCallback(std::function<void()> cb) { writeCallback_ = cb; }
    void SetCloseCallback(std::function<void()> cb) { closeCallback_ = cb; }
    void SetErrorCallback(std::function<void()> cb) { errorCallback_ = cb; }
    
    // 关注/取消关注事件
    void EnableReading() { events_ |= EPOLLIN; }     // 开始关注可读事件
    void EnableWriting() { events_ |= EPOLLOUT; }    // 开始关注可写事件
    void DisableWriting() { events_ &= ~EPOLLOUT; }  // 取消关注可写事件
    void DisableAll() { events_ = 0; }               // 取消所有事件
    
    // 核心函数：根据revents_调用对应的回调
    void HandleEvent();
};

#endif
