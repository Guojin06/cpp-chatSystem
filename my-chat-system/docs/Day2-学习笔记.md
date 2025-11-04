# Day2：Reactor模式与Channel类 - 开始理解设计模式了

**日期**：2024.10.27  
**学习时间**：下午2点到晚上5点（3小时，中间休息了半小时）  
**状态**：今天脑子有点转不过来，不过最后还是搞明白了

---

## 今天干了啥

昨天写的代码今天早上看了一眼，发现main函数里一坨代码，乱七八糟的。虽然能跑，但是看着难受。想起之前看muduo库的时候，人家代码写得清清楚楚，Channel、EventLoop这些类，一看就知道干啥的。

所以今天重点就是学**Reactor模式**和实现**Channel类**。说实话一开始听到Reactor这个名字我是懵的，什么反应器？后来理解了才发现，就是把"事件"和"处理逻辑"绑在一起，让代码更清晰。

今天主要学了：
1. **为什么需要Reactor** - Day1的代码有啥问题
2. **Reactor的4个角色** - Reactor、Demultiplexer、Handler、Event Handler
3. **Channel类的设计** - 封装fd + events + callbacks
4. **测试Channel** - 用lambda设置回调，验证逻辑

核心产出：**Channel类实现 + 测试通过**（80行代码）

---

## Day1的代码问题在哪

昨天写完代码，当时觉得还行，能跑就行。今天再看，问题太明显了：

```cpp
// 昨天的main函数（简化版）
int main() {
    // 一大堆初始化
    int listen_fd = socket(...);
    int epfd = epoll_create(1);
    
    while (1) {
        int n = epoll_wait(...);
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            
            if (fd == listen_fd) {
                // 处理新连接的逻辑写这里
                int client_fd = accept(...);
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, ...);
            } else {
                // 处理数据的逻辑写这里
                char buf[1024];
                int len = recv(fd, buf, sizeof(buf), 0);
                send(fd, buf, len, 0);
            }
        }
    }
}
```

**问题：**
1. **所有逻辑堆在main里**：创建、监听、处理，全在一个函数
2. **硬编码**：`if (fd == listen_fd)` 这种判断，要加新功能怎么办？
3. **fd和处理逻辑分离**：fd是fd，处理逻辑是处理逻辑，没绑在一起
4. **不好测试**：怎么单独测试"处理新连接"的逻辑？

**核心问题：fd和它的事件处理逻辑是分开的。**

比如listen_fd是用来接受新连接的，client_fd是用来收发数据的，但是代码里要用if判断，这就很蠢。

---

## Reactor模式到底是啥

今天花了1个小时理解这个模式。一开始看文档，什么"事件驱动"、"异步I/O"，看得云里雾里。后来看了muduo的实现，才大概明白。

### 核心思想

**把"fd"、"关注的事件"、"事件处理逻辑"绑在一起。**

比如：
- listen_fd关注EPOLLIN事件，处理逻辑是accept新连接
- client_fd关注EPOLLIN事件，处理逻辑是recv数据

这样，epoll_wait返回哪个fd就绪了，直接调用它的处理逻辑就行，不用if判断。

### Reactor的4个角色

今天被这4个角色搞得有点晕，后来画了个图才理解：

```
┌──────────────────────────────────┐
│     1. Reactor（事件循环）        │  ← EventLoop类（明天做）
│  while(1) { epoll_wait() }       │
└──────────────────────────────────┘
            ↓ 事件来了
┌──────────────────────────────────┐
│  2. Demultiplexer（事件分离器）   │  ← epoll
│  "fd 5可读了！"                   │
└──────────────────────────────────┘
            ↓ 找到对应的Handler
┌──────────────────────────────────┐
│    3. Handler（事件处理器）       │  ← Channel类（今天做）
│  "收到EPOLLIN，调用readCallback_" │
└──────────────────────────────────┘
            ↓ 执行业务逻辑
┌──────────────────────────────────┐
│   4. Event Handler（回调函数）    │  ← 用户代码
│  void OnRead() { recv... }       │
└──────────────────────────────────┘
```

**今天重点：Handler这层，也就是Channel类。**

---

## Channel类的设计

### 为什么需要Channel

最开始我不理解为什么要有这个类。后来想通了：

**没有Channel之前：**
```cpp
// fd是fd
int listen_fd = socket(...);

// 处理逻辑是处理逻辑
void handle_accept() {
    int client_fd = accept(listen_fd, ...);
    // ...
}

// epoll_wait返回后，要自己判断
if (fd == listen_fd) {
    handle_accept();
}
```

**有了Channel之后：**
```cpp
// 创建Channel，把fd和处理逻辑绑在一起
Channel listen_ch(listen_fd);
listen_ch.SetReadCallback(handle_accept);  // 绑定回调

// epoll_wait返回后，直接调用
listen_ch.HandleEvent();  // 自动调用handle_accept
```

看出区别了吗？**不用if判断了，fd和处理逻辑绑在一起了。**

### Channel封装了什么

