#include "db_handler.h"
#include <iostream>

DBHandler::DBHandler() 
    : host("127.0.0.1"), user("root"), pass("root"), db_name("IOTSHADOW"), conn(nullptr) {
    
    conn = mysql_init(nullptr);
    if (!conn) {
        throw std::runtime_error("MySQL initialization failed");
    }

    // Connect without database first
    if (!mysql_real_connect(conn, host.c_str(), user.c_str(), pass.c_str(), 
                           nullptr, 3306, nullptr, 0)) {
        std::string err = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("MySQL connection failed: " + err);
    }

    // Create database if not exists
    if (mysql_query(conn, "CREATE DATABASE IF NOT EXISTS IOTSHADOW")) {
        std::string error = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("Failed to create database: " + error);
    }

    // Select the database
    if (mysql_select_db(conn, db_name.c_str())) {
        std::string error = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("Failed to select database: " + error);
    }
}

DBHandler::~DBHandler() {
    if (conn) mysql_close(conn);
}

bool DBHandler::Connect() {
    conn = mysql_init(nullptr);
    if (!mysql_real_connect(conn, host.c_str(), user.c_str(), pass.c_str(), db_name.c_str(), 0, nullptr, 0)) {
        std::cerr << "MySQL connection failed: " << mysql_error(conn) << std::endl;
        return false;
    }
    return true;
}

bool DBHandler::InitializeDatabase() {
    std::string updates_table = R"(
        CREATE TABLE IF NOT EXISTS updates (
            id INT AUTO_INCREMENT PRIMARY KEY,
            app_name VARCHAR(255),
            version VARCHAR(64),
            file_path VARCHAR(512),
            checksum VARCHAR(128)
        )
    )";
    std::string status_table = R"(
        CREATE TABLE IF NOT EXISTS update_status (
            id INT AUTO_INCREMENT PRIMARY KEY,
            device_id INT,
            app_name VARCHAR(255),
            current_version VARCHAR(64),
            target_version VARCHAR(64),
            status VARCHAR(32),
            error_message TEXT,
            last_update TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    return Execute(updates_table) && Execute(status_table);
}

bool DBHandler::Execute(const std::string& query) {
    if (mysql_query(conn, query.c_str())) {
        std::cerr << "MySQL query failed: " << mysql_error(conn) << std::endl;
        return false;
    }
    return true;
}

MYSQL_RES* DBHandler::Query(const std::string& query) {
    if (mysql_query(conn, query.c_str())) {
        std::cerr << "MySQL query failed: " << mysql_error(conn) << std::endl;
        return nullptr;
    }
    return mysql_store_result(conn);
}

MYSQL* DBHandler::GetConnection() {
    return conn;
}