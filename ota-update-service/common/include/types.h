#ifndef OTA_COMMON_TYPES_H
#define OTA_COMMON_TYPES_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <stdexcept>

namespace ota_common {

// Device status enumeration
enum class DeviceStatus {
    ONLINE,
    OFFLINE,
    UPDATING,
    ERROR
};

// Update type enumeration for different types of OTA updates
enum class UpdateType {
    CONFIG_FILE,
    APPLICATION,
    SYSTEMD_SERVICE
    // FIRMWARE,          // Reserved for future extensions
    // KERNEL_MODULE,     // Reserved for Raspberry Pi-specific updates
};

// Update status enumeration for tracking installation progress
enum class UpdateStatus {
    PENDING,
    DOWNLOADING,
    DOWNLOADED,
    INSTALLING,
    SUCCESS,
    FAILED,
    ROLLBACK
};

// Structure for update metadata
struct UpdateInfo {
    int32_t update_id;          // Unique update identifier
    UpdateType update_type;     // Type of update (config, application, etc.)
    std::string version;        // Version string (e.g., "1.2.3")
    std::string file_path;      // Path to the update file
    std::string checksum;       // SHA256 checksum of the update file
    size_t file_size;           // Size of the update file in bytes
    std::string created_at;     // Timestamp of update creation (ISO 8601)
    std::string description;    // Description of the update
    bool requires_reboot;       // Whether the update requires a system reboot
};

// Structure for recording installation attempts
struct InstallationRecord {
    int id;                     // Unique installation record ID
    int32_t device_id;          // Device identifier
    int update_id;              // Update identifier
    bool success;               // Installation success status
    DeviceStatus status;        // Device status during installation
    std::string error_message;  // Error message if installation failed
    int progress_percentage;    // Installation progress (0-100)
    int64_t started_at;         // Timestamp when installation started
    int64_t completed_at;       // Timestamp when installation completed
};

// Structure for device information
struct DeviceInfo {
    int32_t device_id;          // Unique device identifier
    std::string device_type;    // Device type (e.g., "RPi4")
    std::string hostname;       // Device hostname
    std::string current_version;// Current software version
    std::string platform;       // Platform (e.g., "Linux")
    DeviceStatus status;        // Current device status
    std::string last_seen;      // Last contact timestamp (ISO 8601)
    std::string hardware_model; // Hardware model (e.g., "Raspberry Pi 4B")
    std::string os_version;     // Operating system version
    int64_t last_update;        // Timestamp of last update
};

// Convert UpdateType to string (uppercase)
inline std::string updateTypeToString(UpdateType type) {
    switch (type) {
        case UpdateType::CONFIG_FILE: return "CONFIG_FILE";
        case UpdateType::APPLICATION: return "APPLICATION";
        case UpdateType::SYSTEMD_SERVICE: return "SYSTEMD_SERVICE";
        default: throw std::invalid_argument("Unknown UpdateType");
    }
}

// Convert string to UpdateType (case-insensitive)
inline UpdateType stringToUpdateType(const std::string& str) {
    std::string upper_str = str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
    
    if (upper_str == "CONFIG_FILE") return UpdateType::CONFIG_FILE;
    if (upper_str == "APPLICATION") return UpdateType::APPLICATION;
    if (upper_str == "SYSTEMD_SERVICE") return UpdateType::SYSTEMD_SERVICE;
    throw std::invalid_argument("Invalid UpdateType string: " + str);
}

// Convert UpdateStatus to string (uppercase)
inline std::string updateStatusToString(UpdateStatus status) {
    switch (status) {
        case UpdateStatus::PENDING: return "PENDING";
        case UpdateStatus::DOWNLOADING: return "DOWNLOADING";
        case UpdateStatus::DOWNLOADED: return "DOWNLOADED";
        case UpdateStatus::INSTALLING: return "INSTALLING";
        case UpdateStatus::SUCCESS: return "SUCCESS";
        case UpdateStatus::FAILED: return "FAILED";
        case UpdateStatus::ROLLBACK: return "ROLLBACK";
        default: throw std::invalid_argument("Unknown UpdateStatus");
    }
}

// Convert string to UpdateStatus (case-insensitive)
inline UpdateStatus stringToUpdateStatus(const std::string& str) {
    std::string upper_str = str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
    
    if (upper_str == "PENDING") return UpdateStatus::PENDING;
    if (upper_str == "DOWNLOADING") return UpdateStatus::DOWNLOADING;
    if (upper_str == "DOWNLOADED") return UpdateStatus::DOWNLOADED;
    if (upper_str == "INSTALLING") return UpdateStatus::INSTALLING;
    if (upper_str == "SUCCESS") return UpdateStatus::SUCCESS;
    if (upper_str == "FAILED") return UpdateStatus::FAILED;
    if (upper_str == "ROLLBACK") return UpdateStatus::ROLLBACK;
    throw std::invalid_argument("Invalid UpdateStatus string: " + str);
}

} // namespace ota_common

#endif // OTA_COMMON_TYPES_H