今天实现的时候，想清楚了Channel要封装3样东西：

```
Channel = fd + events + callbacks
```

具体来说：
1. **fd**：文件描述符
2. **events_**：我想关注什么事件（EPOLLIN、EPOLLOUT）
3. **revents_**：实际发生了什么事件（epoll_wait返回的）
4. **callbacks**：事件发生时要执行的回调函数

### events_ vs revents_（重要！）

这个我一开始搞混了，后来才理解：

**events_**：我想关注的事件
- "我希望epoll告诉我这个fd什么时候可读"
- 程序员设置的
- 用`EnableReading()`等方法修改

**revents_**：实际发生的事件
- "epoll告诉我：这个fd现在可读了"
- epoll_wait返回的
- 用`SetRevents()`设置（EventLoop调用）

**类比：**
- `events_`：我订阅了"体育新闻"和"财经新闻"
- `revents_`：今天来了一条"体育新闻"

---

## 代码实现过程

### Channel.h（头文件）

```cpp
#ifndef CHANNEL_H
#define CHANNEL_H

#include <sys/epoll.h>
#include <functional>

class Channel {
private:
    int fd_;
    int events_;
    int revents_;
    
    std::function<void()> readCallback_;
    std::function<void()> writeCallback_;
    std::function<void()> closeCallback_;
    std::function<void()> errorCallback_;
    
public:
    Channel(int fd);
    ~Channel();
    
    int GetFd() const { return fd_; }
    int GetEvents() const { return events_; }
    void SetRevents(int revents) { revents_ = revents; }
    
    void SetReadCallback(std::function<void()> cb) { readCallback_ = cb; }
    void SetWriteCallback(std::function<void()> cb) { writeCallback_ = cb; }
    void SetCloseCallback(std::function<void()> cb) { closeCallback_ = cb; }
    void SetErrorCallback(std::function<void()> cb) { errorCallback_ = cb; }
    
    void EnableReading() { events_ |= EPOLLIN; }
    void EnableWriting() { events_ |= EPOLLOUT; }
    void DisableWriting() { events_ &= ~EPOLLOUT; }
    void DisableAll() { events_ = 0; }
    
    void HandleEvent();
};

#endif
```

### Channel.cpp（实现）

```cpp
#include "Channel.h"
#include <stdio.h>

Channel::Channel(int fd) 
    : fd_(fd), events_(0), revents_(0) {
}

Channel::~Channel() {
    // fd由Socket类管理，Channel不负责关闭
}

void Channel::HandleEvent() {
    // 先处理错误和关闭，再处理正常读写
    
    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }
    
    if (revents_ & (EPOLLHUP | EPOLLRDHUP)) {
        if (closeCallback_) {
            closeCallback_();
        }
    }
    
    if (revents_ & EPOLLIN) {
        if (readCallback_) {
            readCallback_();
        }
    }
    
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }
}
```

### 实现过程中的思考

#### 1. EnableReading()为什么用|=？

一开始我写成了：
```cpp
void EnableReading() { events_ = EPOLLIN; }
```

结果发现问题了：如果之前设置了EPOLLOUT，这样会把EPOLLOUT清空！

正确写法：
```cpp
void EnableReading() { events_ |= EPOLLIN; }
```

用位或操作，只添加EPOLLIN，不影响其他事件。

**类比：**
```
假设 events_ = EPOLLOUT（二进制：10）
现在要添加EPOLLIN（二进制：01）

错误写法：events_ = EPOLLIN
    结果：01（EPOLLOUT没了！）

正确写法：events_ |= EPOLLIN
    结果：11（两个事件都有）
```

#### 2. DisableWriting()为什么用&= ~？

相反，如果要去掉一个事件，用&= ~：

```cpp
void DisableWriting() { events_ &= ~EPOLLOUT; }
```

**原理：**
```
假设 events_ = EPOLLIN | EPOLLOUT（二进制：11）
现在要去掉EPOLLOUT（二进制：10）

~EPOLLOUT = 01（取反）
events_ &= ~EPOLLOUT
    = 11 & 01
    = 01（只剩EPOLLIN）
```

#### 3. HandleEvent()的处理顺序

这个我想了一会儿，为什么要先处理错误，再处理读写？

```cpp
void Channel::HandleEvent() {
    // 顺序很重要！
    if (revents_ & EPOLLERR) { ... }           // 第1步：错误
    if (revents_ & (EPOLLHUP | EPOLLRDHUP)) { ... } // 第2步：关闭
    if (revents_ & EPOLLIN) { ... }            // 第3步：可读
    if (revents_ & EPOLLOUT) { ... }           // 第4步：可写
}
```

**原因：**
- 如果fd已经出错了，读写没意义，先处理错误
- 如果fd已经关闭了，读写会失败，先处理关闭
- 正常情况下，才处理读写

**反例：** 如果先处理EPOLLIN再处理EPOLLERR
```cpp
// 错误顺序
if (revents_ & EPOLLIN) {
    int n = read(fd_, buf, sizeof(buf));  // fd已经出错，read返回-1
    // 业务逻辑处理buf... 出bug了！
}
if (revents_ & EPOLLERR) {
    // 太晚了，上面已经出错了
}
```

