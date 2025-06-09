#include "../include/db_handler.h"
#include <stdexcept>
#include <iostream>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <regex>

DBHandler::DBHandler() {
    
    // const char* host = std::getenv("DB_HOST");
    // const char* user = std::getenv("DB_USER");
    // const char* pass = std::getenv("DB_PASS");
    // const char* db   = std::getenv("DB_NAME");

    conn = mysql_init(nullptr);
    if (!conn) {
        throw std::runtime_error("MySQL initialization failed");
    }

    if (!mysql_real_connect(conn, "127.0.0.1", "root", "root", "IOTSHADOW", 3306, nullptr, 0))
     {
        std::string err = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("MySQL connection failed: " + err);
    }
  

    // Ensure the database exists or create it
    if (mysql_query(conn, "CREATE DATABASE IF NOT EXISTS IOTSHADOW")) {
        std::string error = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("Failed to create database: " + error);
    }


    // Update table creation query in constructor
    const char* create_devices_table_query =
        "CREATE TABLE IF NOT EXISTS devices ("
        "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
        "hostname VARCHAR(255) NOT NULL UNIQUE,"
        "password_hash VARCHAR(255) NOT NULL,"
        "user VARCHAR(255) NOT NULL,"
        "location VARCHAR(255),"
        "hardware_type VARCHAR(255) NOT NULL,"
        "os_type VARCHAR(255) NOT NULL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "token VARCHAR(512),"
        "INDEX idx_hostname (hostname)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci";

    if (mysql_query(conn, create_devices_table_query)) {
        std::string error = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("Failed to create devices table: " + error);
    }
}

DBHandler::~DBHandler() {
    if (conn) {
        mysql_close(conn); // Always close the connection when the object is destroyed
    }
}

MYSQL* DBHandler::getConnection() {
    if (!conn) {
        throw std::runtime_error("MySQL connection is not initialized");
    }
    clearPreviousResults();
    return conn;
}

bool DBHandler::executeQuery(const std::string& query) {
    clearPreviousResults();
    if (mysql_query(conn, query.c_str()) != 0) {
        std::cerr << "MySQL query error: " << mysql_error(conn) << std::endl;
        return false;
    }
    return true;
}

MYSQL_RES* DBHandler::executeSelect(const std::string& query) {
    clearPreviousResults();
    if (mysql_query(conn, query.c_str()) != 0) {
        std::cerr << "MySQL query error: " << mysql_error(conn) << std::endl;
        return nullptr;
    }
    return mysql_store_result(conn);
}

bool DBHandler::authenticateDevice(const std::string& hostname, const std::string& password) {
    char escaped_hostname[hostname.length()*2+1];
    mysql_real_escape_string(conn, escaped_hostname, hostname.c_str(), hostname.length());
    
    std::string query = "SELECT password_hash FROM devices WHERE hostname = '" + 
                        std::string(escaped_hostname) + "'";
    
    MYSQL_RES* result = executeSelect(query);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return false;
    }
    
    std::string stored_hash = row[0];
    mysql_free_result(result);
    
    return stored_hash == hashPassword(password);
}

std::vector<DeviceData> DBHandler::getAllDevices() {
    std::vector<DeviceData> devices;
    const char* query = "SELECT id, hostname, user, location, hardware_type, os_type, "
                       "created_at, updated_at FROM devices";
    
    MYSQL_RES* result = executeSelect(query);
    if (!result) return devices;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        DeviceData device;
        device.id = std::stoi(row[0]);
        device.hostname = row[1] ? row[1] : "";
        device.user = row[2] ? row[2] : "";
        device.location = row[3] ? row[3] : "";
        device.hardware_type = row[4] ? row[4] : "";
        device.os_type = row[5] ? row[5] : "";
        device.created_at = row[6] ? row[6] : "";
        device.updated_at = row[7] ? row[7] : "";
        devices.push_back(device);
    }
    
    mysql_free_result(result);
    return devices;
}

DeviceData DBHandler::getDeviceById(int device_id) {
    DeviceData device;
    std::string query = "SELECT id, hostname, user, location, hardware_type, os_type, "
                       "created_at, updated_at FROM devices "
                       "WHERE id = " + std::to_string(device_id);
    
    MYSQL_RES* result = executeSelect(query);
    if (!result) return device;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        device.id = std::stoi(row[0]);
        device.hostname = row[1] ? row[1] : "";
        device.user = row[2] ? row[2] : "";
        device.location = row[3] ? row[3] : "";
        device.hardware_type = row[4] ? row[4] : "";
        device.os_type = row[5] ? row[5] : "";
        device.created_at = row[6] ? row[6] : "";
        device.updated_at = row[7] ? row[7] : "";
    }
    
    mysql_free_result(result);
    return device;
}

