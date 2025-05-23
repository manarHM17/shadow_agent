#ifndef PARTITION_MANAGER_H
#define PARTITION_MANAGER_H

#include <string>
#include <vector>

enum class PartitionSlot {
    SLOT_A = 0,
    SLOT_B = 1,
    UNKNOWN = -1
};

struct PartitionInfo {
    PartitionSlot slot;
    std::string device_path;
    std::string mount_point;
    bool is_active;
    bool is_bootable;
    std::string version;
    size_t size_bytes;
};

class PartitionManager {
public:
    PartitionManager();
    ~PartitionManager();
    
    // Initialize partition manager and detect A/B slots
    bool initialize();
    
    // Get current active partition
    PartitionSlot getCurrentActiveSlot();
    
    // Get inactive partition (target for updates)
    PartitionSlot getInactiveSlot();
    
    // Get partition information
    PartitionInfo getPartitionInfo(PartitionSlot slot);
    
    // Prepare inactive partition for update
    bool prepareInactivePartition();
    
    // Write firmware image to inactive partition
    bool writeFirmwareToInactivePartition(const std::string& firmware_path);
    
    // Mark partition as bootable
    bool markPartitionBootable(PartitionSlot slot);
    
    // Switch boot to specified partition
    bool switchBootPartition(PartitionSlot slot);
    
    // Verify partition integrity
    bool verifyPartition(PartitionSlot slot, const std::string& expected_checksum);
    
    // Get partition mount point
    std::string getPartitionMountPoint(PartitionSlot slot);
    
    // Check if partition is mounted
    bool isPartitionMounted(PartitionSlot slot);
    
    // Mount/unmount partition
    bool mountPartition(PartitionSlot slot);
    bool unmountPartition(PartitionSlot slot);
    
    // Rollback to previous partition
    bool rollbackToPreviousPartition();
    
    // Get firmware version from partition
    std::string getFirmwareVersion(PartitionSlot slot);
    
    // Update firmware version info
    bool updateFirmwareVersion(PartitionSlot slot, const std::string& version);

private:
    // Detect available partitions
    bool detectPartitions();
    
    // Read boot configuration
    bool readBootConfig();
    
    // Update boot configuration
    bool updateBootConfig();
    
    // Execute system command
    bool executeCommand(const std::string& command, std::string& output);
    
    // Get device info
    bool getDeviceInfo(const std::string& device, size_t& size);
    
    // Copy partition data
    bool copyPartition(const std::string& source, const std::string& destination);
    
    PartitionInfo slot_a_info_;
    PartitionInfo slot_b_info_;
    PartitionSlot current_active_slot_;
    
    std::string boot_config_path_;
    std::string version_file_path_;
    
    // Raspberry Pi specific paths
    static const std::string BOOT_CONFIG_TXT;
    static const std::string CMDLINE_TXT;
    static const std::string VERSION_FILE;
};

#endif // PARTITION_MANAGER_H