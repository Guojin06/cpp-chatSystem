// 测试Channel类
#include "Channel.h"
#include <iostream>
#include <sys/epoll.h>

int main() {
    // 创建一个Channel，随便用个fd
    Channel ch(0);
    
    // 设置回调
    ch.SetReadCallback([]() {
        std::cout << "read callback called\n";
    });
    
    ch.SetWriteCallback([]() {
        std::cout << "write callback called\n";
    });
    
    ch.SetErrorCallback([]() {
        std::cout << "error callback called\n";
    });
    
    ch.SetCloseCallback([]() {
        std::cout << "close callback called\n";
    });
    
    // 测试可读
    std::cout << "test EPOLLIN:\n";
    ch.SetRevents(EPOLLIN);
    ch.HandleEvent();
    
    // 测试可写
    std::cout << "\ntest EPOLLOUT:\n";
    ch.SetRevents(EPOLLOUT);
    ch.HandleEvent();
    
    // 测试错误
    std::cout << "\ntest EPOLLERR:\n";
    ch.SetRevents(EPOLLERR);
    ch.HandleEvent();
    
    // 测试关闭
    std::cout << "\ntest EPOLLHUP:\n";
    ch.SetRevents(EPOLLHUP);
    ch.HandleEvent();
    
    return 0;
}
