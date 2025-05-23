#include "update_manager.h"
#include "../../common/include/logging.h"
#include "../../common/include/checksum.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <unistd.h>
#include <sys/reboot.h>

const size_t UpdateManager::MAX_CHUNK_SIZE = 1024 * 1024; // 1MB
const int UpdateManager::UPDATE_TIMEOUT_SECONDS = 1800; // 30 minutes

UpdateManager::UpdateManager() 
    : partition_manager_(std::make_unique<PartitionManager>()),
      current_state_(UpdateState::IDLE),
      progress_percentage_(0),
      downloaded_bytes_(0),
      total_firmware_size_(0) {
}

UpdateManager::~UpdateManager() {
    cleanup();
}

bool UpdateManager::initialize(const std::string& client_id, const std::string& temp_dir) {
    LOG_INFO("Initializing UpdateManager for client: " + client_id);
    
    client_id_ = client_id;
    temp_directory_ = temp_dir;
    
    // Initialize partition manager
    if (!partition_manager_->initialize()) {
        LOG_ERROR("Failed to initialize partition manager");
        return false;
    }
    
    // Create temporary directory
    if (!createTempDirectory()) {
        LOG_ERROR("Failed to create temporary directory");
        return false;
    }
    
    setState(UpdateState::IDLE, "Update manager initialized");
    LOG_INFO("UpdateManager initialized successfully");
    return true;
}

void UpdateManager::setProgressCallback(UpdateProgressCallback callback) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    progress_callback_ = callback;
}

bool UpdateManager::startUpdate(const ota::FirmwareInfo& firmware_info) {
    LOG_INFO("Starting update process for firmware version: " + firmware_info.version());
    
    if (isUpdateInProgress()) {
        LOG_WARNING("Update already in progress");
        return false;
    }
    
    current_firmware_info_ = firmware_info;
    total_firmware_size_ = firmware_info.file_size();
    
    if (!initializeDownload(firmware_info)) {
        setState(UpdateState::UPDATE_FAILED, "Failed to initialize download", "Download initialization failed");
        return false;
    }
    
    setState(UpdateState::DOWNLOADING, "Starting firmware download");
    return true;
}

void UpdateManager::cancelUpdate() {
    LOG_INFO("Cancelling update process");
    
    UpdateState current = getCurrentState();
    if (current == UpdateState::IDLE || 
        current == UpdateState::UPDATE_SUCCESS || 
        current == UpdateState::UPDATE_FAILED) {
        return;
    }
    
    setState(UpdateState::UPDATE_FAILED, "Update cancelled by user", "Update was cancelled");
    cleanup();
}

UpdateState UpdateManager::getCurrentState() const {
    return current_state_.load();
}

UpdateProgress UpdateManager::getCurrentProgress() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    UpdateProgress progress;
    progress.state = current_state_.load();
    progress.progress_percentage = progress_percentage_.load();
    progress.current_operation = current_operation_;
    progress.error_message = error_message_;
    
    return progress;
}

bool UpdateManager::isUpdateInProgress() const {
    UpdateState state = getCurrentState();
    return (state != UpdateState::IDLE && 
            state != UpdateState::UPDATE_SUCCESS && 
            state != UpdateState::UPDATE_FAILED &&
            state != UpdateState::ROLLBACK_COMPLETED);
}

bool UpdateManager::processFirmwareChunk(const std::vector<uint8_t>& chunk_data, 
                                        int64_t offset, 
                                        int64_t total_size, 
                                        bool is_last_chunk) {
    
    if (getCurrentState() != UpdateState::DOWNLOADING) {
        LOG_ERROR("Not in downloading state");
        return false;
    }
    
    // Validate chunk
    if (offset != downloaded_bytes_) {
        LOG_ERROR("Chunk offset mismatch. Expected: " + std::to_string(downloaded_bytes_) + 
                 ", Got: " + std::to_string(offset));
        return false;
    }
    
    // Append chunk to download buffer
    download_buffer_.insert(download_buffer_.end(), chunk_data.begin(), chunk_data.end());
    downloaded_bytes_ += chunk_data.size();
    
    // Update progress
    int progress = (downloaded_bytes_ * 100) / total_firmware_size_;
    updateProgress(progress);
    
    LOG_DEBUG("Received chunk: " + std::to_string(chunk_data.size()) + " bytes. " +
              "Total: " + std::to_string(downloaded_bytes_) + "/" + std::to_string(total_firmware_size_));
    
    // If this is the last chunk, finalize download
    if (is_last_chunk) {
        return finalizeDownload();
    }
    
    return true;
}

