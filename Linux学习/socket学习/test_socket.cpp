#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>

int main(){
    //创建一个Tcp socket（IPv4,流式套接字）
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock < 0){
        perror("socket create failed");
        return 1;
    }
    printf("socket create success, 文件描述符fd = %d\n", sock);
    //关闭socket
    close(sock);
    return 0;
}
