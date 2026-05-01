#include "db_handler.h"
#include <sqlite3.h>
#include <stdexcept>
#include <iostream>

DbHandler::DbHandler(const std::string& dbPath) : db_(nullptr) {
    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Can't open DB: " + std::string(sqlite3_errmsg(db_)));
    }
}

DbHandler::~DbHandler() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool DbHandler::init() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS meter_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            meter_id TEXT,
            timestamp TEXT,
            voltage REAL,
            current REAL,
            active_energy REAL,
            power_factor REAL,
            raw_frame TEXT,
            uploaded INTEGER DEFAULT 0,
            upload_retry INTEGER DEFAULT 0,
            upload_time TEXT
        )
    )";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool DbHandler::insert(const MeterRecord& record) {
    const char* sql = R"(
        INSERT INTO meter_records 
        (meter_id, timestamp, voltage, current, active_energy, power_factor, raw_frame, uploaded, upload_retry)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, record.meterId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, record.timestamp.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, record.voltage);
    sqlite3_bind_double(stmt, 4, record.current);
    sqlite3_bind_double(stmt, 5, record.activeEnergy);
    sqlite3_bind_double(stmt, 6, record.powerFactor);
    sqlite3_bind_text(stmt, 7, record.rawFrame.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 8, record.uploaded);
    sqlite3_bind_int(stmt, 9, record.uploadRetry);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DbHandler::insertBatch(const std::vector<MeterRecord>& records) {
    if (records.empty()) return true;

    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

    const char* sql = R"(
        INSERT INTO meter_records 
        (meter_id, timestamp, voltage, current, active_energy, power_factor, raw_frame, uploaded, upload_retry)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    for (const auto& r : records) {
        sqlite3_bind_text(stmt, 1, r.meterId.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, r.timestamp.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, r.voltage);
        sqlite3_bind_double(stmt, 4, r.current);
        sqlite3_bind_double(stmt, 5, r.activeEnergy);
        sqlite3_bind_double(stmt, 6, r.powerFactor);
        sqlite3_bind_text(stmt, 7, r.rawFrame.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 8, r.uploaded);
        sqlite3_bind_int(stmt, 9, r.uploadRetry);

        sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
    return true;
}

std::vector<MeterRecord> DbHandler::queryRecent(int limit) const {
    std::vector<MeterRecord> result;
    const char* sql = "SELECT * FROM meter_records ORDER BY id DESC LIMIT ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MeterRecord r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.meterId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.voltage = sqlite3_column_double(stmt, 3);
        r.current = sqlite3_column_double(stmt, 4);
        r.activeEnergy = sqlite3_column_double(stmt, 5);
        r.powerFactor = sqlite3_column_double(stmt, 6);
        r.rawFrame = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        r.uploaded = sqlite3_column_int(stmt, 8);
        r.uploadRetry = sqlite3_column_int(stmt, 9);
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            r.uploadTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        }
        result.push_back(r);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<MeterRecord> DbHandler::queryByMeterId(const std::string& meterId, int limit) const {
    std::vector<MeterRecord> result;
    const char* sql = "SELECT * FROM meter_records WHERE meter_id = ? ORDER BY id DESC LIMIT ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_text(stmt, 1, meterId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MeterRecord r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.meterId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.voltage = sqlite3_column_double(stmt, 3);
        r.current = sqlite3_column_double(stmt, 4);
        r.activeEnergy = sqlite3_column_double(stmt, 5);
        r.powerFactor = sqlite3_column_double(stmt, 6);
        r.rawFrame = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        r.uploaded = sqlite3_column_int(stmt, 8);
        r.uploadRetry = sqlite3_column_int(stmt, 9);
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            r.uploadTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        }
        result.push_back(r);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<MeterRecord> DbHandler::queryByTimeRange(
    const std::string& startTime,
    const std::string& endTime,
    int limit) const 
{
    std::vector<MeterRecord> result;
    const char* sql = R"(
        SELECT * FROM meter_records 
        WHERE timestamp BETWEEN ? AND ?
        ORDER BY id DESC 
        LIMIT ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_text(stmt, 1, startTime.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, endTime.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MeterRecord r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.meterId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.voltage = sqlite3_column_double(stmt, 3);
        r.current = sqlite3_column_double(stmt, 4);
        r.activeEnergy = sqlite3_column_double(stmt, 5);
        r.powerFactor = sqlite3_column_double(stmt, 6);
        r.rawFrame = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        r.uploaded = sqlite3_column_int(stmt, 8);
        r.uploadRetry = sqlite3_column_int(stmt, 9);
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            r.uploadTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        }
        result.push_back(r);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<MeterRecord> DbHandler::queryUnuploaded(int limit) const {
    std::vector<MeterRecord> result;
    const char* sql = "SELECT * FROM meter_records WHERE uploaded = 0 ORDER BY id ASC";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MeterRecord r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.meterId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.voltage = sqlite3_column_double(stmt, 3);
        r.current = sqlite3_column_double(stmt, 4);
        r.activeEnergy = sqlite3_column_double(stmt, 5);
        r.powerFactor = sqlite3_column_double(stmt, 6);
        r.rawFrame = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        r.uploaded = sqlite3_column_int(stmt, 8);
        r.uploadRetry = sqlite3_column_int(stmt, 9);
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            r.uploadTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        }
        result.push_back(r);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool DbHandler::markUploadFailed(int64_t id) {
    const char* sql = "UPDATE meter_records SET upload_retry = upload_retry + 1 WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DbHandler::markUploaded(const std::vector<int64_t>& ids) {
    if (ids.empty()) return true;

    const char* sql = "UPDATE meter_records SET uploaded = 1, upload_time = datetime('now') WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

    for (int64_t id : ids) {
        sqlite3_bind_int64(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
    return true;
}

bool DbHandler::pruneOldRecords(int daysToKeep) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "DELETE FROM meter_records WHERE id IN ("
        "  SELECT id FROM meter_records "
        "  WHERE datetime(timestamp) < datetime('now', '-%d days')"
        ")",
        daysToKeep);
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Prune error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

DbHandler::Stats DbHandler::getStats() const {
    Stats s{0, 0, 0.0};

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM meter_records", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            s.total = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM meter_records WHERE uploaded = 0", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            s.pending = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (s.total > 0) {
        s.uploadRate = (s.total - s.pending) * 100.0 / s.total;
    }
    return s;
}
