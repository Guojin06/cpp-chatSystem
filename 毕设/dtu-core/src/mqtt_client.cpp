#include "mqtt_client.h"
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <iostream>

MqttClient::MqttClient(const std::string& broker_host, int broker_port)
    : broker_host_(broker_host)
    , broker_port_(broker_port)
    , socket_fd_(-1)
    , connected_(false)
    , next_packet_id_(1)
{}

MqttClient::~MqttClient() {
    disconnect();
}

// 连接到MQTT服务器
bool MqttClient::connect(const std::string& client_id) {
    // 1. 创建TCP socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "创建socket失败" << std::endl;
        return false;
    }

    // 2. 解析服务器地址
    struct hostent* he = gethostbyname(broker_host_.c_str());
    if (he == nullptr) {
        std::cerr << "解析服务器地址失败: " << broker_host_ << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // 3. 连接服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(broker_port_);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "连接MQTT服务器失败" << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // 4. 发送MQTT CONNECT报文
    if (!sendConnectPacket(client_id)) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // 5. 等待CONNACK回复
    int packet_type = readPacket();
    if (packet_type != 0x20) {  // 0x20 = CONNACK
        std::cerr << "MQTT连接失败，收到包类型: 0x" << std::hex << packet_type << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    connected_ = true;
    std::cout << "MQTT连接成功" << std::endl;
    return true;
}

void MqttClient::disconnect() {
    if (socket_fd_ >= 0) {
        // 发送DISCONNECT报文（简化版，这里直接关闭socket）
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
}

// 发布消息
bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos) {
    if (!connected_) {
        std::cerr << "未连接，无法发布" << std::endl;
        return false;
    }

    return sendPublishPacket(topic, payload, qos);
}

// 发送数据包
bool MqttClient::sendPacket(const uint8_t* data, int len) {
    if (socket_fd_ < 0) return false;
    
    int sent = send(socket_fd_, data, len, 0);
    return (sent == len);
}

// 读取一个MQTT数据包
int MqttClient::readPacket() {
    if (socket_fd_ < 0) return -1;

    uint8_t header[2];
    int n = recv(socket_fd_, header, 2, 0);
    if (n < 2) return -1;

    // 读取剩余长度（MQTT变长编码）
    int remaining = 0;
    int multiplier = 1;
    uint8_t encoded;
    do {
        n = recv(socket_fd_, &encoded, 1, 0);
        if (n < 1) return -1;
        remaining += (encoded & 0x7F) * multiplier;
        multiplier *= 128;
    } while ((encoded & 0x80) && multiplier < 128 * 128 * 128);

    // 跳过剩余字节（简化版）
    char buf[1024];
    if (remaining > 0) {
        recv(socket_fd_, buf, std::min(remaining, 1024), 0);
    }

    return header[0] & 0xF0;  // 返回包类型
}

// 发送CONNECT报文
bool MqttClient::sendConnectPacket(const std::string& client_id) {
    // MQTT CONNECT 报文结构：
    // [固定头][可变头][载荷]
    
    // 可变头：协议名 + 协议级别 + 连接标志 + Keep Alive
    std::string protocol_name = "MQTT";
    uint8_t protocol_level = 4;  // MQTT 3.1.1
    
    // 连接标志：Clean Session = 1
    uint8_t connect_flags = 0x02;
    
    // Keep Alive：60秒
    uint16_t keep_alive = 60;

    // 计算可变头长度
    int var_header_len = 2 + protocol_name.length() + 1 + 1 + 2;
    
    // 计算载荷长度：client_id长度
    int payload_len = 2 + client_id.length();

    // 剩余长度 = 可变头 + 载荷
    int remaining_len = var_header_len + payload_len;

    // 构建报文
    std::vector<uint8_t> packet;
    
    // 固定头
    packet.push_back(0x10);  // CONNECT包类型 (1) + Reserved (0)
    packet.push_back(static_cast<uint8_t>(remaining_len));

    // 可变头
    // 协议名长度（2字节）+ 协议名
    packet.push_back(static_cast<uint8_t>(protocol_name.length() >> 8));
    packet.push_back(static_cast<uint8_t>(protocol_name.length() & 0xFF));
    for (char c : protocol_name) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    // 协议级别
    packet.push_back(protocol_level);
    
    // 连接标志
    packet.push_back(connect_flags);
    
    // Keep Alive
    packet.push_back(static_cast<uint8_t>(keep_alive >> 8));
    packet.push_back(static_cast<uint8_t>(keep_alive & 0xFF));

    // 载荷：client_id
    packet.push_back(static_cast<uint8_t>(client_id.length() >> 8));
    packet.push_back(static_cast<uint8_t>(client_id.length() & 0xFF));
    for (char c : client_id) {
        packet.push_back(static_cast<uint8_t>(c));
    }

    return sendPacket(packet.data(), packet.size());
}

// 发送PUBLISH报文
bool MqttClient::sendPublishPacket(const std::string& topic, const std::string& payload, int qos) {
    // 计算剩余长度
    int var_header_len = 2 + topic.length();  // 主题长度 + 主题名
    if (qos > 0) {
        var_header_len += 2;  // Packet ID
    }
    int remaining_len = var_header_len + payload.length();

    // 构建报文
    std::vector<uint8_t> packet;
    
    // 固定头
    // PUBLISH = 3，左移4位，+QoS标志
    uint8_t fixed_header = 0x30 | ((qos & 0x03) << 1);
    packet.push_back(fixed_header);

    // 剩余长度（变长编码，最多3字节）
    int remaining = remaining_len;
    do {
        uint8_t encoded = remaining % 128;
        remaining /= 128;
        if (remaining > 0) {
            encoded |= 0x80;
        }
        packet.push_back(encoded);
    } while (remaining > 0);

    // 可变头：主题
    packet.push_back(static_cast<uint8_t>(topic.length() >> 8));
    packet.push_back(static_cast<uint8_t>(topic.length() & 0xFF));
    for (char c : topic) {
        packet.push_back(static_cast<uint8_t>(c));
    }

    // Packet ID (QoS 1/2需要)
    if (qos > 0) {
        uint16_t pid = next_packet_id_++;
        packet.push_back(static_cast<uint8_t>(pid >> 8));
        packet.push_back(static_cast<uint8_t>(pid & 0xFF));
    }

    // 载荷：消息内容
    for (char c : payload) {
        packet.push_back(static_cast<uint8_t>(c));
    }

    return sendPacket(packet.data(), packet.size());
}
