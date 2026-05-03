#include "../include/dlt645_parser.h"
#include "../include/serial_reader.h"
#include "../include/db_handler.h"
#include "../include/mqtt_client.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

// 匿名命名空间：内部的函数只在本文件可见，相当于 static
namespace {

// 构造一帧模拟的 DL/T 645 报文（未来替换为真实串口读取）
// 报文结构：帧头(0x68) | 地址域(6B) | 帧头(0x68) | 控制码 | 数据长度 | 数据域 | 校验码 | 帧尾(0x16)
std::vector<uint8_t> buildMockFrame() {
    std::vector<uint8_t> frame = {
        0x68,                                    // 起始帧头
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00,      // 地址域：电表编号 000000000001（低字节在前，BCD编码）
        0x68,                                    // 第二个帧头
        0x91,                                    // 控制码：从站正常应答读数据
        0x0A,                                    // 数据域长度 = 10 字节
        // --- 以下为数据域（每字节已加 0x33，接收方需减回） ---
        0x33, 0x33, 0x34, 0x35,                  // 数据标识（4字节）
        0x55, 0x35,                              // 电压（2字节 BCD）
        0x45, 0x33,                              // 电流（2字节 BCD）
        0x88, 0x43,                              // 有功电能（2字节 BCD）
        0x00,                                    // 校验码占位（下面计算后填入）
        0x16                                     // 结束符，固定值
    };

    // 计算校验码：从 frame[0] 到 frame[N-3] 的所有字节之和，取低8位
    uint8_t checksum = 0;
    for (std::size_t i = 0; i < frame.size() - 2; ++i) {
        checksum = static_cast<uint8_t>(checksum + frame[i]);
    }
    frame[frame.size() - 2] = checksum;  // 填入校验码
    return frame;
}

// 将解析结果写入 JSON 文件，供 Python API 层读取
// 参数用 const& 传递：只读 + 避免拷贝开销
bool writeLatestJson(const MeterReading& reading) {
    // 输出路径：相对于可执行文件位置（build 目录）
    std::filesystem::path outputPath = "../../api-server/data/latest.json";
    // 递归创建目录（类似 mkdir -p），防止目录不存在导致写入失败
    std::filesystem::create_directories(outputPath.parent_path());

    // 打开文件流，写入模式（覆盖旧内容）
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;  // 文件打开失败
    }

    // 手动拼接 JSON（零依赖，未来可替换为 nlohmann/json 库）
    out << "{\n";
    out << "  \"meter_id\": \"" << reading.meterId << "\",\n";
    out << "  \"status\": \"ok\",\n";
    out << "  \"timestamp\": \"" << reading.timestamp << "\",\n";
    out << "  \"data\": {\n";
    out << "    \"voltage\": " << reading.voltage << ",\n";
    out << "    \"current\": " << reading.current << ",\n";
    out << "    \"active_energy\": " << reading.activeEnergy << ",\n";
    out << "    \"power_factor\": " << reading.powerFactor << "\n";
    out << "  },\n";
    out << "  \"raw_frame\": \"" << reading.rawFrame << "\"\n";
    out << "}\n";

    return true;
}

//读文件到字符串
std::string readFile(const std::string& path){
    //1.用ifStream打开文件
    std::ifstream file(path);
    //2.判断是否打开成功
    if(!file.is_open()){
        return "";
    }
    //3.把内容读到一个string里
    std::string content;
    std::string line;
    //4.返回这个string
    while(std::getline(file,line)){//逐行读取
        content += line + "\n";
    }
    return content;
}

// 从 JSON 字符串中提取 poll_interval_sec 的值
// 简单字符串查找，不依赖第三方 JSON 库
int parsePollInterval(const std::string& json) {
    // 1. 找到 "poll_interval_sec" 在字符串中的位置
    size_t pos = json.find("poll_interval_sec");
    if (pos == std::string::npos) {
        return 10;  // 找不到，返回默认值，find 找不到时不会返回 -1，而是返回 std::string::npos（一个很大的数）。所以必须判断
    }
    // 2. 从那个位置往后找 ':' 的位置
    size_t colon = json.find(':', pos);
    if (colon == std::string::npos) {
        return 10;
    }
    // 3. ':' 后面的内容用 stoi 转成 int（stoi 会自动跳过空格）
    //    例如 ": 10," → stoi 从 colon+1 开始读，跳过空格，读到 10
    return std::stoi(json.substr(colon + 1));
}

