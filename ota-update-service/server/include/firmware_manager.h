#ifndef FIRMWARE_MANAGER_H
#define FIRMWARE_MANAGER_H

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <map>

namespace ota {

// Structure contenant les informations sur une mise à jour
struct FirmwareInfo {
    std::string version;
    std::string target_hardware;
    uint64_t size;
    std::string checksum;
    std::string changelog;
    std::string path;
    std::vector<std::string> compatible_versions; // Versions pouvant être mises à jour
};

class FirmwareManager {
public:
    FirmwareManager(const std::string& firmware_directory);

    // Vérifier si une mise à jour est disponible pour un appareil
    std::optional<FirmwareInfo> CheckForUpdate(
        const std::string& device_id,
        const std::string& current_version,
        const std::string& hardware_model);

    // Obtenir le chemin du fichier firmware pour une version donnée
    std::string GetFirmwarePath(const std::string& version);

    // Ajouter un nouveau firmware
    bool AddFirmware(const FirmwareInfo& firmware_info);

    // Supprimer un firmware
    bool RemoveFirmware(const std::string& version);

    // Lister tous les firmwares disponibles
    std::vector<FirmwareInfo> ListAvailableFirmwares();

private:
    std::string firmware_directory_;
    std::map<std::string, FirmwareInfo> available_firmwares_;

    // Charger les informations sur tous les firmwares disponibles
    void LoadAvailableFirmwares();

    // Écrire dans le fichier de configuration des firmwares
    void SaveFirmwareConfig();
};

} // namespace ota

#endif // FIRMWARE_MANAGER_H