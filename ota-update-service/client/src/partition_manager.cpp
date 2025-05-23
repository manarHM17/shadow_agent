#include "partition_manager.h"
#include "../../common/include/checksum.h"
#include "../../common/include/logging.h"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

const std::string PartitionManager::BOOT_CONFIG_TXT = "/boot/config.txt";
const std::string PartitionManager::CMDLINE_TXT = "/boot/cmdline.txt";
const std::string PartitionManager::VERSION_FILE = "/etc/ota_version";

PartitionManager::PartitionManager() 
    : current_active_slot_(PartitionSlot::UNKNOWN) {
}

PartitionManager::~PartitionManager() {
    // Cleanup if needed
}

bool PartitionManager::initialize() {
    LOG_INFO("Initializing partition manager...");
    
    if (!detectPartitions()) {
        LOG_ERROR("Failed to detect partitions");
        return false;
    }
    
    if (!readBootConfig()) {
        LOG_ERROR("Failed to read boot configuration");
        return false;
    }
    
    // Determine current active slot
    std::ifstream cmdline(CMDLINE_TXT);
    if (cmdline.is_open()) {
        std::string line;
        std::getline(cmdline, line);
        
        if (line.find("root=/dev/mmcblk0p2") != std::string::npos) {
            current_active_slot_ = PartitionSlot::SLOT_A;
        } else if (line.find("root=/dev/mmcblk0p3") != std::string::npos) {
            current_active_slot_ = PartitionSlot::SLOT_B;
        }
        cmdline.close();
    }
    
    if (current_active_slot_ == PartitionSlot::UNKNOWN) {
        LOG_ERROR("Could not determine current active slot");
        return false;
    }
    
    LOG_INFO("Current active slot: " + std::string(current_active_slot_ == PartitionSlot::SLOT_A ? "A" : "B"));
    return true;
}

bool PartitionManager::detectPartitions() {
    // Initialize partition info structures
    slot_a_info_.slot = PartitionSlot::SLOT_A;
    slot_a_info_.device_path = "/dev/mmcblk0p2";
    slot_a_info_.mount_point = "/mnt/slot_a";
    
    slot_b_info_.slot = PartitionSlot::SLOT_B;
    slot_b_info_.device_path = "/dev/mmcblk0p3";
    slot_b_info_.mount_point = "/mnt/slot_b";
    
    // Check if partition devices exist
    struct stat stat_buf;
    if (stat(slot_a_info_.device_path.c_str(), &stat_buf) != 0) {
        LOG_ERROR("Slot A partition not found: " + slot_a_info_.device_path);
        return false;
    }
    
    if (stat(slot_b_info_.device_path.c_str(), &stat_buf) != 0) {
        LOG_ERROR("Slot B partition not found: " + slot_b_info_.device_path);
        return false;
    }
    
    // Get partition sizes
    getDeviceInfo(slot_a_info_.device_path, slot_a_info_.size_bytes);
    getDeviceInfo(slot_b_info_.device_path, slot_b_info_.size_bytes);
    
    // Create mount points if they don't exist
    mkdir(slot_a_info_.mount_point.c_str(), 0755);
    mkdir(slot_b_info_.mount_point.c_str(), 0755);
    
    LOG_INFO("Detected partitions successfully");
    return true;
}

bool PartitionManager::readBootConfig() {
    boot_config_path_ = BOOT_CONFIG_TXT;
    version_file_path_ = VERSION_FILE;
    
    // Read current firmware versions
    slot_a_info_.version = getFirmwareVersion(PartitionSlot::SLOT_A);
    slot_b_info_.version = getFirmwareVersion(PartitionSlot::SLOT_B);
    
    return true;
}

PartitionSlot PartitionManager::getCurrentActiveSlot() {
    return current_active_slot_;
}

PartitionSlot PartitionManager::getInactiveSlot() {
    return (current_active_slot_ == PartitionSlot::SLOT_A) ? 
           PartitionSlot::SLOT_B : PartitionSlot::SLOT_A;
}

PartitionInfo PartitionManager::getPartitionInfo(PartitionSlot slot) {
    if (slot == PartitionSlot::SLOT_A) {
        return slot_a_info_;
    } else if (slot == PartitionSlot::SLOT_B) {
        return slot_b_info_;
    }
    
    PartitionInfo empty_info = {};
    empty_info.slot = PartitionSlot::UNKNOWN;
    return empty_info;
}

