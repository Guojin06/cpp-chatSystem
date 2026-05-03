#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>  // 用于设置非阻塞

#define MAX_EVENTS 10
#define PORT 8080

// 设置socket为非阻塞（ET模式需要）
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    // TODO 1: 创建listen socket（和Day1一样）
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);//参数1：地址族，参数2：套接字类型，参数3：协议
    //返回，listen_sock是文件描述符，用于后续的accept
    if (listen_sock < 0) {
        perror("socket");
        return 1;
    }
    printf("Listen socket created: fd=%d\n", listen_sock);
    
    // TODO 2: 设置地址复用（避免TIME_WAIT问题）
    // 提示：setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, ...)
    int on = 1;//设置地址复用
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));//设置地址复用
    //参数
    //SO_REUSEADDR：允许地址复用
    //on：1表示允许地址复用，0表示不允许地址复用
    //sizeof(on)：选项值长度
    //返回值：0表示成功，-1表示失败
    //SOL_SOCKET：套接字选项
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("setsockopt");
        return 1;
    }
    printf("Address reuse enabled\n");
    
    //创建sockaddr_in结构体
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    // TODO 3: bind（和Day1一样）
    // 提示：创建sockaddr_in结构体，设置IP和端口，调用bind
    //绑定地址
    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    {
        perror("bind 绑定地址失败");
        return 1;
    }
    printf("绑定地址成功，监听端口：%d\n", PORT);//绑定成功
    
    
    // TODO 4: listen（和Day1一样）
    // 提示：listen(listen_sock, 128)
    int ret = listen(listen_sock, 128);//128表示等待队列的最大长度
    if (ret < 0) {
        perror("listen");
        return 1;
    }
    printf("监听端口成功，开始监听端口：%d\n", PORT);//监听成功
    
    
    printf("Server listening on port %d\n", PORT);
    
    // TODO 5: 创建epoll实例
    // 提示：int epfd = epoll_create(1);
    int epfd = epoll_create(1); // 1在老版本中表示监听的fd数量，新版本中被忽略，写1就行
    if (epfd < 0) {
        perror("epoll_create");
        return 1;
    }
    printf("epoll实例创建成功，文件描述符：%d\n", epfd);//epoll实例创建成功
    
    // TODO 6: 把listen_sock加入epoll监听
    // 提示：
    //   struct epoll_event ev;
    //   ev.events = EPOLLIN;
    //   ev.data.fd = listen_sock;
    //   epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &ev);
    
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;//【修改】设置为边缘触发模式（ET）
    //EPOLLIN：可读事件
    //EPOLLET：边缘触发（Edge Triggered），只在状态变化时通知一次
    //对比：LT（水平触发）会在缓冲区有数据时一直通知
    ev.data.fd = listen_sock;//用户数据为listen_sock，用于epoll_wait返回时，找到对应的fd
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &ev);//将listen_sock加入epoll监听
    //参数
    //epfd：epoll实例文件描述符
    //EPOLL_CTL_ADD：添加事件
    //listen_sock：要添加的文件描述符
    //&ev：事件结构体，struct epoll_event *event;
    //返回值：0表示成功，-1表示失败
    
    printf("Epoll created, waiting for events...\n");
    
    // 事件循环
    struct epoll_event events[MAX_EVENTS];//MAX_EVENTS是最大事件数，用于存储epoll_wait返回的事件
    while (1) {
        // TODO 7: 调用epoll_wait等待事件
        // 提示：int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        //参数epfd：epoll实例文件描述符
        //events：事件数组
        //MAX_EVENTS：最大事件数
        //-1：等待时间，-1表示永久阻塞，直到有事件
        //返回值：成功返回就绪事件数，失败返回-1
        //n表示就绪事件数
        
        if (n < 0) {
            perror("epoll_wait");
            break;
        }
        
        // 处理所有就绪的事件，如果有新连接，则加入epoll监听，如果有数据可读，则读取数据并发送给客户端
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            
            if (fd == listen_sock) {//如果fd是监听socket文件描述符，则有新连接
                // TODO 8: 有新连接
                // 提示：
                //   1. int client = accept(listen_sock, NULL, NULL);
                //   2. printf("New client: fd=%d\n", client);
                //   3. 把client加入epoll监听（和TODO 6类似）
                int client = accept(listen_sock, NULL, NULL);//accept返回一个新连接的socket文件描述符
                //参数
                //listen_sock：监听socket文件描述符
                //NULL：客户端地址，NULL表示不保存客户端地址
                //NULL：客户端地址长度，NULL表示不保存客户端地址长度
                //返回值：成功返回新连接的socket文件描述符，失败返回-1
                //为什么这里是NULL？因为我们要监听所有客户端的连接，所以不需要保存客户端地址
                if (client < 0) {
                    perror("accept");
                    continue;
                }
                
                set_nonblocking(client);  // ET模式必须：设置为非阻塞
                
                printf("新客户端连接，文件描述符：%d\n", client);//新客户端连接
                //把新客户端加入epoll监听
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET;//【修改】客户端也用边缘触发
                ev.data.fd = client;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev);//将新客户端加入epoll监听

            } else {
                // TODO 9: 某个客户端有数据
                // ET模式必须：循环读取直到EAGAIN（把内核缓冲区数据全部读完）
                char buf[1024];
                
                while (1) {  // 循环读取
                    int len = recv(fd, buf, sizeof(buf)-1, 0);//len表示接收的字节数
                    //参数
                    //fd：客户端socket文件描述符
                    //buf：缓冲区，用于存储接收的数据
                    //sizeof(buf)-1：缓冲区大小，-1表示不包括结尾的'\0'
                    //0：标志位，0表示阻塞模式（但我们设置了socket为非阻塞，所以不会阻塞）
                    //返回值：成功返回接收的字节数，失败返回-1
                    //失败原因：errno
                    //成功原因：recv成功
                    
                    if (len > 0) {
                        // 读到数据，echo回去
                        buf[len] = '\0';//字符串结尾
                        printf("收到 %d 字节，fd=%d\n", len, fd);
                        send(fd, buf, len, 0);//发送消息给客户端
                        //参数
                        //fd：客户端socket文件描述符
                        //buf：缓冲区，用于存储发送的数据
                        //len：发送的字节数
                        //0：标志位，0表示阻塞模式
                        //返回值：成功返回发送的字节数，失败返回-1
                        // 继续循环，读取剩余数据
                        
                    } else if (len == 0) {
                        // len==0：对端关闭连接
                        printf("客户端断开连接，文件描述符：%d\n", fd);//客户端断开连接
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);//将客户端从epoll监听中删除
                        close(fd);//关闭客户端socket文件描述符
                        break;
                        
                    } else {
                        // len==-1：出错或数据读完
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // EAGAIN：数据读完了！（这是ET模式的正常情况）
                            printf("数据读取完毕，fd=%d\n", fd);
                            break;
                        } else {
                            // 真的出错了
                            perror("recv error");
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                            close(fd);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    close(epfd);
    close(listen_sock);
    return 0;
}

