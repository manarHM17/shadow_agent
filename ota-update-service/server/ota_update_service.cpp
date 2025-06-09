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
    // Create version-specific directory
    std::string version_dir = file_storage_path + "/v" + package.version;
    std::filesystem::create_directories(version_dir);
    
    // Save file with component name
    std::string full_path = version_dir + "/" + package.component_name + ".zip";
    std::ofstream outfile(full_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Failed to open file for writing: " << full_path << std::endl;
        return false;
    }
    outfile.write(file_data.data(), file_data.size());
    outfile.close();

    // Update current symlink
    std::string current_link = file_storage_path + "/current/" + package.component_name;
    std::filesystem::remove(current_link);  // Remove old symlink if exists
    std::filesystem::create_symlink(full_path, current_link);

    // Insert metadata into DB
    std::stringstream ss;
    ss << "INSERT INTO updates (component_name, version, file_path, checksum, target_path, service_name, is_service, is_config) VALUES ("
       << "'" << package.component_name << "',"
       << "'" << package.version << "',"
       << "'" << full_path << "',"
       << "'" << package.checksum << "',"
       << "'" << package.target_path << "',"
       << "'" << package.service_name << "',"
       << (package.is_service ? "1" : "0") << ","
       << (package.is_config ? "1" : "0") << ")";

    return db_handler->Execute(ss.str());
}

std::vector<UpdatePackage> OTAUpdateService::GetAvailableUpdates(int32_t device_id) {
    std::vector<UpdatePackage> updates;

    std::string query = "SELECT component_name, version, file_path, checksum, target_path, service_name, is_service, is_config FROM updates";
    MYSQL_RES* res = db_handler->Query(query);
    if (!res) {
        std::cerr << "Failed to fetch updates from DB" << std::endl;
        return updates;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        UpdatePackage pkg;
        pkg.component_name = row[0] ? row[0] : "";
        pkg.version        = row[1] ? row[1] : "";
        pkg.file_path      = row[2] ? row[2] : "";
        pkg.checksum       = row[3] ? row[3] : "";
        pkg.target_path    = row[4] ? row[4] : "";
        pkg.service_name   = row[5] ? row[5] : "";
        pkg.is_service     = row[6] ? std::stoi(row[6]) != 0 : false;
        pkg.is_config      = row[7] ? std::stoi(row[7]) != 0 : false;
        updates.push_back(pkg);
    }
    mysql_free_result(res);

    return updates;
}

bool OTAUpdateService::DownloadUpdate(int32_t device_id, const std::string& component_name, std::vector<char>& file_data) {
    std::string query = "SELECT file_path FROM updates WHERE component_name='" + component_name + "' LIMIT 1";
    MYSQL_RES* res = db_handler->Query(query);
    if (!res) {
        std::cerr << "Failed to fetch file path from DB" << std::endl;
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row || !row[0]) {
        mysql_free_result(res);
        std::cerr << "No file found for component: " << component_name << std::endl;
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
    ss << "INSERT INTO update_status (device_id, component_name, current_version, target_version, status, error_message) VALUES ("
       << status.device_id << "," // no quotes for int
       << "'" << status.component_name << "',"
       << "'" << status.current_version << "',"
       << "'" << status.target_version << "',"
       << "'" << status.status << "',"
       << "'" << status.error_message << "')";
    return db_handler->Execute(ss.str());
}