bool UpdateManager::verifyDownloadedFirmware(const std::string& expected_checksum) {
    setState(UpdateState::VERIFYING, "Verifying firmware integrity");
    
    if (download_buffer_.empty()) {
        setState(UpdateState::UPDATE_FAILED, "Verification failed", "No firmware data to verify");
        return false;
    }
    
    LOG_INFO("Verifying firmware checksum...");
    
    bool checksum_valid = ChecksumCalculator::verifyData(download_buffer_, expected_checksum);
    
    if (!checksum_valid) {
        setState(UpdateState::UPDATE_FAILED, "Verification failed", "Firmware checksum mismatch");
        return false;
    }
    
    LOG_INFO("Firmware verification successful");
    setState(UpdateState::DOWNLOAD_COMPLETED, "Firmware verified successfully");
    return true;
}

bool UpdateManager::installFirmware() {
    LOG_INFO("Installing firmware to inactive partition");
    setState(UpdateState::INSTALLING, "Installing firmware");
    
    if (!prepareInstallation()) {
        setState(UpdateState::UPDATE_FAILED, "Installation failed", "Failed to prepare installation");
        return false;
    }
    
    if (!performInstallation()) {
        setState(UpdateState::UPDATE_FAILED, "Installation failed", "Failed to perform installation");
        return false;
    }
    
    if (!finalizeInstallation()) {
        setState(UpdateState::UPDATE_FAILED, "Installation failed", "Failed to finalize installation");
        return false;
    }
    
    setState(UpdateState::REBOOT_REQUIRED, "Installation completed, reboot required");
    LOG_INFO("Firmware installation completed successfully");
    return true;
}

bool UpdateManager::completeUpdate() {
    LOG_INFO("Completing update process");
    
    if (getCurrentState() != UpdateState::REBOOT_REQUIRED) {
        LOG_ERROR("Cannot complete update - not in reboot required state");
        return false;
    }
    
    // Switch boot partition
    PartitionSlot inactive_slot = partition_manager_->getInactiveSlot();
    if (!partition_manager_->switchBootPartition(inactive_slot)) {
        setState(UpdateState::UPDATE_FAILED, "Update failed", "Failed to switch boot partition");
        return false;
    }
    
    // Update firmware version in new partition
    if (!partition_manager_->updateFirmwareVersion(inactive_slot, current_firmware_info_.version())) {
        LOG_WARNING("Failed to update firmware version file");
    }
    
    setState(UpdateState::UPDATE_SUCCESS, "Update completed successfully");
    
    // Schedule reboot
    LOG_INFO("Rebooting system in 5 seconds...");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Perform reboot
    sync();
    reboot(RB_AUTOBOOT);
    
    return true;
}

bool UpdateManager::rollbackUpdate() {
    LOG_INFO("Starting rollback process");
    setState(UpdateState::ROLLBACK_IN_PROGRESS, "Rolling back to previous version");
    
    if (!partition_manager_->rollbackToPreviousPartition()) {
        setState(UpdateState::UPDATE_FAILED, "Rollback failed", "Failed to switch to previous partition");
        return false;
    }
    
    setState(UpdateState::ROLLBACK_COMPLETED, "Rollback completed successfully");
    
    LOG_INFO("Rollback completed, rebooting system...");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    sync();
    reboot(RB_AUTOBOOT);
    
    return true;
}

bool UpdateManager::verifySystemIntegrity() {
    LOG_INFO("Verifying system integrity after boot");
    
    // Check if we're running on the expected partition
    PartitionSlot current_slot = partition_manager_->getCurrentActiveSlot();
    std::string current_version = partition_manager_->getFirmwareVersion(current_slot);
    
    LOG_INFO("Current active slot: " + std::string(current_slot == PartitionSlot::SLOT_A ? "A" : "B"));
    LOG_INFO("Current firmware version: " + current_version);
    
    // Verify system files and services
    // This is a simplified check - in practice, you'd want more comprehensive verification
    
    // Check if essential system files exist
    std::vector<std::string> essential_files = {
        "/bin/sh",
        "/sbin/init", 
        "/etc/passwd",
        "/etc/fstab"
    };
    
    for (const auto& file : essential_files) {
        if (access(file.c_str(), F_OK) != 0) {
            LOG_ERROR("Essential system file missing: " + file);
            return false;
        }
    }
    
    LOG_INFO("System integrity verification passed");
    return true;
}

void UpdateManager::cleanup() {
    LOG_INFO("Cleaning up temporary files");
    
    download_buffer_.clear();
    downloaded_bytes_ = 0;
    
    if (!downloaded_firmware_path_.empty()) {
        std::filesystem::remove(downloaded_firmware_path_);
        downloaded_firmware_path_.clear();
    }
    
    removeTempDirectory();
}

float UpdateManager::getDownloadProgress() const {
    if (total_firmware_size_ == 0) {
        return 0.0f;
    }
    
    return (static_cast<float>(downloaded_bytes_) / static_cast<float>(total_firmware_size_)) * 100.0f;
}

