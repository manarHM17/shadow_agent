#include "db_handler.hpp"
#include <stdexcept>
#include <iostream>

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

bool DBHandler::insertDevice(const std::string& hostname, const std::string& type, const std::string& os_type,
                             const std::string& username, const std::string& token) {
    // Allocate buffers for escaped strings (2*length+1 is the safe size for mysql_real_escape_string)
    char escaped_hostname[hostname.length()*2+1];
    char escaped_type[type.length()*2+1];
    char escaped_os_type[os_type.length()*2+1];
    char escaped_username[username.length()*2+1];
    char escaped_token[token.length()*2+1];
    
    // Escape input strings to prevent SQL injection
    mysql_real_escape_string(conn, escaped_hostname, hostname.c_str(), hostname.length());
    mysql_real_escape_string(conn, escaped_type, type.c_str(), type.length());
    mysql_real_escape_string(conn, escaped_os_type, os_type.c_str(), os_type.length());
    mysql_real_escape_string(conn, escaped_username, username.c_str(), username.length());
    mysql_real_escape_string(conn, escaped_token, token.c_str(), token.length());
    
    std::string query = "INSERT INTO devices (hostname, type, os_type, username, token) VALUES ('" +
                        std::string(escaped_hostname) + "', '" + 
                        std::string(escaped_type) + "', '" + 
                        std::string(escaped_os_type) + "', '" + 
                        std::string(escaped_username) + "', '" + 
                        std::string(escaped_token) + "')";
    
    return executeQuery(query);
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
