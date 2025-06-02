#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include "../common/include/ota_service.pb.h"
#include <string>
#include <functional>

namespace ota {

class UpdateManager {
public:
    UpdateManager();
    ~UpdateManager();
    
    // Niveau 1: Gestion des mises à jour simples
    bool installConfigUpdate(const std::string& update_file, const std::string& target_path);
    bool installApplicationUpdate(const std::string& update_file, const std::string& app_name);
    bool installSystemdServiceUpdate(const std::string& update_file, const std::string& service_name);
    
    // Vérification et validation
    bool verifyChecksum(const std::string& file_path, const std::string& expected_checksum);
    bool backupCurrentVersion(const std::string& target_path, const std::string& backup_path);
    bool rollbackUpdate(const std::string& backup_path, const std::string& target_path);
    
    // Utilitaires système
    bool restartSystemdService(const std::string& service_name);
    bool reloadSystemdDaemon();
    bool restartDockerContainer(const std::string& container_name);
    
    // Callbacks pour les événements
    void setProgressCallback(std::function<void(int)> callback);
    void setLogCallback(std::function<void(const std::string&)> callback);
    
private:
    std::function<void(int)> progress_callback_;
    std::function<void(const std::string&)> log_callback_;
    
    bool executeCommand(const std::string& command, std::string& output);
    void logMessage(const std::string& message);
    void updateProgress(int percentage);
    std::string calculateSHA256(const std::string& file_path);
};

} // namespace ota

#endif // UPDATE_MANAGER_H