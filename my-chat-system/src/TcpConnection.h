#ifndef TCPCONNECTION_H
#define TCPCONNECTION_H

#include <string>
#include <functional>

// 前置声明
class EventLoop;
class Channel;

// TcpConnection: 管理一个TCP连接
class TcpConnection {
public:
    TcpConnection(int fd, EventLoop* loop);
    
    ~TcpConnection();
    
    // 公开接口（给业务层用）
    
    // 发送数据
    void Send(const std::string& msg);
    
    // 设置回调函数
    void SetMessageCallback(std::function<void(TcpConnection*)> cb);
    //回调参数是TcpConnection*，因为TcpConnection是管理一个TCP连接的类，所以需要传递TcpConnection*
    
    void SetCloseCallback(std::function<void(TcpConnection*)> cb);
    
    // 获取读缓冲区（业务层读取数据用）
    const std::string& GetReadBuf() const { return readBuf_; }
    
    // 清空读缓冲区
    void ClearReadBuf() { readBuf_.clear(); }
    
private:
    // 内部方法（给Channel回调用）
    void HandleRead();    // fd可读
    void HandleWrite();   // fd可写
    void HandleClose();   // fd关闭
    void HandleError();   // fd出错
    
private:
    // 成员变量
    int fd_;                    // socket文件描述符
    EventLoop* loop_;           // 所属的EventLoop
    Channel* channel_;          // 对应的Channel
    
    std::string readBuf_;       // 读缓冲区
    std::string writeBuf_;      // 写缓冲区
    
    // 回调函数（业务层设置）
    std::function<void(TcpConnection*)> messageCallback_;  // 收到消息
    std::function<void(TcpConnection*)> closeCallback_;    // 连接关闭
};

#endif