DeviceData DBHandler::getDeviceByHostname(const std::string& hostname) {
    DeviceData device;
    char escaped_hostname[hostname.length()*2+1];
    mysql_real_escape_string(conn, escaped_hostname, hostname.c_str(), hostname.length());
    
    std::string query = "SELECT id, hostname, user, location, hardware_type, os_type, "
                       "created_at, updated_at FROM devices "
                       "WHERE hostname = '" + std::string(escaped_hostname) + "'";
    
    MYSQL_RES* result = executeSelect(query);
    if (!result) return device;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        device.id = std::stoi(row[0]);
        device.hostname = row[1] ? row[1] : "";
        device.user = row[2] ? row[2] : "";
        device.location = row[3] ? row[3] : "";
        device.hardware_type = row[4] ? row[4] : "";
        device.os_type = row[5] ? row[5] : "";
        device.created_at = row[6] ? row[6] : "";
        device.updated_at = row[7] ? row[7] : "";
    }
    
    mysql_free_result(result);
    return device;
}

bool DBHandler::hostnameExists(const std::string& hostname) {
    char escaped_hostname[hostname.length()*2+1];
    mysql_real_escape_string(conn, escaped_hostname, hostname.c_str(), hostname.length());
    
    std::string query = "SELECT COUNT(*) FROM devices WHERE hostname = '" + 
                        std::string(escaped_hostname) + "'";
    
    MYSQL_RES* result = executeSelect(query);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    bool exists = row && std::stoi(row[0]) > 0;
    
    mysql_free_result(result);
    return exists;
}

int DBHandler::addDevice(const DeviceData& device) {
    char escaped_hostname[device.hostname.length()*2+1];
    char escaped_user[device.user.length()*2+1];
    char escaped_location[device.location.length()*2+1];
    char escaped_hw_type[device.hardware_type.length()*2+1];
    char escaped_os_type[device.os_type.length()*2+1];
    
    mysql_real_escape_string(conn, escaped_hostname, device.hostname.c_str(), device.hostname.length());
    mysql_real_escape_string(conn, escaped_user, device.user.c_str(), device.user.length());
    mysql_real_escape_string(conn, escaped_location, device.location.c_str(), device.location.length());
    mysql_real_escape_string(conn, escaped_hw_type, device.hardware_type.c_str(), device.hardware_type.length());
    mysql_real_escape_string(conn, escaped_os_type, device.os_type.c_str(), device.os_type.length());
    
    std::string hashed_password = hashPassword(device.password_hash);
    
    std::string query = "INSERT INTO devices (hostname, password_hash, user, location, "
                       "hardware_type, os_type) VALUES ('" +
                       std::string(escaped_hostname) + "', '" +
                       hashed_password + "', '" +
                       std::string(escaped_user) + "', '" +
                       std::string(escaped_location) + "', '" +
                       std::string(escaped_hw_type) + "', '" +
                       std::string(escaped_os_type) + "')";
    
    if (!executeQuery(query)) {
        return 0;
    }
    return mysql_insert_id(conn);
}

bool DBHandler::deleteDevice(int device_id) {
    std::string query = "DELETE FROM devices WHERE id = " + std::to_string(device_id);
    return executeQuery(query);
}

bool DBHandler::updateDevice(int device_id, const DeviceData& device) {
    char escaped_user[device.user.length()*2+1];
    char escaped_location[device.location.length()*2+1];
    char escaped_hw_type[device.hardware_type.length()*2+1];
    char escaped_os_type[device.os_type.length()*2+1];
    
    mysql_real_escape_string(conn, escaped_user, device.user.c_str(), device.user.length());
    mysql_real_escape_string(conn, escaped_location, device.location.c_str(), device.location.length());
    mysql_real_escape_string(conn, escaped_hw_type, device.hardware_type.c_str(), device.hardware_type.length());
    mysql_real_escape_string(conn, escaped_os_type, device.os_type.c_str(), device.os_type.length());
    
    std::string query = "UPDATE devices SET "
                       "user = '" + std::string(escaped_user) + "', "
                       "location = '" + std::string(escaped_location) + "', "
                       "hardware_type = '" + std::string(escaped_hw_type) + "', "
                       "os_type = '" + std::string(escaped_os_type) + "' "
                       "WHERE id = " + std::to_string(device_id);
    
    return executeQuery(query);
}

std::string DBHandler::hashPassword(const std::string& password) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize digest");
    }

    if (!EVP_DigestUpdate(ctx, password.c_str(), password.length())) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Failed to update digest");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;
    if (!EVP_DigestFinal_ex(ctx, hash, &hashLen)) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize digest");
    }

    EVP_MD_CTX_free(ctx);

    std::stringstream ss;
    for(unsigned int i = 0; i < hashLen; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::string DBHandler::getLastError() {
    return mysql_error(conn);
}

void DBHandler::clearPreviousResults() {
    MYSQL_RES* result;
    while ((result = mysql_store_result(conn)) != nullptr) {
        mysql_free_result(result);
    }
    while (mysql_next_result(conn) == 0) {
        result = mysql_store_result(conn);
        if (result) {
            mysql_free_result(result);
        }
    }
}