std::string UpdateManager::getCurrentFirmwareVersion() const {
    PartitionSlot current_slot = partition_manager_->getCurrentActiveSlot();
    return partition_manager_->getFirmwareVersion(current_slot);
}

// Private methods implementation

void UpdateManager::setState(UpdateState state, const std::string& operation, const std::string& error) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    current_state_.store(state);
    current_operation_ = operation;
    error_message_ = error;
    
    LOG_INFO("State changed to: " + std::to_string(static_cast<int>(state)) + " - " + operation);
    
    if (!error.empty()) {
        LOG_ERROR("Error: " + error);
    }
    
    notifyProgress();
}

void UpdateManager::updateProgress(int percentage) {
    progress_percentage_.store(percentage);
    notifyProgress();
}

void UpdateManager::notifyProgress() {
    if (progress_callback_) {
        UpdateProgress progress = getCurrentProgress();
        progress_callback_(progress);
    }
}

bool UpdateManager::initializeDownload(const ota::FirmwareInfo& firmware_info) {
    LOG_INFO("Initializing download for firmware: " + firmware_info.filename());
    
    download_buffer_.clear();
    download_buffer_.reserve(firmware_info.file_size());
    
    downloaded_bytes_ = 0;
    total_firmware_size_ = firmware_info.file_size();
    
    downloaded_firmware_path_ = temp_directory_ + "/" + firmware_info.filename();
    
    LOG_INFO("Download initialized. Expected size: " + std::to_string(total_firmware_size_) + " bytes");
    return true;
}

bool UpdateManager::finalizeDownload() {
    LOG_INFO("Finalizing firmware download");
    
    // Write buffer to file
    std::ofstream file(downloaded_firmware_path_, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to create firmware file: " + downloaded_firmware_path_);
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(download_buffer_.data()), download_buffer_.size());
    file.close();
    
    if (downloaded_bytes_ != total_firmware_size_) {
        LOG_ERROR("Download size mismatch. Expected: " + std::to_string(total_firmware_size_) + 
                 ", Got: " + std::to_string(downloaded_bytes_));
        return false;
    }
    
    LOG_INFO("Firmware download completed: " + downloaded_firmware_path_);
    return verifyDownloadedFirmware(current_firmware_info_.checksum());
}

bool UpdateManager::prepareInstallation() {
    LOG_INFO("Preparing installation");
    
    updateProgress(10);
    
    // Prepare inactive partition
    if (!partition_manager_->prepareInactivePartition()) {
        LOG_ERROR("Failed to prepare inactive partition");
        return false;
    }
    
    updateProgress(30);
    return true;
}

bool UpdateManager::performInstallation() {
    LOG_INFO("Performing firmware installation");
    
    updateProgress(50);
    
    // Write firmware to inactive partition
    if (!partition_manager_->writeFirmwareToInactivePartition(downloaded_firmware_path_)) {
        LOG_ERROR("Failed to write firmware to inactive partition");
        return false;
    }
    
    updateProgress(80);
    return true;
}

bool UpdateManager::finalizeInstallation() {
    LOG_INFO("Finalizing installation");
    
    updateProgress(90);
    
    // Verify installation
    if (!verifyInstallation()) {
        LOG_ERROR("Installation verification failed");
        return false;
    }
    
    updateProgress(100);
    LOG_INFO("Installation finalized successfully");
    return true;
}

bool UpdateManager::verifyInstallation() {
    LOG_INFO("Verifying installation");
    
    PartitionSlot inactive_slot = partition_manager_->getInactiveSlot();
    
    // Basic verification - check if partition can be mounted
    if (!partition_manager_->mountPartition(inactive_slot)) {
        LOG_ERROR("Cannot mount inactive partition for verification");
        return false;
    }
    
    // Check if essential directories exist
    std::string mount_point = partition_manager_->getPartitionMountPoint(inactive_slot);
    std::vector<std::string> essential_dirs = {
        mount_point + "/bin",
        mount_point + "/etc", 
        mount_point + "/usr",
        mount_point + "/var"
    };
    
    for (const auto& dir : essential_dirs) {
        if (!std::filesystem::exists(dir)) {
            LOG_ERROR("Essential directory missing in installation: " + dir);
            partition_manager_->unmountPartition(inactive_slot);
            return false;
        }
    }
    
    partition_manager_->unmountPartition(inactive_slot);
    
    LOG_INFO("Installation verification completed successfully");
    return true;
}

bool UpdateManager::createTempDirectory() {
    try {
        std::filesystem::create_directories(temp_directory_);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create temp directory: " + std::string(e.what()));
        return false;
    }
}

bool UpdateManager::removeTempDirectory() {
    try {
        std::filesystem::remove_all(temp_directory_);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to remove temp directory: " + std::string(e.what()));
        return false;
    }
}

std::string UpdateManager::getFirmwarePath() const {
    return downloaded_firmware_path_;
}