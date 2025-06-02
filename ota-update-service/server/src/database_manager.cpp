// server/database_manager.cpp
#include "../include/database_manager.h"
#include <iostream>
#include <sstream>
#include <cstring>
using namespace ota ;
namespace ota {

DatabaseManager::DatabaseManager() : connection(nullptr), connected(false) {
    connection = mysql_init(nullptr);
}

DatabaseManager::~DatabaseManager() {
    disconnect();
}

bool DatabaseManager::connect(const std::string& host, const std::string& user,
                             const std::string& password, const std::string& database) {
    if (!connection) {
        std::cerr << "Failed to initialize MySQL connection" << std::endl;
        return false;
    }

    if (!mysql_real_connect(connection, host.c_str(), user.c_str(),
                           password.c_str(), database.c_str(), 0, nullptr, 0)) {
        std::cerr << "Connection failed: " << mysql_error(connection) << std::endl;
        return false;
    }

    connected = true;
    return initializeTables();
}

void DatabaseManager::disconnect() {
    if (connection) {
        mysql_close(connection);
        connection = nullptr;
        connected = false;
    }
}

bool DatabaseManager::initializeTables() {
    const char* create_devices = R"(
        CREATE TABLE IF NOT EXISTS devices (
            device_id INT PRIMARY KEY,
            device_type VARCHAR(100) NOT NULL,
            current_version VARCHAR(50) NOT NULL,
            platform VARCHAR(50) NOT NULL,
            last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            status INT DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    const char* create_updates = R"(
        CREATE TABLE IF NOT EXISTS updates (
            update_id INT PRIMARY KEY,
            version VARCHAR(50) NOT NULL,
            description TEXT,
            file_path VARCHAR(500) NOT NULL,
            checksum VARCHAR(64) NOT NULL,
            file_size BIGINT NOT NULL,
            update_type INT NOT NULL,
            target_device_type VARCHAR(100),
            created_at TIMESTAMP gDEFAULT CURRENT_TIMESTAMP
        )
    )";

    const char* create_installations = R"(
        CREATE TABLE IF NOT EXISTS installations (
            id INT AUTO_INCREMENT PRIMARY KEY,
            device_id INT NOT NULL,
            update_id INT NOT NULL,
            success BOOLEAN NOT NULL,
            error_message TEXT,
            installed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (device_id) REFERENCES devices(device_id),
            FOREIGN KEY (update_id) REFERENCES updates(update_id)
        )
    )";

    return (mysql_query(connection, create_devices) == 0) &&
           (mysql_query(connection, create_updates) == 0) &&
           (mysql_query(connection, create_installations) == 0);
}

bool DatabaseManager::registerDevice(const DeviceInfo& device) {
    if (!connected) return false;

    std::ostringstream query;
    query << "INSERT INTO devices (device_id, device_type, current_version, platform, status) "
          << "VALUES (" << device.device_id << ", '"
          << escapeString(device.device_type) << "', '"
          << escapeString(device.current_version) << "', '"
          << escapeString(device.platform) << "', " 
          << static_cast<int>(device.status) << ") "
          << "ON DUPLICATE KEY UPDATE "
          << "current_version='" << escapeString(device.current_version) << "', "
          << "status=" << static_cast<int>(device.status);

    return mysql_query(connection, query.str().c_str()) == 0;
}

bool DatabaseManager::updateDeviceStatus(int32_t device_id, DeviceStatus status) {
    if (!connected) return false;

    std::ostringstream query;
    query << "UPDATE devices SET status=" << static_cast<int>(status)
          << " WHERE device_id=" << device_id;

    return mysql_query(connection, query.str().c_str()) == 0;
}

