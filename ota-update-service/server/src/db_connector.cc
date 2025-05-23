#include "db_connector.h"
#include "../common/include/logging.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace ota {

DbConnector::DbConnector(const std::string& host, const std::string& user,
                         const std::string& password, const std::string& database)
    : host_(host), user_(user), password_(password), database_(database), db_(nullptr) {
    db_ = mysql_init(nullptr);
    if (!db_) {
        LOG_ERROR("Failed to initialize MySQL");
        return;
    }
    if (!mysql_real_connect(db_, host_.c_str(), user_.c_str(), password_.c_str(),
                            database_.c_str(), 0, nullptr, 0)) {
        LOG_ERROR("MySQL connection failed: " + std::string(mysql_error(db_)));
        mysql_close(db_);
        db_ = nullptr;
    }
}

DbConnector::~DbConnector() {
    if (db_) {
        mysql_close(db_);
    }
}

bool DbConnector::Initialize() {
    if (!db_) {
        LOG_ERROR("Database not initialized");
        return false;
    }

    const std::vector<std::string> schema = {
        "CREATE TABLE IF NOT EXISTS devices ("
        "  device_id VARCHAR(255) PRIMARY KEY,"
        "  hardware_model VARCHAR(255),"
        "  current_version VARCHAR(50),"
        "  first_seen DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  last_seen DATETIME"
        ");",

        "CREATE TABLE IF NOT EXISTS update_checks ("
        "  id INT AUTO_INCREMENT PRIMARY KEY,"
        "  device_id VARCHAR(255),"
        "  check_time DATETIME,"
        "  current_version VARCHAR(50),"
        "  update_available TINYINT(1),"
        "  available_version VARCHAR(50),"
        "  FOREIGN KEY (device_id) REFERENCES devices(device_id)"
        ");",

        "CREATE TABLE IF NOT EXISTS downloads ("
        "  id INT AUTO_INCREMENT PRIMARY KEY,"
        "  device_id VARCHAR(255),"
        "  version VARCHAR(50),"
        "  start_time DATETIME,"
        "  complete_time DATETIME,"
        "  bytes_downloaded BIGINT,"
        "  status INT,"
        "  FOREIGN KEY (device_id) REFERENCES devices(device_id)"
        ");",

        "CREATE TABLE IF NOT EXISTS update_status ("
        "  id INT AUTO_INCREMENT PRIMARY KEY,"
        "  device_id VARCHAR(255),"
        "  version VARCHAR(50),"
        "  status_time DATETIME,"
        "  status INT,"
        "  error_message TEXT,"
        "  FOREIGN KEY (device_id) REFERENCES devices(device_id)"
        ");"
    };

    for (const auto& query : schema) {
        if (!ExecuteQuery(query)) {
            return false;
        }
    }

    LOG_INFO("Database initialized successfully: " + database_);
    return true;
}

std::string DbConnector::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool DbConnector::LogUpdateCheck(const std::string& device_id, 
                                 const std::string& current_version,
                                 bool update_available,
                                 const std::string& available_version) {
    if (!db_) return false;

    std::string timestamp = GetCurrentTimestamp();

    std::stringstream device_query;
    device_query << "INSERT INTO devices (device_id, current_version, last_seen) "
                 << "VALUES ('" << device_id << "', '"
                 << current_version << "', '"
                 << timestamp << "') "
                 << "ON DUPLICATE KEY UPDATE "
                 << "current_version = '" << current_version << "', "
                 << "last_seen = '" << timestamp << "';";

    if (!ExecuteQuery(device_query.str())) {
        return false;
    }

    std::stringstream check_query;
    check_query << "INSERT INTO update_checks "
                << "(device_id, check_time, current_version, update_available, available_version) "
                << "VALUES ('" << device_id << "', '"
                << timestamp << "', '"
                << current_version << "', "
                << (update_available ? "1" : "0") << ", '"
                << available_version << "');";

    return ExecuteQuery(check_query.str());
}

bool DbConnector::LogDownloadStart(const std::string& device_id,
                                   const std::string& version) {
    if (!db_) return false;

    std::string timestamp = DbConnector::GetCurrentTimestamp();

    std::stringstream query;
    query << "INSERT INTO downloads "
          << "(device_id, version, start_time, status) "
          << "VALUES ('" << device_id << "', '"
          << version << "', '"
          << timestamp << "', 0);";

    return ExecuteQuery(query.str());
}