bool PartitionManager::prepareInactivePartition() {
    PartitionSlot inactive_slot = getInactiveSlot();
    PartitionInfo& inactive_info = (inactive_slot == PartitionSlot::SLOT_A) ? 
                                   slot_a_info_ : slot_b_info_;
    
    LOG_INFO("Preparing inactive partition: " + inactive_info.device_path);
    
    // Unmount if mounted
    if (isPartitionMounted(inactive_slot)) {
        if (!unmountPartition(inactive_slot)) {
            LOG_ERROR("Failed to unmount inactive partition");
            return false;
        }
    }
    
    // Format the partition (ext4)
    std::string format_cmd = "mkfs.ext4 -F " + inactive_info.device_path;
    std::string output;
    if (!executeCommand(format_cmd, output)) {
        LOG_ERROR("Failed to format inactive partition: " + output);
        return false;
    }
    
    LOG_INFO("Inactive partition prepared successfully");
    return true;
}

bool PartitionManager::writeFirmwareToInactivePartition(const std::string& firmware_path) {
    PartitionSlot inactive_slot = getInactiveSlot();
    PartitionInfo& inactive_info = (inactive_slot == PartitionSlot::SLOT_A) ? 
                                   slot_a_info_ : slot_b_info_;
    
    LOG_INFO("Writing firmware to inactive partition: " + firmware_path);
    
    // Mount inactive partition
    if (!mountPartition(inactive_slot)) {
        LOG_ERROR("Failed to mount inactive partition");
        return false;
    }
    
    // Extract firmware image to partition
    std::string extract_cmd = "tar -xzf " + firmware_path + " -C " + inactive_info.mount_point;
    std::string output;
    if (!executeCommand(extract_cmd, output)) {
        LOG_ERROR("Failed to extract firmware: " + output);
        unmountPartition(inactive_slot);
        return false;
    }
    
    // Sync filesystem
    sync();
    
    // Unmount partition
    if (!unmountPartition(inactive_slot)) {
        LOG_WARNING("Failed to cleanly unmount partition after firmware write");
    }
    
    LOG_INFO("Firmware written successfully to inactive partition");
    return true;
}

bool PartitionManager::switchBootPartition(PartitionSlot slot) {
    LOG_INFO("Switching boot to slot: " + std::string(slot == PartitionSlot::SLOT_A ? "A" : "B"));
    
    // Update cmdline.txt to point to the new root partition
    std::string new_root = (slot == PartitionSlot::SLOT_A) ? 
                          "/dev/mmcblk0p2" : "/dev/mmcblk0p3";
    
    std::ifstream cmdline_in(CMDLINE_TXT);
    std::string cmdline_content;
    if (cmdline_in.is_open()) {
        std::getline(cmdline_in, cmdline_content);
        cmdline_in.close();
    }
    
    // Replace root device
    size_t root_pos = cmdline_content.find("root=");
    if (root_pos != std::string::npos) {
        size_t space_pos = cmdline_content.find(" ", root_pos);
        if (space_pos == std::string::npos) {
            space_pos = cmdline_content.length();
        }
        cmdline_content.replace(root_pos, space_pos - root_pos, "root=" + new_root);
    } else {
        cmdline_content += " root=" + new_root;
    }
    
    // Write updated cmdline.txt
    std::ofstream cmdline_out(CMDLINE_TXT);
    if (!cmdline_out.is_open()) {
        LOG_ERROR("Failed to open cmdline.txt for writing");
        return false;
    }
    
    cmdline_out << cmdline_content;
    cmdline_out.close();
    
    sync();
    
    LOG_INFO("Boot partition switched successfully");
    return true;
}

bool PartitionManager::verifyPartition(PartitionSlot slot, const std::string& expected_checksum) {
    PartitionInfo info = getPartitionInfo(slot);
    if (info.slot == PartitionSlot::UNKNOWN) {
        return false;
    }
    
    LOG_INFO("Verifying partition checksum...");
    
    // For simplicity, we'll verify a specific file in the partition
    // In a real implementation, you might want to verify the entire partition or key files
    if (!mountPartition(slot)) {
        LOG_ERROR("Failed to mount partition for verification");
        return false;
    }
    
    std::string version_file = info.mount_point + "/etc/ota_version";
    bool result = ChecksumCalculator::verifyFile(version_file, expected_checksum);
    
    unmountPartition(slot);
    
    return result;
}

bool PartitionManager::mountPartition(PartitionSlot slot) {
    PartitionInfo info = getPartitionInfo(slot);
    if (info.slot == PartitionSlot::UNKNOWN) {
        return false;
    }
    
    if (isPartitionMounted(slot)) {
        return true; // Already mounted
    }
    
    if (mount(info.device_path.c_str(), info.mount_point.c_str(), "ext4", 0, nullptr) != 0) {
        LOG_ERROR("Failed to mount partition: " + info.device_path + " to " + info.mount_point);
        return false;
    }
    
    LOG_DEBUG("Mounted partition: " + info.device_path + " to " + info.mount_point);
    return true;
}

