#pragma once
#include <mysql/mysql.h>
#include <string>
#include <memory>
#include <stdexcept>
#include <iostream>

class DBHandler {
public:
    // Constructor: Initializes the MySQL connection and creates necessary tables
    DBHandler() {
        conn = mysql_init(nullptr);
        if (!conn) {
            throw std::runtime_error("MySQL initialization failed");
        }

        if (!mysql_real_connect(conn, "localhost", "iotuser", "iot2025", nullptr, 3306, nullptr, 0)) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("MySQL connection failed: " + error);
        }

        if (mysql_query(conn, "CREATE DATABASE IF NOT EXISTS shadow_agent")) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Failed to create database: " + error);
        }

        if (mysql_select_db(conn, "shadow_agent")) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Failed to select database: " + error);
        }

        const char* create_devices_table_query = 
            "CREATE TABLE IF NOT EXISTS devices ("
            "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
            "hostname VARCHAR(255) NOT NULL,"
            "type VARCHAR(255) NOT NULL,"
            "os_type VARCHAR(255) NOT NULL,"
            "username VARCHAR(255) NOT NULL,"
            "`current_time` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "token VARCHAR(512)"
            ") ENGINE=InnoDB";

        if (mysql_query(conn, create_devices_table_query)) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Failed to create devices table: " + error);
        }

        const char* create_monitoring_table_query = 
            "CREATE TABLE IF NOT EXISTS monitoring ("
            "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
            "device_id VARCHAR(255) NOT NULL,"
            "timestamp BIGINT NOT NULL,"
            "cpu_usage FLOAT NOT NULL,"
            "memory_total_mb INT NOT NULL,"
            "memory_used_mb INT NOT NULL,"
            "disk_usage_root VARCHAR(10) NOT NULL,"
            "uptime VARCHAR(255) NOT NULL,"
            "usb_devices TEXT NOT NULL,"
            "ip_address VARCHAR(45) NOT NULL,"
            "network_status VARCHAR(20) NOT NULL,"
            "services_status JSON NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ") ENGINE=InnoDB";

        if (mysql_query(conn, create_monitoring_table_query)) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Failed to create monitoring table: " + error);
        }
    }

    // Destructor: Closes the MySQL connection
    ~DBHandler() {
        if (conn) {
            mysql_close(conn);
        }
    }

    // Prevent copying of the object
    DBHandler(const DBHandler&) = delete;
    DBHandler& operator=(const DBHandler&) = delete;

    // Get the MySQL connection
    MYSQL* getConnection() { 
        if (!conn) {
            throw std::runtime_error("MySQL connection is not initialized");
        }
        clearPreviousResults();
        return conn; 
    }

    // Execute a query that does not return results (e.g., INSERT, UPDATE, DELETE)
    bool executeQuery(const std::string& query) {
        clearPreviousResults();
        if (mysql_query(conn, query.c_str()) != 0) {
            std::cerr << "MySQL query error: " << mysql_error(conn) << std::endl;
            return false;
        }
        return true;
    }

    // Execute a SELECT query and return the result
    MYSQL_RES* executeSelect(const std::string& query) {
        clearPreviousResults();
        if (mysql_query(conn, query.c_str()) != 0) {
            std::cerr << "MySQL query error: " << mysql_error(conn) << std::endl;
            return nullptr;
        }
        return mysql_store_result(conn);
    }

    // Get the last MySQL error message
    std::string getLastError() {
        return mysql_error(conn);
    }

    // Insert monitoring data into the monitoring table
    bool insertMonitoringData(const std::string& device_id, long timestamp, float cpu_usage, int memory_total_mb,
                              int memory_used_mb, const std::string& disk_usage_root, const std::string& uptime,
                              const std::string& usb_devices, const std::string& ip_address,
                              const std::string& network_status, const std::string& services_status_json) {
        std::string query = "INSERT INTO monitoring (device_id, timestamp, cpu_usage, memory_total_mb, memory_used_mb, "
                            "disk_usage_root, uptime, usb_devices, ip_address, network_status, services_status) VALUES ('" +
                            device_id + "', " + std::to_string(timestamp) + ", " + std::to_string(cpu_usage) + ", " +
                            std::to_string(memory_total_mb) + ", " + std::to_string(memory_used_mb) + ", '" +
                            disk_usage_root + "', '" + uptime + "', '" + usb_devices + "', '" + ip_address + "', '" +
                            network_status + "', '" + services_status_json + "')";
        return executeQuery(query);
    }

private:
    MYSQL* conn;

    // Clear any previous results from the MySQL connection
    void clearPreviousResults() {
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
};

