#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <string>
#include <map>
#include <functional>

// 前置声明
class EventLoop;
class TcpConnection;
class Channel;

// TcpServer: 管理整个TCP服务器
class TcpServer {
public:
    TcpServer(EventLoop* loop, int port);
    ~TcpServer();
    
    // 启动服务器
    void Start();
    
    // 设置回调（业务层调用）
    void SetMessageCallback(std::function<void(TcpConnection*)> cb);
    
private:
    // 处理新连接（listen_fd可读时调用）
    void HandleNewConnection();
    
    // 处理连接关闭（TcpConnection的closeCallback）
    void HandleConnectionClose(TcpConnection* conn);
    
private:
    EventLoop* loop_;         // EventLoop
    int listen_fd_;           // 监听socket
    int port_;                // 端口号
    Channel* listen_channel_; // listen_fd的Channel
    
    // 所有连接（key: client_fd, value: TcpConnection*）
    std::map<int, TcpConnection*> connections_;
    
    // 业务回调（所有连接共享）
    std::function<void(TcpConnection*)> messageCallback_;
};

#endif

