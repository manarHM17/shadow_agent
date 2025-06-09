// ota_update_service.cpp
#include "ota_update_service.h"
#include <openssl/sha.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <mysql/mysql.h>

OTAUpdateService::OTAUpdateService(const std::string& storage_path) 
    : file_storage_path(storage_path) {
    // Create directory structure
    std::filesystem::create_directories(storage_path);
    std::filesystem::create_directories(storage_path + "/current");
    std::filesystem::create_directories(storage_path + "/rollback");
    
    // Initialize DB handler with default parameters
    db_handler = std::make_unique<DBHandler>();
    if (!db_handler->InitializeDatabase()) {
        std::cerr << "Failed to initialize database" << std::endl;
    }
}

OTAUpdateService::~OTAUpdateService() = default;

bool OTAUpdateService::InitializeDatabase() {
    return db_handler->InitializeDatabase();
}

std::string OTAUpdateService::CalculateChecksum(const std::vector<char>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

bool OTAUpdateService::ValidateChecksum(const std::vector<char>& data, const std::string& expected_checksum) {
    return CalculateChecksum(data) == expected_checksum;
}

bool OTAUpdateService::UploadUpdatePackage(const UpdatePackage& package, const std::vector<char>& file_data) {
    std::string version_dir = file_storage_path + "/v" + package.version;
    std::filesystem::create_directories(version_dir);

    std::string full_path = version_dir + "/" + package.app_name;
    std::ofstream outfile(full_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Failed to open file for writing: " << full_path << std::endl;
        return false;
    }
    outfile.write(file_data.data(), file_data.size());
    outfile.close();

    std::stringstream ss;
    ss << "INSERT INTO updates (app_name, version, file_path, checksum) VALUES ("
       << "'" << package.app_name << "',"
       << "'" << package.version << "',"
       << "'" << full_path << "',"
       << "'" << package.checksum << "')";
    return db_handler->Execute(ss.str());
}

std::vector<UpdatePackage> OTAUpdateService::GetAvailableUpdates(int32_t device_id, const std::string& app_name, const std::string& current_version) {
    std::vector<UpdatePackage> updates;
    std::string query = "SELECT app_name, version, file_path, checksum FROM updates WHERE app_name='" + app_name + "' AND version > '" + current_version + "'";
    MYSQL_RES* res = db_handler->Query(query);
    if (!res) {
        std::cerr << "Failed to fetch updates from DB" << std::endl;
        return updates;
    }
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        UpdatePackage pkg;
        pkg.app_name   = row[0] ? row[0] : "";
        pkg.version    = row[1] ? row[1] : "";
        pkg.file_path  = row[2] ? row[2] : "";
        pkg.checksum   = row[3] ? row[3] : "";
        updates.push_back(pkg);
    }
    mysql_free_result(res);
    return updates;
}

bool OTAUpdateService::DownloadUpdate(int32_t device_id, const std::string& app_name, std::vector<char>& file_data) {
    std::string query = "SELECT file_path FROM updates WHERE app_name='" + app_name + "' ORDER BY version DESC LIMIT 1";
    MYSQL_RES* res = db_handler->Query(query);
    if (!res) {
        std::cerr << "Failed to fetch file path from DB" << std::endl;
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row || !row[0]) {
        mysql_free_result(res);
        std::cerr << "No file found for app: " << app_name << std::endl;
        return false;
    }
    std::string file_path = row[0];
    mysql_free_result(res);

    std::ifstream infile(file_path, std::ios::binary);
    if (!infile.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return false;
    }
    file_data.assign(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>());
    infile.close();
    return true;
}

bool OTAUpdateService::ReportUpdateStatus(const UpdateStatus& status) {
    std::stringstream ss;
    ss << "INSERT INTO update_status (device_id, app_name, current_version, target_version, status, error_message) VALUES ("
       << status.device_id << ","
       << "'" << status.app_name << "',"
       << "'" << status.current_version << "',"
       << "'" << status.target_version << "',"
       << "'" << status.status << "',"
       << "'" << status.error_message << "')";
    return db_handler->Execute(ss.str());
}