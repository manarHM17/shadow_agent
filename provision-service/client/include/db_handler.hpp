#pragma once

#include <mysql/mysql.h>
#include <string>

class DBHandler {
public:
    DBHandler();
    ~DBHandler();

    // Disable copy
    DBHandler(const DBHandler&) = delete;
    DBHandler& operator=(const DBHandler&) = delete;

    MYSQL* getConnection();
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelect(const std::string& query);  // <<=== ADD THIS LINE
    bool insertDevice(const std::string& hostname, const std::string& type, const std::string& os_type,
                      const std::string& username, const std::string& token);
    std::string getLastError();

private:
    MYSQL* conn;
    void clearPreviousResults();
};
