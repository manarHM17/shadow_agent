// common/src/database_manager.cpp
#include "database_manager.h"
#include <iostream>
#include <sstream>
#include <crypt.h>
#include <random>
#include <iomanip>

DatabaseManager::DatabaseManager(const std::string& host, const std::string& user,
                                const std::string& password, const std::string& database)
    : connection_(nullptr), host_(host), user_(user), password_(password), database_(database) {
}

DatabaseManager::~DatabaseManager() {
    disconnect();
}

bool DatabaseManager::connect() {
    connection_ = mysql_init(nullptr);
    if (!connection_) {
        std::cerr << "Failed to initialize MySQL connection" << std::endl;
        return false;
    }
    
    if (!mysql_real_connect(connection_, host_.c_str(), user_.c_str(), 
                           password_.c_str(), database_.c_str(), 0, nullptr, 0)) {
        std::cerr << "Failed to connect to database: " << mysql_error(connection_) << std::endl;
        return false;
    }
    
    initializeDatabase();
    return true;
}

void DatabaseManager::disconnect() {
    if (connection_) {
        mysql_close(connection_);
        connection_ = nullptr;
    }
}

void DatabaseManager::initializeDatabase() {
    std::string create_table_query = R"(
        CREATE TABLE IF NOT EXISTS devices (
            id INT AUTO_INCREMENT PRIMARY KEY,
            hostname VARCHAR(255) UNIQUE NOT NULL,
            password_hash VARCHAR(255) NOT NULL,
            user VARCHAR(255) NOT NULL,
            location VARCHAR(255),
            hardware_type VARCHAR(255),
            os_type VARCHAR(255),
            ip_address VARCHAR(45),
            serial_number VARCHAR(255),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
        )
    )";
    
    executeQuery(create_table_query);
}

bool DatabaseManager::authenticateDevice(const std::string& hostname, const std::string& password) {
    std::string query = "SELECT password_hash FROM devices WHERE hostname = '" + hostname + "'";
    MYSQL_RES* result = executeSelectQuery(query);
    
    if (!result) {
        return false;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return false;
    }
    
    std::string stored_hash = row[0];
    mysql_free_result(result);
    
    return verifyPassword(password, stored_hash);
}

bool DatabaseManager::hostnameExists(const std::string& hostname) {
    std::string query = "SELECT COUNT(*) FROM devices WHERE hostname = '" + hostname + "'";
    MYSQL_RES* result = executeSelectQuery(query);
    
    if (!result) {
        return false;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    
    return count > 0;
}

int DatabaseManager::addDevice(const DeviceData& device) {
    if (hostnameExists(device.hostname)) {
        return -1; // Hostname already exists
    }
    
    std::string hashed_password = hashPassword(device.password_hash);
    
    std::stringstream query;
    query << "INSERT INTO devices (hostname, password_hash, user, location, hardware_type, os_type, ip_address, serial_number) VALUES ('"
          << device.hostname << "', '"
          << hashed_password << "', '"
          << device.user << "', '"
          << device.location << "', '"
          << device.hardware_type << "', '"
          << device.os_type << "', '"
          << device.ip_address << "', '"
          << device.serial_number << "')";
    
    if (executeQuery(query.str())) {
        return mysql_insert_id(connection_);
    }
    
    return -1;
}

bool DatabaseManager::deleteDevice(int device_id) {
    std::string query = "DELETE FROM devices WHERE id = " + std::to_string(device_id);
    return executeQuery(query);
}

bool DatabaseManager::updateDevice(int device_id, const DeviceData& device) {
    std::stringstream query;
    query << "UPDATE devices SET "
          << "user = '" << device.user << "', "
          << "location = '" << device.location << "', "
          << "hardware_type = '" << device.hardware_type << "', "
          << "os_type = '" << device.os_type << "', "
          << "ip_address = '" << device.ip_address << "', "
          << "serial_number = '" << device.serial_number << "' "
          << "WHERE id = " << device_id;
    
    return executeQuery(query.str());
}

std::vector<DeviceData> DatabaseManager::getAllDevices() {
    std::vector<DeviceData> devices;
    std::string query = "SELECT id, hostname, password_hash, user, location, hardware_type, os_type, ip_address, serial_number, created_at, updated_at FROM devices";
    
    MYSQL_RES* result = executeSelectQuery(query);
    if (!result) {
        return devices;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        DeviceData device;
        device.id = std::stoi(row[0]);
        device.hostname = row[1] ? row[1] : "";
        device.password_hash = row[2] ? row[2] : "";
        device.user = row[3] ? row[3] : "";
        device.location = row[4] ? row[4] : "";
        device.hardware_type = row[5] ? row[5] : "";
        device.os_type = row[6] ? row[6] : "";
        device.ip_address = row[7] ? row[7] : "";
        device.serial_number = row[8] ? row[8] : "";
        device.created_at = row[9] ? row[9] : "";
        device.updated_at = row[10] ? row[10] : "";
        
        devices.push_back(device);
    }
    
    mysql_free_result(result);
    return devices;
}

DeviceData DatabaseManager::getDeviceById(int device_id) {
    DeviceData device;
    std::string query = "SELECT id, hostname, password_hash, user, location, hardware_type, os_type, ip_address, serial_number, created_at, updated_at FROM devices WHERE id = " + std::to_string(device_id);
    
    MYSQL_RES* result = executeSelectQuery(query);
    if (!result) {
        return device;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        device.id = std::stoi(row[0]);
        device.hostname = row[1] ? row[1] : "";
        device.password_hash = row[2] ? row[2] : "";
        device.user = row[3] ? row[3] : "";
        device.location = row[4] ? row[4] : "";
        device.hardware_type = row[5] ? row[5] : "";
        device.os_type = row[6] ? row[6] : "";
        device.ip_address = row[7] ? row[7] : "";
        device.serial_number = row[8] ? row[8] : "";
        device.created_at = row[9] ? row[9] : "";
        device.updated_at = row[10] ? row[10] : "";
    }
    
    mysql_free_result(result);
    return device;
}

std::string DatabaseManager::hashPassword(const std::string& password) {
    // Generate salt
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61);
    
    std::string salt_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::string salt = "$6$";
    for (int i = 0; i < 16; ++i) {
        salt += salt_chars[dis(gen)];
    }
    salt += "$";
    
    char* hashed = crypt(password.c_str(), salt.c_str());
    return std::string(hashed);
}

bool DatabaseManager::verifyPassword(const std::string& password, const std::string& hash) {
    char* result = crypt(password.c_str(), hash.c_str());
    return result && hash == std::string(result);
}

bool DatabaseManager::executeQuery(const std::string& query) {
    if (mysql_query(connection_, query.c_str())) {
        std::cerr << "Query failed: " << mysql_error(connection_) << std::endl;
        return false;
    }
    return true;
}

MYSQL_RES* DatabaseManager::executeSelectQuery(const std::string& query) {
    if (mysql_query(connection_, query.c_str())) {
        std::cerr << "Query failed: " << mysql_error(connection_) << std::endl;
        return nullptr;
    }
    return mysql_store_result(connection_);
}