#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

// sqlite3 的前向声明：只声明存在，不暴露具体结构体内容
struct sqlite3;

#include <sqlite3.h>

// 单条采集记录：存入 SQLite + 上报云端后都依赖此结构
struct MeterRecord {
    int64_t id;              // 自增主键
    std::string meterId;     // 电表编号
    std::string timestamp;   // 采集时间
    double voltage;          // 电压(V)
    double current;          // 电流(A)
    double activeEnergy;     // 有功电能(kWh)
    double powerFactor;      // 功率因数
    std::string rawFrame;    // 原始报文十六进制
    int uploaded;             // 上传状态: 0=未上传, 1=已上传
    int uploadRetry;          // 上传重试次数
    std::string uploadTime;   // 上传时间(成功时记录)
};

// 数据库访问层
class DbHandler {
public:
    // 构造时打开/创建数据库；析构自动关闭
    explicit DbHandler(const std::string& dbPath);
    ~DbHandler();

    // 禁止拷贝（SQLite连接不可拷贝）
    DbHandler(const DbHandler&) = delete;
    DbHandler& operator=(const DbHandler&) = delete;

    // 初始化表结构（若不存在则创建）
    bool init();

    // 插入一条采集记录
    bool insert(const MeterRecord& record);

    // 批量插入（一次事务多条记录，减少IO）
    bool insertBatch(const std::vector<MeterRecord>& records);

    // 查询最近 N 条记录（用于前端历史曲线）
    std::vector<MeterRecord> queryRecent(int limit = 100) const;

    // 查询指定电表的最近 N 条
    std::vector<MeterRecord> queryByMeterId(const std::string& meterId, int limit = 100) const;

    // 查询指定时间范围内的记录
    std::vector<MeterRecord> queryByTimeRange(
        const std::string& startTime,
        const std::string& endTime,
        int limit = 1000) const;

    // 获取所有未上传的记录（断网恢复后补传）
    std::vector<MeterRecord> queryUnuploaded(int limit = 200) const;

    // 标记为已上传（幂等性：按id批量更新）
    bool markUploaded(const std::vector<int64_t>& ids);

    // 标记上传失败，增加重试计数
    bool markUploadFailed(int64_t id);

    // 删除 N 天前的历史数据（自动清理，避免数据库膨胀）
    bool pruneOldRecords(int daysToKeep = 7);

    // 统计：总记录数、待上传数、上传成功率
    struct Stats {
        int64_t total;
        int64_t pending;
        double uploadRate;
    };
    Stats getStats() const;

private:
    std::string dbPath_;
    sqlite3* db_;  // SQLite 数据库句柄

    // 内部：执行任意 SQL（用于建表等）
    bool execSql(const std::string& sql);

    // 内部：参数绑定（INSERT/UPDATE 用）
    bool bindRecord(void* stmt, const MeterRecord& record);
};
