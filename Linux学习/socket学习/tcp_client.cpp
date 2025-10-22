// tcp_client.cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main() {
    // 1. 创建socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    printf("1. socket创建成功, fd = %d\n", sock);
    
    // 2. 连接服务器（connect）
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;           // IPv4
    server_addr.sin_port = htons(8080);         // 服务器端口
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // 服务器IP
    
    int ret = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));//connect会主动连接服务器，如果连接失败，返回-1
    if (ret < 0) {
        perror("connect");
        return 1;
    }
    printf("2. 连接服务器成功！\n");
    //3.发送消息
     const char* msg = "Hello, Server! 我是客户端！";
     send(sock, msg, strlen(msg), 0);
     printf("3. 发送消息：%s\n", msg);

     //4.接收服务器回复
     char buf[1024];
     ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
     if (n > 0) {
        buf[n] = '\0';
        printf("4. 收到服务器回复：%s\n", buf);
     }
    
    close(sock);
    return 0;
}