#### 4. 为什么不关闭fd？

```cpp
Channel::~Channel() {
    // 注意：不调用close(fd_)
}
```

一开始我想在析构函数里close(fd_)，后来想了想不对：
- fd的生命周期由Socket类管理
- Channel只是"使用"fd，不"拥有"fd
- 可能有多个Channel对应一个fd（虽然不常见）

**类比：**
- fd是一辆车（Socket拥有）
- Channel是司机（使用车，但不拥有车）
- 司机下班不能把车砸了（不能close fd）

---

## 测试代码

写完Channel后，必须测试一下。写了个简单的测试：

```cpp
// test_channel.cpp
#include "Channel.h"
#include <iostream>
#include <sys/epoll.h>

int main() {
    Channel ch(0);  // fd 0是stdin
    
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
    
    // 模拟事件发生
    std::cout << "test EPOLLIN:\n";
    ch.SetRevents(EPOLLIN);
    ch.HandleEvent();

    std::cout << "\ntest EPOLLOUT:\n";
    ch.SetRevents(EPOLLOUT);
    ch.HandleEvent();

    std::cout << "\ntest EPOLLERR:\n";
    ch.SetRevents(EPOLLERR);
    ch.HandleEvent();

    std::cout << "\ntest EPOLLHUP:\n";
    ch.SetRevents(EPOLLHUP);
    ch.HandleEvent();
    
    return 0;
}
```

编译运行：
```bash
make test_channel
./test_channel
```

输出：
```
test EPOLLIN:
read callback called

test EPOLLOUT:
write callback called

test EPOLLERR:
error callback called

test EPOLLHUP:
close callback called
```

**成功！** 回调能正确调用。

---

## 遇到的坑

### 坑1：Makefile编译错误

一开始make报错：
```
g++ -c src/Channel.cpp -o build/Channel.o
Channel.cpp:1:10: fatal error: Channel.h: No such file or directory
```

**原因：** Channel.cpp include的是`"Channel.h"`，但是Channel.h在src目录下，编译器找不到。

**解决：** Makefile里加上`-Isrc`：
```makefile
CXXFLAGS = -Wall -g -std=c++11 -Isrc
```

### 坑2：lambda不能用，编译报错

一开始写测试的时候，lambda编译报错：
```
error: no matching function for call to 'Channel::SetReadCallback(<lambda>)'
```

**原因：** 忘记在Makefile里加`-std=c++11`了。

**解决：** 加上`-std=c++11`。

---

## 核心知识点

### 1. 位运算操作事件

**启用事件（|=）：**
```cpp
void EnableReading() { 
    events_ |= EPOLLIN;  // 不影响已有的其他事件
}
```

**禁用事件（&= ~）：**
```cpp
void DisableWriting() { 
    events_ &= ~EPOLLOUT;  // 只清除EPOLLOUT，不影响其他
}
```

### 2. std::function的强大之处

可以存储任何可调用对象：

```cpp
// 1. 普通函数
void MyRead() { std::cout << "read\n"; }
ch.SetReadCallback(MyRead);

// 2. lambda表达式（最常用）
ch.SetReadCallback([]() {
    std::cout << "read\n";
});

// 3. 带捕获的lambda
int count = 0;
ch.SetReadCallback([&count]() {
    std::cout << "read " << ++count << "\n";
});
```

### 3. events_ vs revents_

- `events_`：我订阅了什么事件（我设置的）
- `revents_`：实际发生了什么事件（epoll告诉我的）

---

## 今天的收获

### 技术收获

1. **Reactor模式的核心思想**
   - 把事件和处理逻辑绑定
   - 4个角色：Reactor、Demultiplexer、Handler、Event Handler

2. **Channel类的设计**
   - 封装fd + events + revents + callbacks
   - events_是想关注的，revents_是实际发生的

3. **位运算操作事件**
   - `|=`：添加事件（不影响其他）
   - `&= ~`：删除事件（不影响其他）

4. **HandleEvent()的顺序**
   - 先错误、后关闭、再读写

5. **std::function的用法**
   - 可以存储函数、lambda、成员函数



---

## 理解

今天学Reactor模式的时候，刚开始真的有点转不过弯。什么Handler、Demultiplexer，听着就头疼。后来画了图，对着muduo的代码看，才慢慢理解。

特别是理解events_和revents_的区别，我一开始搞混了，后来想明白了：一个是"我想要的"，一个是"实际发生的"。就像订阅和接收的关系。

还有位运算，之前学过但是不熟。今天实现EnableReading()和DisableWriting()的时候，又复习了一遍，这次应该记牢了。

明天要实现EventLoop，把整个Reactor模式跑起来！到时候就能把Day1的代码重构了，应该会清爽很多。

---

**Day2完成时间：** 2025.10.27 17:00  

**Bug数量：** 2个（Makefile相关，已修复）  



