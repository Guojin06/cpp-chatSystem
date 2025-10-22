#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>//inet_ntoa函数头文件
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(){
    //1. 创建socket
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_sock < 0){
        perror("socket create failed");
        return 1;
    }
   printf("socket create success, 文件描述符fd = %d\n", listen_sock);

   //2.绑定地址（bind）
   struct sockaddr_in addr;//IPv4地址结构体,头文件<netinet/in.h>
   addr.sin_family = AF_INET;//这里本机不用127.0.0.1吗？还是，af_inet表示IPv4地址族，地址族是:IPv4,IPv6,Unix
   addr.sin_port = htons(8080);//host to network short,网络传输时需要把数字转换成网络字节序
   addr.sin_addr.s_addr = INADDR_ANY;//0.0.0.0表示监听所有网卡的IP地址
   int ret = bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr));
   if(ret < 0){
    perror("bind failed");
    return 1;
   }
   printf("bind success，监听端口：%d\n", ntohs(addr.sin_port));
   //3. 监听（listen）
   int ret2 = listen(listen_sock, 128);
   if(ret2 < 0){
    perror("listen failed");
    return 1;
   }
   printf("listen success，开始监听端口：%d\n", ntohs(addr.sin_port));
   //4. 接受连接（accept）
   printf("等待客户端连接...\n");

   struct sockaddr_in client_addr;
   socklen_t client_addr_len = sizeof(client_addr);//客户端地址长度

   int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_addr_len);//accept返回一个新连接的socket文件描述符
   if(client_sock < 0){
    perror("accept failed");
    return 1;
   }

   //处理连接（recv/send）

   //5.接收消息
   char buf[1024];
   ssize_t n = recv(client_sock, buf, sizeof(buf)-1, 0);
   if (n > 0) {
       buf[n] = '\0';  // 字符串结尾
       printf("5. 收到客户端消息：%s\n", buf);
       //6.回复消息
       const char* reply = "Hello, Client! 我收到你的消息了！";
       send(client_sock, reply, strlen(reply), 0);
       printf("6. 回复客户端：%s\n", reply);
   }
   
   //6. 关闭连接（close）
   close(listen_sock);
   return 0;
}