bool DbConnector::LogDownloadComplete(const std::string& device_id,
                                      const std::string& version,
                                      uint64_t bytes_downloaded) {
    if (!db_) return false;

    std::string timestamp = DbConnector::GetCurrentTimestamp();

    std::stringstream query;
    query << "UPDATE downloads SET "
          << "complete_time = '" << timestamp << "', "
          << "bytes_downloaded = " << bytes_downloaded << ", "
          << "status = 1 "
          << "WHERE device_id = '" << device_id << "' "
          << "AND version = '" << version << "' "
          << "AND complete_time IS NULL;";

    return ExecuteQuery(query.str());
}

bool DbConnector::LogUpdateStatus(const std::string& device_id,
                                  const std::string& version,
                                  int status,
                                  const std::string& error_message) {
    if (!db_) return false;

    std::string timestamp = DbConnector::GetCurrentTimestamp();

    if (status == 4) { // INSTALL_SUCCESS
        std::stringstream update_query;
        update_query << "UPDATE devices SET "
                     << "current_version = '" << version << "' "
                     << "WHERE device_id = '" << device_id << "';";

        if (!ExecuteQuery(update_query.str())) {
            return false;
        }
    }

    std::stringstream query;
    query << "INSERT INTO update_status "
          << "(device_id, version, status_time, status, error_message) "
          << "VALUES ('" << device_id << "', '"
          << version << "', '"
          << timestamp << "', "
          << status << ", '"
          << error_message << "');";

    return ExecuteQuery(query.str());
}

std::vector<DeviceStats> DbConnector::GetDeviceStats(const std::string& device_id) {
    std::vector<DeviceStats> result;
    if (!db_) return result;

    std::stringstream query;
    query << "SELECT d.device_id, d.current_version, d.hardware_model, "
          << "d.last_seen AS last_check_time, "
          << "(SELECT MAX(status_time) FROM update_status us WHERE us.device_id = d.device_id AND us.status = 4) AS last_update_time, "
          << "(SELECT COUNT(*) FROM update_status us WHERE us.device_id = d.device_id AND us.status = 4) AS update_success_count, "
          << "(SELECT COUNT(*) FROM update_status us WHERE us.device_id = d.device_id AND us.status = 5) AS update_failure_count "
          << "FROM devices d ";

    if (!device_id.empty()) {
        query << "WHERE d.device_id = '" << device_id << "' ";
    }

    query << "ORDER BY d.last_seen DESC;";

    if (mysql_query(db_, query.str().c_str()) != 0) {
        LOG_ERROR("SQL execution error: " + std::string(mysql_error(db_)));
        return result;
    }

    MYSQL_RES* res = mysql_store_result(db_);
    if (!res) {
        LOG_ERROR("Result retrieval error: " + std::string(mysql_error(db_)));
        return result;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr)) {
        DeviceStats stats;
        stats.device_id = row[0] ? row[0] : "";
        stats.current_version = row[1] ? row[1] : "";
        stats.hardware_model = row[2] ? row[2] : "";
        stats.last_check_time = row[3] ? row[3] : "";
        stats.last_update_time = row[4] ? row[4] : "";
        stats.update_success_count = row[5] ? std::stoi(row[5]) : 0;
        stats.update_failure_count = row[6] ? std::stoi(row[6]) : 0;
        result.push_back(stats);
    }

    mysql_free_result(res);
    return result;
}

bool DbConnector::CheckHealth() {
    if (!db_) {
        LOG_ERROR("Database not initialized");
        return false;
    }

    if (mysql_query(db_, "SELECT 1") != 0) {
        LOG_ERROR("Database health check failed: " + std::string(mysql_error(db_)));
        return false;
    }

    MYSQL_RES* res = mysql_store_result(db_);
    if (res) {
        mysql_free_result(res);
    }

    return true;
}

bool DbConnector::ExecuteQuery(const std::string& query) {
    if (!db_) return false;

    if (mysql_query(db_, query.c_str()) != 0) {
        LOG_ERROR("SQL execution error: " + std::string(mysql_error(db_)));
        return false;
    }
    return true;
}

} // namespace ota
