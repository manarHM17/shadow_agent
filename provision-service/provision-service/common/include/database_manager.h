// common/include/database_manager.h
#pragma once

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

struct DeviceData {
    int id;
    std::string hostname;
    std::string password_hash;
    std::string user;
    std::string location;
    std::string hardware_type;
    std::string os_type;
    std::string ip_address;
    std::string serial_number;
    std::string created_at;
    std::string updated_at;
};

class DatabaseManager {
public:
    DatabaseManager(const std::string& host, const std::string& user, 
                   const std::string& password, const std::string& database);
    ~DatabaseManager();
    
    bool connect();
    void disconnect();
    
    // Authentication
    bool authenticateDevice(const std::string& hostname, const std::string& password);
    
    // Device Management
    bool hostnameExists(const std::string& hostname);
    int addDevice(const DeviceData& device);
    bool deleteDevice(int device_id);
    bool updateDevice(int device_id, const DeviceData& device);
    std::vector<DeviceData> getAllDevices();
    DeviceData getDeviceById(int device_id);
    
    // Utility
    std::string hashPassword(const std::string& password);
    bool verifyPassword(const std::string& password, const std::string& hash);
    
private:
    MYSQL* connection_;
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelectQuery(const std::string& query);
    void initializeDatabase();
};