DeviceInfo DatabaseManager::getDevice(const int32_t device_id) {
    DeviceInfo device;
    if (!connected) return device;

    std::ostringstream query;
    query << "SELECT device_id, device_type, current_version, platform, "
          << "last_seen, status FROM devices WHERE device_id="
          << device_id;

    if (mysql_query(connection, query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(connection);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                device.device_id = row[0] ? std::stoi(row[0]) : 0;
                device.device_type = row[1] ? row[1] : "";
                device.current_version = row[2] ? row[2] : "";
                device.platform = row[3] ? row[3] : "";
                device.last_seen = row[4] ? row[4] : "";
                device.status = row[5] ? static_cast<DeviceStatus>(std::stoi(row[5])) : DeviceStatus::OFFLINE;
            }
            mysql_free_result(result);
        }
    }

    return device;
}

bool DatabaseManager::addUpdate(const UpdateInfo& update) {
    if (!connected) return false;

    std::ostringstream query;
    query << "INSERT INTO updates (update_id, version, description, file_path, "
          << "checksum, file_size, update_type) VALUES ("
          << update.update_id << ", '"
          << escapeString(update.version) << "', '"
          << escapeString(update.description) << "', '"
          << escapeString(update.file_path) << "', '"
          << escapeString(update.checksum) << "', "
          << update.file_size << ", "
          << static_cast<int>(update.update_type) << ")";

    return mysql_query(connection, query.str().c_str()) == 0;
}

UpdateInfo DatabaseManager::getLatestUpdate(const std::string& device_type, UpdateType update_type) {
    UpdateInfo update;
    if (!connected) return update;

    std::ostringstream query;
    query << "SELECT update_id, version, description, file_path, checksum, "
          << "file_size, update_type, created_at FROM updates "
          << "WHERE update_type=" << static_cast<int>(update_type)
          << " ORDER BY created_at DESC LIMIT 1";

    if (mysql_query(connection, query.str().c_str()) == 0) {
        MYSQL_RES* result = mysql_store_result(connection);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                update.update_id = row[0] ? std::stoi(row[0]) : 0;
                update.version = row[1] ? row[1] : "";
                update.description = row[2] ? row[2] : "";
                update.file_path = row[3] ? row[3] : "";
                update.checksum = row[4] ? row[4] : "";
                update.file_size = row[5] ? std::stoll(row[5]) : 0;
                update.update_type = row[6] ? static_cast<UpdateType>(std::stoi(row[6])) : UpdateType::CONFIG;
                update.created_at = row[7] ? row[7] : "";
            }
            mysql_free_result(result);
        }
    }

    return update;
}

bool DatabaseManager::recordInstallation(const InstallationRecord& record) {
    if (!connected) return false;

    std::ostringstream query;
    query << "INSERT INTO installations (device_id, update_id, success, error_message) "
          << "VALUES (" << record.device_id << ", "
          << record.update_id << ", "
          << (record.success == ota::UpdateStatus::SUCCESS ? "SUCCESS" : "FAILED") << ", '"
          << escapeString(record.error_message) << "')";

    return mysql_query(connection, query.str().c_str()) == 0;
}

std::string DatabaseManager::escapeString(const std::string& str) {
    if (!connected || str.empty() || !connection) return str;

    char* escaped = new char[str.length() * 2 + 1];
    mysql_real_escape_string(connection, escaped, str.c_str(), str.length());
    std::string result(escaped);
    delete[] escaped;
    return result;
}

std::vector<UpdateInfo> DatabaseManager::getAvailableUpdates(int32_t device_id) {
    std::vector<UpdateInfo> updates;

    // Itérer sur tous les types d'update
    for (int type = static_cast<int>(UpdateType::CONFIG);
         type <= static_cast<int>(UpdateType::SYSTEM_SERVICE);
         ++type) 
    {
        UpdateType update_type = static_cast<UpdateType>(type);
        UpdateInfo update = getLatestUpdate("", update_type); // device_type vide pour obtenir tous les updates
        // On considère que 0 est une valeur invalide pour update_id
        if (update.update_id != 0) {
            updates.push_back(update);
        }
    }
    return updates;
}

std::vector<InstallationRecord> DatabaseManager::getInstallationHistory(const std::string& device_id) {
    std::vector<InstallationRecord> records;
    // Implémentation basique pour l'historique
    return records;
}

} // namespace ota