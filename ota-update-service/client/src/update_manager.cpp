// client/update_manager.cpp (suite et fin)
#include "../include/update_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <iomanip>

namespace ota {

UpdateManager::UpdateManager() {}

UpdateManager::~UpdateManager() {}

void UpdateManager::setProgressCallback(std::function<void(int)> callback) {
    progress_callback_ = callback;
}

void UpdateManager::setLogCallback(std::function<void(const std::string&)> callback) {
    log_callback_ = callback;
}

void UpdateManager::logMessage(const std::string& message) {
    std::cout << "[UpdateManager] " << message << std::endl;
    if (log_callback_) {
        log_callback_(message);
    }
}

void UpdateManager::updateProgress(int percentage) {
    if (progress_callback_) {
        progress_callback_(percentage);
    }
}

bool UpdateManager::executeCommand(const std::string& command, std::string& output) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return false;
    }
    
    char buffer[128];
    std::ostringstream result;
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result << buffer;
    }
    
    int exit_code = pclose(pipe);
    output = result.str();
    
    return exit_code == 0;
}

std::string UpdateManager::calculateSHA256(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

bool UpdateManager::verifyChecksum(const std::string& file_path, const std::string& expected_checksum) {
    logMessage("Verifying checksum for: " + file_path);
    std::string calculated = calculateSHA256(file_path);
    
    if (calculated.empty()) {
        logMessage("ERROR: Failed to calculate checksum");
        return false;
    }
    
    if (calculated != expected_checksum) {
        logMessage("ERROR: Checksum mismatch. Expected: " + expected_checksum + ", Got: " + calculated);
        return false;
    }
    
    logMessage("Checksum verification successful");
    return true;
}

bool UpdateManager::backupCurrentVersion(const std::string& target_path, const std::string& backup_path) {
    logMessage("Creating backup: " + target_path + " -> " + backup_path);
    
    struct stat buffer;
    if (stat(target_path.c_str(), &buffer) != 0) {
        logMessage("WARNING: Target file doesn't exist, skipping backup");
        return true;
    }
    
    std::string copy_command = "cp \"" + target_path + "\" \"" + backup_path + "\"";
    std::string output;
    
    return executeCommand(copy_command, output);
}

bool UpdateManager::rollbackUpdate(const std::string& backup_path, const std::string& target_path) {
    logMessage("Rolling back update: " + backup_path + " -> " + target_path);
    
    std::string copy_command = "cp \"" + backup_path + "\" \"" + target_path + "\"";
    std::string output;
    
    return executeCommand(copy_command, output);
}

bool UpdateManager::installConfigUpdate(const std::string& update_file, const std::string& target_path) {
    logMessage("Installing configuration update: " + target_path);
    updateProgress(10);
    
    // Vérifier que le fichier de mise à jour existe
    struct stat buffer;
    if (stat(update_file.c_str(), &buffer) != 0) {
        logMessage("ERROR: Update file not found: " + update_file);
        return false;
    }
    
    updateProgress(25);
    
    // Créer une sauvegarde de la configuration actuelle
    std::string backup_path = target_path + ".backup." + std::to_string(time(nullptr));
    if (!backupCurrentVersion(target_path, backup_path)) {
        logMessage("WARNING: Failed to create backup");
    }
    
    updateProgress(50);
    
    // Copier le nouveau fichier de configuration
    std::string copy_command = "cp \"" + update_file + "\" \"" + target_path + "\"";
    std::string output;
    
    if (!executeCommand(copy_command, output)) {
        logMessage("ERROR: Failed to copy configuration file");
        return false;
    }
    
    updateProgress(75);
    
    // Valider la syntaxe JSON si c'est un fichier .json
    if (target_path.find(".json") != std::string::npos) {
        std::string validate_command = "python3 -m json.tool \"" + target_path + "\" > /dev/null 2>&1";
        if (!executeCommand(validate_command, output)) {
            logMessage("ERROR: Invalid JSON configuration file");
            rollbackUpdate(backup_path, target_path);
            return false;
        }
    }
    
    updateProgress(100);
    logMessage("Configuration update completed successfully");
    return true;
}

bool UpdateManager::installApplicationUpdate(const std::string& update_file, const std::string& app_name) {
    logMessage("Installing application update for: " + app_name);
    updateProgress(10);
    
    // Vérifier que le fichier de mise à jour existe
    struct stat buffer;
    if (stat(update_file.c_str(), &buffer) != 0) {
        logMessage("ERROR: Update file not found: " + update_file);
        return false;
    }
    
    updateProgress(20);
    
    // Arrêter le conteneur Docker existant
    std::string stop_command = "docker stop " + app_name + " 2>/dev/null || true";
    std::string output;
    executeCommand(stop_command, output);
    
    updateProgress(40);
    
    // Supprimer l'ancien conteneur
    std::string remove_command = "docker rm " + app_name + " 2>/dev/null || true";
    executeCommand(remove_command, output);
    
    updateProgress(60);
    
    // Charger la nouvelle image Docker
    std::string load_command = "docker load -i \"" + update_file + "\"";
    if (!executeCommand(load_command, output)) {
        logMessage("ERROR: Failed to load Docker image");
        return false;
    }
    
    updateProgress(80);
    
    // Redémarrer le conteneur avec la nouvelle image
    if (!restartDockerContainer(app_name)) {
        logMessage("ERROR: Failed to restart Docker container");
        return false;
    }
    
    updateProgress(100);
    logMessage("Application update completed successfully");
    return true;
}

bool UpdateManager::installSystemdServiceUpdate(const std::string& update_file, const std::string& service_name) {
    logMessage("Installing systemd service update: " + service_name);
    updateProgress(10);
    
    std::string service_path = "/etc/systemd/system/" + service_name + ".service";
    std::string backup_path = service_path + ".backup." + std::to_string(time(nullptr));
    
    updateProgress(25);
    
    // Arrêter le service
    std::string stop_command = "systemctl stop " + service_name;
    std::string output;
    executeCommand(stop_command, output);
    
    updateProgress(40);
    
    // Créer une sauvegarde
    backupCurrentVersion(service_path, backup_path);
    
    updateProgress(60);
    
    // Copier le nouveau fichier de service
    std::string copy_command = "cp \"" + update_file + "\" \"" + service_path + "\"";
    if (!executeCommand(copy_command, output)) {
        logMessage("ERROR: Failed to copy service file");
        return false;
    }
    
    updateProgress(80);
    
    // Recharger systemd et redémarrer le service
    if (!reloadSystemdDaemon()) {
        logMessage("ERROR: Failed to reload systemd daemon");
        rollbackUpdate(backup_path, service_path);
        return false;
    }
    
    if (!restartSystemdService(service_name)) {
        logMessage("ERROR: Failed to restart service");
        rollbackUpdate(backup_path, service_path);
        reloadSystemdDaemon();
        return false;
    }
    
    updateProgress(100);
    logMessage("Systemd service update completed successfully");
    return true;
}

bool UpdateManager::restartSystemdService(const std::string& service_name) {
    logMessage("Restarting systemd service: " + service_name);
    
    std::string command = "systemctl restart " + service_name;
    std::string output;
    
    if (!executeCommand(command, output)) {
        logMessage("ERROR: Failed to restart service " + service_name);
        return false;
    }
    
    // Vérifier que le service est actif
    std::string status_command = "systemctl is-active " + service_name;
    if (!executeCommand(status_command, output)) {
        logMessage("ERROR: Service " + service_name + " is not active after restart");
        return false;
    }
    
    logMessage("Service " + service_name + " restarted successfully");
    return true;
}

bool UpdateManager::reloadSystemdDaemon() {
    logMessage("Reloading systemd daemon");
    
    std::string command = "systemctl daemon-reload";
    std::string output;
    
    return executeCommand(command, output);
}

bool UpdateManager::restartDockerContainer(const std::string& container_name) {
    logMessage("Restarting Docker container: " + container_name);
    
    // Vérifier si le conteneur existe et le démarrer
    std::string start_command = "docker start " + container_name;
    std::string output;
    
    if (!executeCommand(start_command, output)) {
        logMessage("ERROR: Failed to start Docker container " + container_name);
        return false;
    }
    
    // Vérifier que le conteneur est en cours d'exécution
    std::string status_command = "docker ps --filter name=" + container_name + " --format '{{.Names}}'";
    if (!executeCommand(status_command, output) || output.find(container_name) == std::string::npos) {
        logMessage("ERROR: Container " + container_name + " is not running");
        return false;
    }
    
    logMessage("Docker container " + container_name + " restarted successfully");
    return true;
}

} // namespace ota