// 从 JSON 字符串中提取 serial_port 的值（字符串类型）
std::string parseSerialPort(const std::string& json) {
    size_t pos = json.find("serial_port");
    if (pos == std::string::npos) {
        return "";  // 没配置串口，使用 mock 模式
    }
    // 找到冒号后的第一个引号和第二个引号之间的内容
    size_t q1 = json.find('"', json.find(':', pos) + 1);
    size_t q2 = json.find('"', q1 + 1);
    if (q1 == std::string::npos || q2 == std::string::npos) {
        return "";
    }
    return json.substr(q1 + 1, q2 - q1 - 1);
}

} // namespace

// ==================== 重传逻辑辅助函数 ====================

// 将 MeterRecord 转换为 JSON 字符串（与 MQTT 发布格式一致）
std::string recordToJson(const MeterRecord& r) {
    std::string json = "{";
    json += "\"meter_id\":\"" + r.meterId + "\",";
    json += "\"timestamp\":\"" + r.timestamp + "\",";
    json += "\"voltage\":" + std::to_string(r.voltage) + ",";
    json += "\"current\":" + std::to_string(r.current) + ",";
    json += "\"active_energy\":" + std::to_string(r.activeEnergy) + ",";
    json += "\"power_factor\":" + std::to_string(r.powerFactor);
    json += "}";
    return json;
}

// 尝试重传数据库中未上传的记录
// 返回值：本次成功上传的记录数量
int tryResendPendingRecords(DbHandler& db, MqttClient& mqtt) {
    // 1. 从数据库查出所有 uploaded=0 的记录（即之前发布失败的）
    auto pending = db.queryUnuploaded(100);  // 每次最多处理100条
    if (pending.empty()) {
        return 0;
    }

    std::cout << "[重传] 发现 " << pending.size() << " 条待重传记录" << std::endl;

    int successCount = 0;
    std::vector<int64_t> successIds;  // 收集成功上传的记录ID

    for (const auto& record : pending) {
        std::string topic = "dtu/meter/" + record.meterId;
        std::string payload = recordToJson(record);

        // 2. 尝试重新发布到 MQTT
        if (mqtt.publish(topic, payload, 0)) {
            successIds.push_back(record.id);
            successCount++;
            std::cout << "[重传] 成功: id=" << record.id
                      << " meter=" << record.meterId
                      << " ts=" << record.timestamp << std::endl;
        } else {
            // 3. 发布失败，累加重试次数
            db.markUploadFailed(record.id);
            std::cerr << "[重传] 失败: id=" << record.id << std::endl;
        }
    }

    // 4. 批量标记成功的记录为已上传
    if (!successIds.empty()) {
        db.markUploaded(successIds);
        std::cout << "[重传] 本轮成功上传 " << successCount << " 条" << std::endl;
    }

    return successCount;
}

// ==================== 主程序 ====================

