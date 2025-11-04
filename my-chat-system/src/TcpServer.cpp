#include "TcpServer.h"
#include "EventLoop.h"
#include "TcpConnection.h"
#include "Channel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <functional>

// 构造函数
TcpServer::TcpServer(EventLoop* loop, int port)
    : loop_(loop), listen_fd_(-1), port_(port), listen_channel_(nullptr) {
    
    // 第1步：创建listen_fd
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        perror("socket error");
        return;
    }
    
    // 第2步：设置SO_REUSEADDR（避免TIME_WAIT状态占用端口）
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 第3步：bind绑定端口
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
    addr.sin_port = htons(port_);
    
    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind error");
        close(listen_fd_);
        return;
    }
    
    // 第4步：listen开始监听
    if (listen(listen_fd_, 128) < 0) {
        perror("listen error");
        close(listen_fd_);
        return;
    }
    
    printf("TcpServer listening on port %d\n", port_);
}

// 析构函数
TcpServer::~TcpServer() {
    // 第1步：删除所有TcpConnection
    for (auto& pair : connections_) {
        delete pair.second;  // 删除TcpConnection对象
    }
    connections_.clear();
    
    // 第2步：删除listen_channel
    if (listen_channel_) {
        loop_->RemoveChannel(listen_channel_);
        delete listen_channel_;
    }
    
    // 第3步：关闭listen_fd
    if (listen_fd_ >= 0) {
        close(listen_fd_);
    }
}

// 启动服务器
void TcpServer::Start() {
    // 第1步：创建listen_fd的Channel
    listen_channel_ = new Channel(listen_fd_);
    
    // 第2步：设置readCallback
    // 当listen_fd可读时（有新连接），调用HandleNewConnection
    listen_channel_->SetReadCallback(
        std::bind(&TcpServer::HandleNewConnection, this)
    );
    
    // 第3步：启用读事件
    listen_channel_->EnableReading();
    
    // 第4步：注册到EventLoop
    loop_->AddChannel(listen_channel_);
    
    printf("TcpServer started, waiting for connections...\n");
}

// 处理新连接
void TcpServer::HandleNewConnection() {
    // 第1步：accept新连接
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &len);
    
    if (client_fd < 0) {
        perror("accept error");
        return;
    }
    
    // 打印连接信息
    char ip[16];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    printf("New connection from %s:%d, fd=%d\n", 
           ip, ntohs(client_addr.sin_port), client_fd);
    
    // 第2步：创建TcpConnection
    TcpConnection* conn = new TcpConnection(client_fd, loop_);
    
    // 第3步：设置TcpConnection的回调
    // 设置消息回调（业务层设置的）
    if (messageCallback_) {
        conn->SetMessageCallback(messageCallback_);
    }
    
    // 设置关闭回调（TcpServer自己处理）
    conn->SetCloseCallback(
        std::bind(&TcpServer::HandleConnectionClose, this, std::placeholders::_1)
    );
    // std::placeholders::_1 表示传递第1个参数（TcpConnection*）
    
    // 第4步：保存到connections_
    connections_[client_fd] = conn;
    
    printf("Total connections: %lu\n", connections_.size());
}

// 处理连接关闭
void TcpServer::HandleConnectionClose(TcpConnection* conn) {
    // 第1步：从connections_删除
    // 需要遍历map找到这个conn
    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        if (it->second == conn) {
            printf("Connection closed, fd=%d\n", it->first);
            
            // 删除TcpConnection对象
            delete conn;
            
            // 从map删除
            connections_.erase(it);
            
            printf("Total connections: %lu\n", connections_.size());
            break;
        }
    }
}

// 设置消息回调
void TcpServer::SetMessageCallback(std::function<void(TcpConnection*)> cb) {
    messageCallback_ = cb;
}

