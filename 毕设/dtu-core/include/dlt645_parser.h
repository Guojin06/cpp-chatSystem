#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// 解析结果结构体：645 报文解析后的"翻译成果"
struct MeterReading {
    std::string meterId;       // 电表编号，如 "000000000001"
    std::string timestamp;     // 采集时间戳
    double voltage;            // 电压（V）
    double current;            // 电流（A）
    double activeEnergy;       // 有功电能（kWh）
    double powerFactor;        // 电表功率
    std::string rawFrame;      // 原始报文的十六进制字符串（调试用）
};

// DL/T 645 协议解析器
// public 接口只暴露一个 parseFrame，外部只需关心"传入字节流、拿到结果"
// private 方法封装了验证、校验、解码等内部细节
class DLT645Parser {
public:
    // 解析一帧 645 报文，成功返回 MeterReading，失败返回 nullopt
    std::optional<MeterReading> parseFrame(const std::vector<uint8_t>& frame) const;

private:
    // 验证帧合法性：帧头帧尾、长度、校验码
    bool validateFrame(const std::vector<uint8_t>& frame) const;
    // 计算校验码：frame[0] 到 frame[endExclusive-1] 的字节之和，取低 8 位
    uint8_t calculateChecksum(const std::vector<uint8_t>& frame, std::size_t endExclusive) const;
    // 从地址域提取电表编号（低字节在前，需反转）
    std::string parseMeterId(const std::vector<uint8_t>& frame) const;
    // BCD 解码：将 BCD 字节（低字节在前）转为浮点数，除以 scale 得到实际值
    double decodeBcdValueReversed(const std::vector<uint8_t>& bytes, double scale) const;
    // 将整帧字节转为十六进制字符串（如 "68 01 00 ..."），方便调试
    std::string frameToHex(const std::vector<uint8_t>& frame) const;
};
