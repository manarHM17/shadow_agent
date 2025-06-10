#include "mysql_metrics_storage.h"
#include <mysql/mysql.h>
#include <iostream>

MySQLMetricsStorage::MySQLMetricsStorage() {
    conn_ = mysql_init(nullptr);
    
    // Première connexion sans sélectionner de base
    std::cout << "Attempting to connect to MySQL..." << std::endl;
    if (!mysql_real_connect(static_cast<MYSQL*>(conn_), "127.0.0.1", "root", "root", nullptr, 0, nullptr, 0)) {
        std::cerr << "MySQL connection failed: " << mysql_error(static_cast<MYSQL*>(conn_)) << std::endl;
        conn_ = nullptr;
        return;
    }
    std::cout << "Connected to MySQL successfully" << std::endl;

    initDatabase();
}

MySQLMetricsStorage::~MySQLMetricsStorage() {
    if (conn_) mysql_close(static_cast<MYSQL*>(conn_));
}

bool MySQLMetricsStorage::reconnect() {
    std::lock_guard<std::mutex> lock(mysql_mutex_);
    
    if (conn_) {
        mysql_close(static_cast<MYSQL*>(conn_));
    }
    
    conn_ = mysql_init(nullptr);
    if (!conn_) return false;

    // Enable auto-reconnect
    my_bool reconnect = 1;
    mysql_options(static_cast<MYSQL*>(conn_), MYSQL_OPT_RECONNECT, &reconnect);
    
    if (!mysql_real_connect(static_cast<MYSQL*>(conn_), "127.0.0.1", "root", "root", nullptr, 0, nullptr, 0)) {
        std::cerr << "MySQL reconnection failed: " << mysql_error(static_cast<MYSQL*>(conn_)) << std::endl;
        return false;
    }

    return initDatabase();
}

bool MySQLMetricsStorage::initDatabase() {
    if (!conn_) return false;

    // Créer la base si elle n'existe pas
    std::cout << "Creating database if not exists..." << std::endl;
    if (mysql_query(static_cast<MYSQL*>(conn_), "CREATE DATABASE IF NOT EXISTS IOTSHADOW")) {
        std::cerr << "Failed to create database: " << mysql_error(static_cast<MYSQL*>(conn_)) << std::endl;
        return false;
    }

    // Sélectionner la base
    std::cout << "Selecting database IOTSHADOW..." << std::endl;
    if (mysql_select_db(static_cast<MYSQL*>(conn_), "IOTSHADOW")) {
        std::cerr << "Failed to select database: " << mysql_error(static_cast<MYSQL*>(conn_)) << std::endl;
        return false;
    }

    // Création des tables
    const char* create_hw_table =
        "CREATE TABLE IF NOT EXISTS hardware_info ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "device_id VARCHAR(128),"
        "readable_date VARCHAR(32),"
        "cpu_usage VARCHAR(16),"
        "memory_usage VARCHAR(16),"
        "disk_usage VARCHAR(16),"
        "usb_state TEXT,"
        "gpio_state INT,"
        "kernel_version VARCHAR(64),"
        "hardware_model VARCHAR(128),"
        "firmware_version VARCHAR(128),"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")";

    if (mysql_query(static_cast<MYSQL*>(conn_), create_hw_table)) {
        std::cerr << "Failed to create hardware_info table: " << mysql_error(static_cast<MYSQL*>(conn_)) << std::endl;
        return false;
    }

    const char* create_sw_table =
        "CREATE TABLE IF NOT EXISTS software_info ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "device_id VARCHAR(128),"
        "readable_date VARCHAR(32),"
        "ip_address VARCHAR(64),"
        "uptime VARCHAR(64),"
        "network_status VARCHAR(32),"
        "os_version VARCHAR(128),"
        "applications TEXT,"
        "services TEXT,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")";

    if (mysql_query(static_cast<MYSQL*>(conn_), create_sw_table)) {
        std::cerr << "Failed to create software_info table: " << mysql_error(static_cast<MYSQL*>(conn_)) << std::endl;
        return false;
    }
    return true;
}

bool MySQLMetricsStorage::executeQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(mysql_mutex_);
    
    if (!conn_) {
        if (!reconnect()) return false;
    }

    if (mysql_query(static_cast<MYSQL*>(conn_), query.c_str())) {
        // Check if connection was lost
        if (mysql_errno(static_cast<MYSQL*>(conn_)) == CR_SERVER_LOST ||
            mysql_errno(static_cast<MYSQL*>(conn_)) == CR_SERVER_GONE_ERROR) {
            
            // Try to reconnect once
            if (reconnect()) {
                // Retry query after successful reconnection
                if (!mysql_query(static_cast<MYSQL*>(conn_), query.c_str())) {
                    return true;
                }
            }
        }
        std::cerr << "MySQL query error: " << mysql_error(static_cast<MYSQL*>(conn_)) << std::endl;
        return false;
    }
    return true;
}

bool MySQLMetricsStorage::insertHardwareInfo(const nlohmann::json& m) {
    if (!conn_) {
        std::cerr << "No MySQL connection available" << std::endl;
        return false;
    }

    // Log the full JSON for debugging
    std::cout << "Inserting hardware metrics: " << m.dump(2) << std::endl;
    
    std::string query =
        "INSERT INTO hardware_info (device_id, readable_date, cpu_usage, memory_usage, disk_usage, usb_state, gpio_state, kernel_version, hardware_model, firmware_version) VALUES ('" +
        m.value("device_id", "unknown") + "','" +  // Add default "unknown"
        m.value("readable_date", "") + "','" +
        m.value("cpu_usage", "") + "','" +
        m.value("memory_usage", "") + "','" +
        m.value("disk_usage", "") + "','" +
        m.value("usb_state", "") + "'," +
        std::to_string(m.value("gpio_state", 0)) + ",'" +
        m.value("kernel_version", "") + "','" +
        m.value("hardware_model", "") + "','" +
        m.value("firmware_version", "") + "')";
    std::cout << "Executing hardware query: " << query << std::endl;
    return executeQuery(query);
}

bool MySQLMetricsStorage::insertSoftwareInfo(const nlohmann::json& m) {
    if (!conn_) {
        std::cerr << "No MySQL connection available" << std::endl;
        return false;
    }

    // Log the full JSON for debugging
    std::cout << "Inserting software metrics: " << m.dump(2) << std::endl;
    
    std::string apps;
    if (m.contains("applications")) {
        for (const auto& app : m["applications"]) {
            if (!apps.empty()) apps += ";";
            apps += app.value("name", "") + ":" + app.value("version", "");
        }
    }
    std::string services;
    if (m.contains("services")) {
        for (auto& [k, v] : m["services"].items()) {
            if (!services.empty()) services += ";";
            services += k + ":" + v.get<std::string>();
        }
    }
    std::string query =
        "INSERT INTO software_info (device_id, readable_date, ip_address, uptime, network_status, os_version, applications, services) VALUES ('" +
        m.value("device_id", "unknown") + "','" +  // Add default "unknown"
        m.value("readable_date", "") + "','" +
        m.value("ip_address", "") + "','" +
        m.value("uptime", "") + "','" +
        m.value("network_status", "") + "','" +
        m.value("os_version", "") + "','" +
        apps + "','" +
        services + "')";
    std::cout << "Executing software query: " << query << std::endl;
    return executeQuery(query);
}
