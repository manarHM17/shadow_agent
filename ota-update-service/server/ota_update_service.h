// ota_update_service.h
#pragma once
#include "db_handler.h"
#include <string>
#include <vector>
#include <memory>

// Structure pour représenter une mise à jour
struct UpdatePackage {
    std::string app_name;
    std::string version;
    std::string file_path;
    std::string checksum;
};

// Structure pour le status de l'update
struct UpdateStatus {
    int32_t device_id;
    std::string app_name;
    std::string current_version;
    std::string target_version;
    std::string status; // PENDING, DOWNLOADING, INSTALLING, SUCCESS, FAILED
    std::string error_message;
    time_t last_update;
};

class OTAUpdateService {
public:
    explicit OTAUpdateService(const std::string& storage_path);
    ~OTAUpdateService();

    // Méthodes principales
    bool InitializeDatabase();
    std::string CalculateChecksum(const std::vector<char>& data);
    bool ValidateChecksum(const std::vector<char>& data, const std::string& expected_checksum);
    bool UploadUpdatePackage(const UpdatePackage& package, const std::vector<char>& file_data);
    std::vector<UpdatePackage> GetAvailableUpdates(int32_t device_id, const std::string& app_name, const std::string& current_version);
    bool DownloadUpdate(int32_t device_id, const std::string& app_name, std::vector<char>& file_data);
    bool ReportUpdateStatus(const UpdateStatus& status);

private:
    std::string file_storage_path;
    std::unique_ptr<DBHandler> db_handler;
};

