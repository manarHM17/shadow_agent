#include "firmware_manager.h"
#include "../common/include/logging.h"
#include "../common/include/checksum.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <json/json.h>

namespace fs = std::filesystem;

namespace ota {

FirmwareManager::FirmwareManager(const std::string& firmware_directory)
    : firmware_directory_(firmware_directory) {
    
    // S'assurer que le répertoire des firmwares existe
    if (!fs::exists(firmware_directory_)) {
        fs::create_directories(firmware_directory_);
        LOG_INFO("Répertoire de firmwares créé: " + firmware_directory_);
    }
    
    // Charger les informations sur les firmwares disponibles
    LoadAvailableFirmwares();
}

std::optional<FirmwareInfo> FirmwareManager::CheckForUpdate(
    const std::string& device_id,
    const std::string& current_version,
    const std::string& hardware_model) {
    
    // Trouver le firmware le plus récent compatible avec ce matériel
    std::optional<FirmwareInfo> latest_firmware;
    
    for (const auto& [version, firmware] : available_firmwares_) {
        // Vérifier si le firmware est compatible avec ce modèle de matériel
        if (firmware.target_hardware == hardware_model) {
            // Vérifier si la version actuelle est dans la liste des versions compatibles
            bool is_compatible = false;
            for (const auto& compat_version : firmware.compatible_versions) {
                if (compat_version == current_version) {
                    is_compatible = true;
                    break;
                }
            }
            
            if (!is_compatible) {
                continue;
            }
            
            // Comparer avec la version actuelle (simple comparaison lexicographique)
            // Dans un système réel, il faudrait utiliser une méthode plus robuste de comparaison de versions
            if (version > current_version) {
                // Si c'est la première version compatible trouvée ou si elle est plus récente que celle trouvée précédemment
                if (!latest_firmware.has_value() || version > latest_firmware->version) {
                    latest_firmware = firmware;
                }
            }
        }
    }
    
    return latest_firmware;
}

std::string FirmwareManager::GetFirmwarePath(const std::string& version) {
    auto it = available_firmwares_.find(version);
    if (it != available_firmwares_.end()) {
        return it->second.path;
    }
    return "";
}

bool FirmwareManager::AddFirmware(const FirmwareInfo& firmware_info) {
    // Vérifier si le fichier firmware existe
    if (!fs::exists(firmware_info.path)) {
        LOG_ERROR("Fichier firmware introuvable: " + firmware_info.path);
        return false;
    }
    
    // Vérifier le checksum
    std::string computed_checksum = common::CalculateChecksum(firmware_info.path);
    if (computed_checksum != firmware_info.checksum) {
        LOG_ERROR("Validation du checksum échouée pour " + firmware_info.path +
                  " (attendu: " + firmware_info.checksum + 
                  ", calculé: " + computed_checksum + ")");
        return false;
    }
    
    // Vérifier la taille
    uint64_t actual_size = fs::file_size(firmware_info.path);
    if (actual_size != firmware_info.size) {
        LOG_ERROR("Taille de fichier incorrecte pour " + firmware_info.path +
                  " (attendu: " + std::to_string(firmware_info.size) + 
                  ", réel: " + std::to_string(actual_size) + ")");
        return false;
    }
    
    // Ajouter à la liste des firmwares disponibles
    available_firmwares_[firmware_info.version] = firmware_info;
    
    // Mettre à jour le fichier de configuration
    SaveFirmwareConfig();
    
    LOG_INFO("Firmware ajouté: " + firmware_info.version + 
             " pour " + firmware_info.target_hardware);
    
    return true;
}

bool FirmwareManager::RemoveFirmware(const std::string& version) {
    auto it = available_firmwares_.find(version);
    if (it == available_firmwares_.end()) {
        LOG_ERROR("Firmware non trouvé pour la suppression: " + version);
        return false;
    }
    
    std::string firmware_path = it->second.path;
    
    // Supprimer de la liste
    available_firmwares_.erase(it);
    
    // Mettre à jour le fichier de configuration
    SaveFirmwareConfig();
    
    // Supprimer le fichier (optionnel)
    try {
        if (fs::exists(firmware_path)) {
            fs::remove(firmware_path);
            LOG_INFO("Fichier firmware supprimé: " + firmware_path);
        }
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR("Erreur lors de la suppression du fichier firmware: " + 
                 std::string(e.what()));
    }
    
    LOG_INFO("Firmware supprimé: " + version);
    return true;
}

std::vector<FirmwareInfo> FirmwareManager::ListAvailableFirmwares() {
    std::vector<FirmwareInfo> result;
    result.reserve(available_firmwares_.size());
    
    for (const auto& [version, info] : available_firmwares_) {
        result.push_back(info);
    }
    
    return result;
}

void FirmwareManager::LoadAvailableFirmwares() {
    available_firmwares_.clear();
    
    std::string config_path = firmware_directory_ + "/firmware_config.json";
    
    if (!fs::exists(config_path)) {
        LOG_INFO("Fichier de configuration des firmwares non trouvé, création d'un nouveau fichier");
        SaveFirmwareConfig();
        return;
    }
    
    try {
        // Ouvrir et lire le fichier de configuration
        std::ifstream config_file(config_path);
        Json::Value root;
        config_file >> root;
        
        // Parcourir les firmwares dans le fichier de configuration
        for (const auto& version_name : root.getMemberNames()) {
            const auto& firmware_json = root[version_name];
            
            FirmwareInfo info;
            info.version = version_name;
            info.target_hardware = firmware_json["target_hardware"].asString();
            info.size = firmware_json["size"].asUInt64();
            info.checksum = firmware_json["checksum"].asString();
            info.changelog = firmware_json["changelog"].asString();
            info.path = firmware_json["path"].asString();
            
            // Lire les versions compatibles
            const auto& compat_versions = firmware_json["compatible_versions"];
            for (const auto& compat_version : compat_versions) {
                info.compatible_versions.push_back(compat_version.asString());
            }
            
            // Vérifier si le fichier existe
            if (fs::exists(info.path)) {
                available_firmwares_[info.version] = info;
            } else {
                LOG_WARNING("Fichier firmware manquant pour la version " + 
                           info.version + ": " + info.path);
            }
        }
        
        LOG_INFO("Chargé " + std::to_string(available_firmwares_.size()) + 
                " firmwares disponibles");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Erreur lors du chargement des firmwares: " + 
                 std::string(e.what()));
    }
}

void FirmwareManager::SaveFirmwareConfig() {
    std::string config_path = firmware_directory_ + "/firmware_config.json";
    
    try {
        Json::Value root;
        
        // Écrire les informations de chaque firmware
        for (const auto& [version, info] : available_firmwares_) {
            Json::Value firmware_json;
            firmware_json["target_hardware"] = info.target_hardware;
            firmware_json["size"] = Json::Value::UInt64(info.size);
            firmware_json["checksum"] = info.checksum;
            firmware_json["changelog"] = info.changelog;
            firmware_json["path"] = info.path;
            
            // Ajouter les versions compatibles
            Json::Value compat_versions(Json::arrayValue);
            for (const auto& compat_version : info.compatible_versions) {
                compat_versions.append(compat_version);
            }
            firmware_json["compatible_versions"] = compat_versions;
            
            root[version] = firmware_json;
        }
        
        // Écrire dans le fichier
        std::ofstream config_file(config_path);
        config_file << root;
        
        LOG_INFO("Configuration des firmwares sauvegardée: " + config_path);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Erreur lors de la sauvegarde de la configuration des firmwares: " + 
                 std::string(e.what()));
    }
}

} // namespace ota