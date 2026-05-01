#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <string>
#include <memory>

/**
 * @brief MQTT客户端（简化版，仅支持发布）
 * 
 * 使用TCP直连，不依赖第三方库
 * 适合学习理解MQTT协议原理
 */
class MqttClient {
public:
    /**
     * @brief 构造函数
     * @param broker_host MQTT服务器地址，如 "localhost"
     * @param broker_port MQTT服务器端口，默认1883
     */
    MqttClient(const std::string& broker_host, int broker_port = 1883);
    
    ~MqttClient();

    /**
     * @brief 连接到MQTT服务器
     * @param client_id 客户端ID，用于标识
     * @return 是否连接成功
     */
    bool connect(const std::string& client_id);

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 发布消息到指定主题
     * @param topic 主题名，如 "dtu/meter/001"
     * @param payload 消息内容（JSON格式）
     * @param qos 服务质量等级（0=最多一次，1=至少一次）
     * @return 是否发布成功
     */
    bool publish(const std::string& topic, const std::string& payload, int qos = 0);

    /**
     * @brief 检查是否已连接
     */
    bool isConnected() const { return connected_; }

private:
    // 内部函数
    bool sendPacket(const uint8_t* data, int len);
    int readPacket();
    bool sendConnectPacket(const std::string& client_id);
    bool sendPublishPacket(const std::string& topic, const std::string& payload, int qos);

    std::string broker_host_;
    int broker_port_;
    int socket_fd_;
    bool connected_;
    
    // 下一个packet的ID（用于QoS 1/2）
    uint16_t next_packet_id_;
};

#endif // MQTT_CLIENT_H
