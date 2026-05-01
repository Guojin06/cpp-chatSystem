#include "../include/serial_reader.h"

#include <fcntl.h>      // open() 的标志位
#include <unistd.h>     // read(), write(), close()
#include <termios.h>    // 串口配置（波特率、数据位等）
#include <cstring>      // memset
#include <iostream>

// 将整数波特率转换为 termios 的波特率常量
static speed_t toBaudConst(int baudrate) {
    switch (baudrate) {
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 115200: return B115200;
        default:     return B9600;
    }
}

SerialReader::SerialReader(const std::string& port, int baudrate)
    : port_(port), baudrate_(baudrate), fd_(-1) {
    // fd_ = -1 表示未打开
}

SerialReader::~SerialReader() {
    close();
}

bool SerialReader::open() {
    // O_RDWR: 可读可写
    // O_NOCTTY: 不让串口成为进程的控制终端
    // O_NDELAY: 非阻塞打开（后面通过 termios 设置超时）
    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd_ < 0) {
        std::cerr << "无法打开串口: " << port_ << std::endl;
        return false;
    }

    // 配置串口参数
    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "tcgetattr 失败" << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // 设置波特率
    speed_t baud = toBaudConst(baudrate_);
    cfsetispeed(&tty, baud);  // 输入波特率
    cfsetospeed(&tty, baud);  // 输出波特率

    // 8N1：8 数据位，无校验，1 停止位（串口通信最常见的配置）
    tty.c_cflag &= ~PARENB;   // 无校验位
    tty.c_cflag &= ~CSTOPB;   // 1 个停止位
    tty.c_cflag &= ~CSIZE;    // 清除数据位设置
    tty.c_cflag |= CS8;       // 8 个数据位

    // 使能接收 + 本地模式
    tty.c_cflag |= (CLOCAL | CREAD);

    // 原始模式（不做任何字符处理，直接传输原始字节）
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    // 读取超时设置：最少读 1 字节，超时 10 * 0.1s = 1 秒
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;  // 单位是 0.1 秒

    tcsetattr(fd_, TCSANOW, &tty);
    tcflush(fd_, TCIOFLUSH);  // 清空缓冲区

    std::cout << "串口已打开: " << port_ << " @ " << baudrate_ << " baud" << std::endl;
    return true;
}

void SerialReader::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialReader::isOpen() const {
    return fd_ >= 0;
}

std::vector<uint8_t> SerialReader::readFrame(int timeoutMs) {
    // 645 报文结构：68 [addr 6B] 68 [ctrl] [len] [data...] [cs] 16
    // 最小帧长 = 12 字节
    // 策略：逐字节读取，找到 0x68 开头，根据长度字段确定帧尾

    std::vector<uint8_t> buffer;
    uint8_t byte;
    int elapsed = 0;
    const int stepMs = 10;
    bool frameStarted = false;

    while (elapsed < timeoutMs) {
        ssize_t n = ::read(fd_, &byte, 1);

        if (n <= 0) {
            // 没读到数据，等一会再试
            usleep(stepMs * 1000);  // usleep 的单位是微秒
            elapsed += stepMs;
            continue;
        }

        // 找帧头 0x68
        if (!frameStarted) {
            if (byte == 0x68) {
                buffer.clear();
                buffer.push_back(byte);
                frameStarted = true;
            }
            continue;
        }

        buffer.push_back(byte);

        // 帧尾 0x16 且长度足够（至少 12 字节）
        if (byte == 0x16 && buffer.size() >= 12) {
            // 验证第二个帧头
            if (buffer.size() > 7 && buffer[7] == 0x68) {
                return buffer;  // 完整帧
            }
            // 不是有效帧，重新找
            frameStarted = false;
            buffer.clear();
        }

        // 防止垃圾数据撑爆内存
        if (buffer.size() > 256) {
            frameStarted = false;
            buffer.clear();
        }
    }

    // 超时，返回空
    return {};
}
