#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include "partition_manager.h"
#include "ota_service.grpc.pb.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

enum class UpdateState {
    IDLE,
    CHECKING_FOR_UPDATES,
    UPDATE_AVAILABLE,
    DOWNLOADING,
    DOWNLOAD_COMPLETED,
    VERIFYING,
    INSTALLING, 
    REBOOT_REQUIRED,
    UPDATE_SUCCESS,
    UPDATE_FAILED,
    ROLLBACK_IN_PROGRESS,
    ROLLBACK_COMPLETED
};

struct UpdateProgress {
    UpdateState state;
    int progress_percentage;
    std::string current_operation;
    std::string error_message;
};

// Callback type for update progress notifications
using UpdateProgressCallback = std::function<void(const UpdateProgress&)>;

class UpdateManager {
public:
    UpdateManager();
    ~UpdateManager();
    
    // Initialize the update manager
    bool initialize(const std::string& client_id, const std::string& temp_dir = "/tmp/ota");
    
    // Set progress callback
    void setProgressCallback(UpdateProgressCallback callback);
    
    // Start update process with firmware info
    bool startUpdate(const ota::FirmwareInfo& firmware_info);
    
    // Cancel ongoing update
    void cancelUpdate();
    
    // Get current update state
    UpdateState getCurrentState() const;
    
    // Get current progress
    UpdateProgress getCurrentProgress() const;
    
    // Check if update is in progress
    bool isUpdateInProgress() const;
    
    // Download firmware chunk
    bool processFirmwareChunk(const std::vector<uint8_t>& chunk_data, 
                             int64_t offset, 
                             int64_t total_size, 
                             bool is_last_chunk);
    
    // Verify downloaded firmware
    bool verifyDownloadedFirmware(const std::string& expected_checksum);
    
    // Install firmware to inactive partition
    bool installFirmware();
    
    // Complete update process (switch partitions and reboot)
    bool completeUpdate();
    
    // Rollback to previous version
    bool rollbackUpdate();
    
    // Cleanup temporary files
    void cleanup();
    
    // Get download progress
    float getDownloadProgress() const;
    
    // Get current firmware version
    std::string getCurrentFirmwareVersion() const;
    
    // Verify system integrity after boot
    bool verifySystemIntegrity();

private:
    // Internal state management
    void setState(UpdateState state, const std::string& operation = "", const std::string& error = "");
    void updateProgress(int percentage);
    void notifyProgress();
    
    // Download management
    bool initializeDownload(const ota::FirmwareInfo& firmware_info);
    bool finalizeDownload();
    
    // Installation steps
    bool prepareInstallation();
    bool performInstallation();
    bool finalizeInstallation();
    
    // Verification methods
    bool verifyFirmwareIntegrity();
    bool verifyInstallation();
    
    // File operations
    bool createTempDirectory();
    bool removeTempDirectory();
    std::string getFirmwarePath() const;
    
    // Thread-safe operations
    mutable std::mutex state_mutex_;
    
    // Core components
    std::unique_ptr<PartitionManager> partition_manager_;
    
    // Configuration
    std::string client_id_;
    std::string temp_directory_;
    
    // Current update info
    ota::FirmwareInfo current_firmware_info_;
    std::string downloaded_firmware_path_;
    
    // State tracking
    std::atomic<UpdateState> current_state_;
    std::atomic<int> progress_percentage_;
    std::string current_operation_;
    std::string error_message_;
    
    // Download tracking
    std::vector<uint8_t> download_buffer_;
    int64_t downloaded_bytes_;
    int64_t total_firmware_size_;
    
    // Callback
    UpdateProgressCallback progress_callback_;
    
    // Constants
    static const size_t MAX_CHUNK_SIZE;
    static const int UPDATE_TIMEOUT_SECONDS;
};

#endif // UPDATE_MANAGER_H