bool PartitionManager::unmountPartition(PartitionSlot slot) {
    PartitionInfo info = getPartitionInfo(slot);
    if (info.slot == PartitionSlot::UNKNOWN) {
        return false;
    }
    
    if (umount(info.mount_point.c_str()) != 0) {
        LOG_ERROR("Failed to unmount partition: " + info.mount_point);
        return false;
    }
    
    LOG_DEBUG("Unmounted partition: " + info.mount_point);
    return true;
}

bool PartitionManager::isPartitionMounted(PartitionSlot slot) {
    PartitionInfo info = getPartitionInfo(slot);
    if (info.slot == PartitionSlot::UNKNOWN) {
        return false;
    }
    
    std::ifstream mounts("/proc/mounts");
    std::string line;
    
    while (std::getline(mounts, line)) {
        if (line.find(info.device_path) != std::string::npos && 
            line.find(info.mount_point) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool PartitionManager::rollbackToPreviousPartition() {
    LOG_INFO("Initiating rollback to previous partition");
    
    PartitionSlot previous_slot = getInactiveSlot();
    return switchBootPartition(previous_slot);
}

std::string PartitionManager::getFirmwareVersion(PartitionSlot slot) {
    PartitionInfo info = getPartitionInfo(slot);
    if (info.slot == PartitionSlot::UNKNOWN) {
        return "";
    }
    
    bool was_mounted = isPartitionMounted(slot);
    if (!was_mounted && !mountPartition(slot)) {
        LOG_ERROR("Failed to mount partition to read firmware version");
        return "";
    }
    
    std::string version_file = info.mount_point + "/etc/ota_version";
    std::ifstream file(version_file);
    std::string version;
    
    if (file.is_open()) {
        std::getline(file, version);
        file.close();
    } else {
        version = "unknown";
    }
    
    if (!was_mounted) {
        unmountPartition(slot);
    }
    
    return version;
}

bool PartitionManager::updateFirmwareVersion(PartitionSlot slot, const std::string& version) {
    PartitionInfo info = getPartitionInfo(slot);
    if (info.slot == PartitionSlot::UNKNOWN) {
        return false;
    }
    
    bool was_mounted = isPartitionMounted(slot);
    if (!was_mounted && !mountPartition(slot)) {
        LOG_ERROR("Failed to mount partition to update firmware version");
        return false;
    }
    
    std::string version_file = info.mount_point + "/etc/ota_version";
    std::ofstream file(version_file);
    
    if (!file.is_open()) {
        LOG_ERROR("Failed to open version file for writing: " + version_file);
        if (!was_mounted) {
            unmountPartition(slot);
        }
        return false;
    }
    
    file << version << std::endl;
    file.close();
    sync();
    
    if (!was_mounted) {
        unmountPartition(slot);
    }
    
    // Update local info
    if (slot == PartitionSlot::SLOT_A) {
        slot_a_info_.version = version;
    } else {
        slot_b_info_.version = version;
    }
    
    LOG_INFO("Updated firmware version for slot " + 
             std::string(slot == PartitionSlot::SLOT_A ? "A" : "B") + 
             " to: " + version);
    
    return true;
}

std::string PartitionManager::getPartitionMountPoint(PartitionSlot slot) {
    PartitionInfo info = getPartitionInfo(slot);
    return info.mount_point;
}

bool PartitionManager::executeCommand(const std::string& command, std::string& output) {
    LOG_DEBUG("Executing command: " + command);
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        LOG_ERROR("Failed to execute command: " + command);
        return false;
    }
    
    char buffer[128];
    output.clear();
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int exit_code = pclose(pipe);
    
    if (exit_code != 0) {
        LOG_ERROR("Command failed with exit code " + std::to_string(exit_code) + ": " + command);
        return false;
    }
    
    return true;
}

bool PartitionManager::getDeviceInfo(const std::string& device, size_t& size) {
    std::string command = "blockdev --getsize64 " + device;
    std::string output;
    
    if (!executeCommand(command, output)) {
        return false;
    }
    
    try {
        size = std::stoull(output);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse device size: " + output);
        return false;
    }
}

bool PartitionManager::updateBootConfig() {
    // This method can be used to update additional boot configuration
    // For now, the main boot switching is handled in switchBootPartition()
    return true;
}

bool PartitionManager::markPartitionBootable(PartitionSlot slot) {
    // On Raspberry Pi, bootability is determined by cmdline.txt
    // This is handled in switchBootPartition()
    return switchBootPartition(slot);
}