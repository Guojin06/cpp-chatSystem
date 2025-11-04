#include "Channel.h"
#include <stdio.h>

Channel::Channel(int fd) 
    : fd_(fd), events_(0), revents_(0) {
}

Channel::~Channel() {
    // fd由Socket管理，Channel不负责关闭
}

void Channel::HandleEvent() {
    // 先处理错误和关闭，再处理正常读写
    
    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }
    
    if (revents_ & (EPOLLHUP | EPOLLRDHUP)) {
        if (closeCallback_) {
            closeCallback_();
        }
    }
    
    if (revents_ & EPOLLIN) {
        if (readCallback_) {
            readCallback_();
        }
    }
    
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }
}
