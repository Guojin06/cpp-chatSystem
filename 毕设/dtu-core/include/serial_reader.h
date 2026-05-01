#pragma once

#include <string>
#include <vector>
#include <cstdint>

// 串口读取器：从串口设备读取 DL/T 645 报文
// 使用 Linux 的 termios API 操作串口
class SerialReader {
public:
    // 构造函数：指定串口设备路径和波特率
    SerialReader(const std::string& port, int baudrate = 9600);

    // 析构函数：关闭串口
    ~SerialReader();

    // 禁止拷贝（串口文件描述符不能被拷贝）
    SerialReader(const SerialReader&) = delete;
    SerialReader& operator=(const SerialReader&) = delete;

    // 打开串口，返回是否成功
    bool open();

    // 关闭串口
    void close();

    // 读取一帧完整的 645 报文
    // 从字节流中找到 0x68 开头、0x16 结尾的完整帧
    // 超时返回空 vector
    std::vector<uint8_t> readFrame(int timeoutMs = 3000);

    // 串口是否已打开
    bool isOpen() const;

private:
    std::string port_;      // 设备路径，如 "/tmp/vserial1"
    int baudrate_;          // 波特率
    int fd_;                // 文件描述符（Linux 用 int 表示打开的文件/设备）
};
