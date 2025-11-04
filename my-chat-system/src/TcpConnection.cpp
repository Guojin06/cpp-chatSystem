#include "TcpConnection.h"
#include "EventLoop.h"
#include "Channel.h"
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <functional>

// 构造函数：初始化一个TCP连接
TcpConnection::TcpConnection(int fd, EventLoop* loop)
    : fd_(fd), loop_(loop), channel_(nullptr) {
    
    // 创建Channel，绑定这个fd
    channel_ = new Channel(fd_);
    
    // 设置Channel的4个回调函数
    // 当epoll检测到事件时，Channel会调用这些回调
    // 使用std::bind把成员函数绑定到this对象
    
    channel_->SetReadCallback(
        std::bind(&TcpConnection::HandleRead, this)
    );
    // 绑定结果：channel_->readCallback_ 指向 this->HandleRead
    
    channel_->SetWriteCallback(
        std::bind(&TcpConnection::HandleWrite, this)
    );
    
    channel_->SetCloseCallback(
        std::bind(&TcpConnection::HandleClose, this)
    );
    
    channel_->SetErrorCallback(
        std::bind(&TcpConnection::HandleError, this)
    );
    
    // 启用读事件监听（EPOLLIN）
    // 这样客户端发来数据时，epoll会通知我们
    channel_->EnableReading();
    
    // 把Channel注册到EventLoop
    // EventLoop会调用epoll_ctl(ADD)把这个fd加入监听
    loop_->AddChannel(channel_);
}

// 析构函数：清理资源
TcpConnection::~TcpConnection() {
    // 第1步：从EventLoop移除Channel
    // 这样epoll不再监听这个fd
    loop_->RemoveChannel(channel_);
    
    // 第2步：关闭socket
    // 释放系统资源，TCP四次挥手
    if (fd_ >= 0) {
        close(fd_);
    }
    
    // 第3步：删除Channel对象
    // 释放内存
    if (channel_) {
        delete channel_;
        channel_ = nullptr;
    }
}

// 发送数据（给业务层调用）
void TcpConnection::Send(const std::string& msg) {
    // 优化：如果writeBuf_为空，先尝试直接发送
    // 大部分情况下socket缓冲区是空的，可以立即发送，不用等EPOLLOUT事件
    if (writeBuf_.empty()) {
        // 尝试直接发送
        int n = send(fd_, msg.data(), msg.size(), 0);//msg.data()是字符串的指针，msg.size()是字符串的长度
        
        if (n >= 0) {
            // 发送成功
            if (n < (int)msg.size()) {
                // 只发送了一部分（socket缓冲区满了）
                // 剩下的数据放到writeBuf_，等待EPOLLOUT事件
                writeBuf_.append(msg.data() + n, msg.size() - n);
                channel_->EnableWriting();  // 启用写事件监听
            }
            // else: 全部发送完成，不需要EnableWriting
        } else {
            // 发送失败（比如EAGAIN/EWOULDBLOCK）
            // 把数据放到writeBuf_，等待EPOLLOUT
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                writeBuf_ = msg;
                channel_->EnableWriting();
            } else {
                // 其他错误
                perror("send error");
            }
        }
    } else {
        // writeBuf_不为空，说明之前有数据没发完
        // 追加到writeBuf_，等待HandleWrite继续发送
        writeBuf_.append(msg);
        channel_->EnableWriting();
    }
}

// HandleRead：处理读事件（Channel回调）
void TcpConnection::HandleRead() {
    // 从socket读数据到readBuf_
    char buf[65536];  // 64KB缓冲区
    int n = recv(fd_, buf, sizeof(buf), 0);
    
    if (n > 0) {
        // 读到数据，追加到readBuf_
        readBuf_.append(buf, n);
        
        // 通知业务层：收到消息了
        // 业务层可以从readBuf_获取数据并处理
        if (messageCallback_) {
            messageCallback_(this);
        }
        
        // 注意：这里简化处理，实际项目中需要：
        // 1. 循环读到EAGAIN（ET模式）
        // 2. 处理粘包半包（长度+内容协议）
    } else if (n == 0) {
        // 对端关闭连接（recv返回0）
        // TCP四次挥手，客户端断开了
        HandleClose();
    } else {
        // 出错（recv返回-1）
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // 不是"暂时无数据"错误，是真的出错了
            HandleError();
        }
    }
}

// HandleWrite：处理写事件（Channel回调）
void TcpConnection::HandleWrite() {
    // 当socket可写时（EPOLLOUT事件），发送writeBuf_的数据
    if (!writeBuf_.empty()) {
        int n = send(fd_, writeBuf_.data(), writeBuf_.size(), 0);
        
        if (n > 0) {
            // 发送成功，删除已发送的部分
            writeBuf_.erase(0, n);
            
            if (writeBuf_.empty()) {
                // 全部发送完成，禁用写事件
                // 避免不必要的EPOLLOUT通知（节省CPU）
                channel_->DisableWriting();
            }
            // else: 还有数据没发完，等待下一次EPOLLOUT
        } else {
            // 发送失败
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("send error in HandleWrite");
            }
        }
    }
}

// HandleClose：处理连接关闭（Channel回调）
void TcpConnection::HandleClose() {
    // 通知业务层：连接关闭了
    // 业务层可以清理资源、删除TcpConnection对象等
    if (closeCallback_) {
        closeCallback_(this);
    }
    
    // 注意：不在这里delete this！
    // 应该由创建者（TcpServer）负责删除
}

// HandleError：处理错误（Channel回调）
void TcpConnection::HandleError() {
    // 打印错误信息
    int err;
    socklen_t len = sizeof(err);
    getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len);//getsockopt是获取socket的错误信息
    printf("TcpConnection error: %s\n", strerror(err));
    
    // 关闭连接
    HandleClose();
}

// 设置消息回调（业务层调用）
void TcpConnection::SetMessageCallback(std::function<void(TcpConnection*)> cb) {
    messageCallback_ = cb;
}

// 设置关闭回调（业务层调用）
void TcpConnection::SetCloseCallback(std::function<void(TcpConnection*)> cb) {
    closeCallback_ = cb;
}
