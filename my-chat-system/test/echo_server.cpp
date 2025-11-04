#include "EventLoop.h"
#include "TcpServer.h"
#include "TcpConnection.h"
#include <stdio.h>
#include <string>

// 业务层回调：收到消息时的处理
void OnMessage(TcpConnection* conn) {
    // 从readBuf_获取数据
    std::string msg = conn->GetReadBuf();
    
    if (msg.empty()) {
        return;  // 没有数据，直接返回
    }
    
    printf("Received: %s", msg.c_str());
    
    // Echo回去
    conn->Send(msg);
    
    // 重要：清空readBuf_，避免重复处理
    conn->ClearReadBuf();
}

int main() {
    printf("========================================\n");
    printf("Echo Server - Reactor Pattern\n");
    printf("========================================\n");
    
    // 第1步：创建EventLoop
    EventLoop loop;
    
    // 第2步：创建TcpServer，监听8080端口
    TcpServer server(&loop, 8080);
    
    // 第3步：设置消息回调
    server.SetMessageCallback(OnMessage);
    
    // 第4步：启动服务器
    server.Start();
    
    printf("\n========================================\n");
    printf("Server is running...\n");
    printf("Test with: telnet localhost 8080\n");
    printf("========================================\n\n");
    
    // 第5步：启动事件循环
    loop.Loop();
    
    return 0;
}

