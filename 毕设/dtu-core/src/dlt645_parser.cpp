#include "../include/dlt645_parser.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {
// 获取当前时间戳，格式："2026-03-08 21:00:00"
std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
}

// 主解析函数：外部唯一入口
// 流程：验证帧 → 提取基本信息 → 数据域减 0x33 → 切片取值 → BCD 解码
std::optional<MeterReading> DLT645Parser::parseFrame(const std::vector<uint8_t>& frame) const {
    // 第1步：验证帧合法性（帧头帧尾、长度、校验码）
    if (!validateFrame(frame)) {
        return std::nullopt;
    }

    // 第2步：填充基本信息
    MeterReading reading;
    reading.meterId = parseMeterId(frame);      // 从地址域提取电表编号
    reading.timestamp = currentTimestamp();      // 当前时间
    reading.rawFrame = frameToHex(frame);        // 原始报文十六进制

    // 第3步：取出数据域并执行 645 协议规定的"减 0x33"操作
    // frame[9] = 数据域长度，frame[10] 开始是数据域内容
    const std::size_t dataLen = frame[9];
    if (dataLen < 10 || frame.size() < 12 + dataLen) {
        return std::nullopt;
    }

    // 拷贝数据域，然后每个字节减 0x33 还原真实值
    // （645 协议为了避免数据域出现帧头 0x68 / 帧尾 0x16，发送前对每字节加了 0x33）
    std::vector<uint8_t> data(frame.begin() + 10, frame.begin() + 10 + dataLen);
    for (auto& byte : data) {
        byte = static_cast<uint8_t>(byte - 0x33);
    }

    if (data.size() < 10) {
        return std::nullopt;
    }

    // 第4步：切片取值
    // 减 0x33 后的数据域布局：[ 数据标识 4B | 电压 2B | 电流 2B | 电能 2B ]
    std::vector<uint8_t> voltageBytes(data.begin() + 4, data.begin() + 6);
    std::vector<uint8_t> currentBytes(data.begin() + 6, data.begin() + 8);
    std::vector<uint8_t> energyBytes(data.begin() + 8, data.begin() + 10);

    // 第5步：BCD 解码，除以 scale 得到带小数点的实际值
    reading.voltage = decodeBcdValueReversed(voltageBytes, 10.0);       // 如 222 / 10.0 = 22.2V
    reading.current = decodeBcdValueReversed(currentBytes, 100.0);      // 如 12 / 100.0 = 0.12A
    reading.activeEnergy = decodeBcdValueReversed(energyBytes, 100.0);  // 如 1055 / 100.0 = 10.55kWh

    // 功率因数：目前用模拟值 0.85，未来从报文中额外字段解析
    reading.powerFactor = 0.85;

    return reading;
}

// 帧合法性验证：4道门卫检查
bool DLT645Parser::validateFrame(const std::vector<uint8_t>& frame) const {
    // 检查 1：最小长度（帧头+地址+帧头+控制码+长度+校验+帧尾 = 12 字节）
    if (frame.size() < 12) {
        return false;
    }

    // 检查 2：帧头帧尾是否符合 645 协议规定
    if (frame.front() != 0x68 || frame[7] != 0x68 || frame.back() != 0x16) {
        return false;
    }

    // 检查 3：数据域长度字段与实际帧长是否一致
    const std::size_t dataLen = frame[9];
    if (frame.size() != 12 + dataLen) {
        return false;
    }

    // 检查 4：重新计算校验码，与报文中携带的校验码对比
    const auto checksum = calculateChecksum(frame, frame.size() - 2);
    return checksum == frame[frame.size() - 2];
}

// 校验码计算：frame[0] 到 frame[endExclusive-1] 的字节累加，取低 8 位
uint8_t DLT645Parser::calculateChecksum(const std::vector<uint8_t>& frame, std::size_t endExclusive) const {
    uint8_t sum = 0;
    for (std::size_t i = 0; i < endExclusive; ++i) {
        sum = static_cast<uint8_t>(sum + frame[i]);
    }
    return sum;
}

// 提取电表编号：frame[1..6] 为地址域，低字节在前，所以从后往前读
std::string DLT645Parser::parseMeterId(const std::vector<uint8_t>& frame) const {
    std::ostringstream oss;
    // 反向遍历 frame[6] → frame[1]，每个字节转为两位十六进制
    for (int i = 6; i >= 1; --i) {
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(frame[i]);
    }
    return oss.str();  // 例如 "000000000001"
}

// BCD 解码（低字节在前，所以反向遍历）
// 每个字节拆成高4位和低4位，各代表一位十进制数字
// 例如：{0x22, 0x02} → 反向处理 0x02=02, 0x22=22 → 拼成 0222 → 222 ÷ 10.0 = 22.2
double DLT645Parser::decodeBcdValueReversed(const std::vector<uint8_t>& bytes, double scale) const {
    long long value = 0;
    for (auto it = bytes.rbegin(); it != bytes.rend(); ++it) {
        const auto byte = *it;
        const int high = (byte >> 4) & 0x0F;  // 右移 4 位取高 4 位
        const int low = byte & 0x0F;           // 掩码取低 4 位
        value = value * 100 + high * 10 + low; // 每个字节贡献两位十进制
    }
    return static_cast<double>(value) / scale;  // 除以缩放因子得到实际值
}

// 将字节数组转为十六进制字符串，空格分隔，方便调试和日志记录
// 例如："68 01 00 00 00 00 00 68 91 0A ..."
std::string DLT645Parser::frameToHex(const std::vector<uint8_t>& frame) const {
    std::ostringstream oss;
    for (std::size_t i = 0; i < frame.size(); ++i) {
        if (i > 0) {
            oss << ' ';
        }
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(frame[i]);
    }
    return oss.str();
}