int main() {
    // 读取配置文件
    std::string configStr = readFile("../config/config.json");
    int pollInterval = parsePollInterval(configStr);
    std::string serialPort = parseSerialPort(configStr);
    std::cout << "poll interval: " << pollInterval << "s" << std::endl;

    // 初始化数据库
    std::string dbPath = "../data/meter_readings.db";
    DbHandler db(dbPath);
    if (!db.init()) {
        std::cerr << "数据库初始化失败，程序退出" << std::endl;
        return 1;
    }
    std::cout << "数据库初始化成功: " << dbPath << std::endl;

    // 初始化MQTT客户端（简化版，先硬编码）
    // TODO: 从配置文件读取MQTT参数
    MqttClient mqtt("localhost", 1883);
    bool mqttConnected = false;
    
    std::cout << "尝试连接MQTT服务器..." << std::endl;
    if (mqtt.connect("dtu_client_001")) {
        mqttConnected = true;
    } else {
        std::cout << "MQTT连接失败，继续运行（数据将仅存储本地）" << std::endl;
    }

    // 如果配置了串口路径，使用串口模式；否则使用 mock 模式
    SerialReader* serial = nullptr;
    if (!serialPort.empty()) {
        serial = new SerialReader(serialPort);
        if (!serial->open()) {
            std::cerr << "串口打开失败，回退到 mock 模式" << std::endl;
            delete serial;
            serial = nullptr;
        }
    } else {
        std::cout << "未配置串口，使用 mock 模式" << std::endl;
    }

    DLT645Parser parser;

    // ==================== 重传相关状态初始化 ====================
    const int RESEND_INTERVAL_SEC = 60;      // 每60秒尝试重传一次
    int elapsedSinceResend = 0;               // 距离上次重传的已过秒数
    auto lastTick = std::chrono::steady_clock::now();  // 上次 tick 的时间点

    while (true) {
        // 第1步：获取一帧数据
        std::vector<uint8_t> frame;
        if (serial != nullptr) {
            frame = serial->readFrame(5000);
            if (frame.empty()) {
                std::cerr << "串口读取超时，等待下一轮" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(pollInterval));
                continue;
            }
        } else {
            frame = buildMockFrame();
        }

        // 第2步：解析帧
        auto result = parser.parseFrame(frame);
        if (!result.has_value()) {
            std::cerr << "parse frame failed" << std::endl;
            continue;
        }

        // 第3步：插入数据库
        MeterRecord record;
        record.meterId = result->meterId;
        record.timestamp = result->timestamp;
        record.voltage = result->voltage;
        record.current = result->current;
        record.activeEnergy = result->activeEnergy;
        record.powerFactor = result->powerFactor;
        record.rawFrame = result->rawFrame;
        record.uploaded = 0;
        record.uploadRetry = 0;
        if (!db.insert(record)) {
            std::cerr << "数据库插入失败" << std::endl;
        } else {
            std::cout << "数据已存入数据库" << std::endl;
        }

        // 第4步：写入 JSON 文件（供 Python API 层透传）
        if (!writeLatestJson(result.value())) {
            std::cerr << "write latest json failed" << std::endl;
        }

        // 第5步：终端打印
        std::cout << "meter_id: " << result->meterId << std::endl;
        std::cout << "voltage: " << result->voltage << std::endl;
        std::cout << "current: " << result->current << std::endl;
        std::cout << "active_energy: " << result->activeEnergy << std::endl;
        std::cout << "power_factor: " << result->powerFactor << std::endl;
        std::cout << "---" << std::endl;

        // 第6步：发布到MQTT（如果已连接）
        if (mqttConnected) {
            // 构建JSON消息
            std::string mqtt_payload = "{";
            mqtt_payload += "\"meter_id\":\"" + result->meterId + "\",";
            mqtt_payload += "\"timestamp\":\"" + result->timestamp + "\",";
            mqtt_payload += "\"voltage\":" + std::to_string(result->voltage) + ",";
            mqtt_payload += "\"current\":" + std::to_string(result->current) + ",";
            mqtt_payload += "\"active_energy\":" + std::to_string(result->activeEnergy) + ",";
            mqtt_payload += "\"power_factor\":" + std::to_string(result->powerFactor);
            mqtt_payload += "}";

            std::string mqtt_topic = "dtu/meter/" + result->meterId;
            if (mqtt.publish(mqtt_topic, mqtt_payload, 0)) {
                std::cout << "[MQTT] 已发布到: " << mqtt_topic << std::endl;
            } else {
                std::cerr << "[MQTT] 发布失败，存入本地待重传" << std::endl;
            }
        }

        // ==================== 重传检查 ====================
        // 计算本轮采集消耗的时间
        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastTick).count();
        lastTick = now;
        elapsedSinceResend += elapsed;

        // 每隔 RESEND_INTERVAL_SEC 秒，执行一次重传
        if (elapsedSinceResend >= RESEND_INTERVAL_SEC && mqttConnected) {
            std::cout << "===== 触发定时重传 =====" << std::endl;
            tryResendPendingRecords(db, mqtt);
            elapsedSinceResend = 0;  // 重置计数器
        }

        std::this_thread::sleep_for(std::chrono::seconds(pollInterval));
    }

    return 0;
}
