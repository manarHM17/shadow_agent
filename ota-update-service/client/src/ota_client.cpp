#include "ota_client.h"
#include "../../common/include/logging.h"
#include <chrono>
#include <fstream>
#include <sys/utsname.h>

OTAClient::OTAClient(const ClientConfig& config) 
    : config_(config),
      running_(false),
      update_in_progress_(false),
      current_state_(UpdateState::IDLE),
      retry_count_(0) {
    
    // Initialize managers
    update_manager_ = std::make_unique<UpdateManager>();
    partition_manager_ = std::make_unique<PartitionManager>();
}

OTAClient::~OTAClient() {
    stop();
}

bool OTAClient::initialize() {
    LOG_INFO("Initializing OTA Client with ID: " + config_.client_id);
    
    // Initialize partition manager
    if (!partition_manager_->initialize()) {
        LOG_ERROR("Failed to initialize partition manager");
        return false;
    }
    
    // Initialize update manager
    if (!update_manager_->initialize(config_.client_id, config_.temp_directory)) {
        LOG_ERROR("Failed to initialize update manager");
        return false;
    }
    
    // Set up update progress callback
    update_manager_->setProgressCallback(
        [this](const UpdateProgress& progress) {
            handleUpdateProgress(progress);
        }
    );
    
    // Get current firmware version
    current_version_ = update_manager_->getCurrentFirmwareVersion();
    if (current_version_.empty()) {
        current_version_ = "unknown";
    }
    
    LOG_INFO("Current firmware version: " + current_version_);
    
    // Connect to gRPC server
    if (!connectToServer()) {
        LOG_ERROR("Failed to connect to OTA server");
        return false;
    }
    
    setState(UpdateState::IDLE, "OTA Client initialized");
    LOG_INFO("OTA Client initialized successfully");
    return true;
}

void OTAClient::start() {
    if (running_.load()) {
        LOG_WARNING("OTA Client already running");
        return;
    }
    
    LOG_INFO("Starting OTA Client background services");
    running_.store(true);
    
    // Start update checking thread
    check_updates_thread_ = std::thread([this]() {
        checkUpdatesLoop();
    });
    
    // Start status reporting thread
    status_report_thread_ = std::thread([this]() {
        statusReportLoop();
    });
    
    LOG_INFO("OTA Client started successfully");
}

void OTAClient::stop() {
    if (!running_.load()) {
        return;
    }
    
    LOG_INFO("Stopping OTA Client");
    running_.store(false);
    
    // Join threads
    if (check_updates_thread_.joinable()) {
        check_updates_thread_.join();
    }
    
    if (status_report_thread_.joinable()) {
        status_report_thread_.join();
    }
    
    LOG_INFO("OTA Client stopped");
}

void OTAClient::setStatusCallback(StatusCallback callback) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    status_callback_ = callback;
}

UpdateProgress OTAClient::getUpdateProgress() const {
    if (update_manager_) {
        return update_manager_->getCurrentProgress();
    }
    
    UpdateProgress progress;
    progress.state = current_state_.load();
    progress.progress_percentage = 0;
    return progress;
}

bool OTAClient::checkForUpdatesNow() {
    LOG_INFO("Manual update check requested");
    
    if (update_in_progress_.load()) {
        LOG_WARNING("Update already in progress");
        return false;
    }
    
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(30));
    
    auto stream = stub_->CheckForUpdates(&context);
    
    // Send check request
    ota::UpdateCheckRequest request;
    request.set_client_id(config_.client_id);
    request.set_current_version(current_version_);
    request.set_hardware_model(getHardwareModel());
    request.set_timestamp(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    if (!stream->Write(request)) {
        LOG_ERROR("Failed to send update check request");
        return false;
    }
    
    // Read response
    ota::UpdateCheckResponse response;
    if (stream->Read(&response)) {
        LOG_INFO("Received update check response. Available: " + 
                std::string(response.update_available() ? "Yes" : "No"));
        
        if (response.update_available()) {
            resetRetryCount();
            return processAvailableUpdate(response.firmware_info());
        }
    }
    
    stream->WritesDone();
    grpc::Status status = stream->Finish();
    
    if (!status.ok()) {
        LOG_ERROR("Update check failed: " + status.error_message());
        return false;
    }
    
    return true;
}

bool OTAClient::cancelCurrentUpdate() {
    LOG_INFO("Cancelling current update");
    
    if (!update_in_progress_.load()) {
        LOG_WARNING("No update in progress to cancel");
        return false;
    }
    
    if (update_manager_) {
        update_manager_->cancelUpdate();
    }
    
    update_in_progress_.store(false);
    setState(UpdateState::UPDATE_FAILED, "Update cancelled by user");
    
    return true;
}

bool OTAClient::performRollback() {
    LOG_INFO("Performing system rollback");
    
    if (update_in_progress_.load()) {
        LOG_ERROR("Cannot rollback while update is in progress");
        return false;
    }
    
    setState(UpdateState::ROLLBACK_IN_PROGRESS, "Starting rollback");
    
    if (update_manager_ && update_manager_->rollbackUpdate()) {
        setState(UpdateState::ROLLBACK_COMPLETED, "Rollback completed");
        return true;
    }
    
    setState(UpdateState::UPDATE_FAILED, "Rollback failed");
    return false;
}

// Private methods

bool OTAClient::connectToServer() {
    LOG_INFO("Connecting to OTA server: " + config_.server_address);
    
    channel_ = grpc::CreateChannel(config_.server_address, grpc::InsecureChannelCredentials());
    stub_ = ota::OTAService::NewStub(channel_);
    
    // Test connection
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
    
    auto stream = stub_->CheckForUpdates(&context);
    if (!stream) {
        LOG_ERROR("Failed to create gRPC stream");
        return false;
    }
    
    stream->WritesDone();
    grpc::Status status = stream->Finish();
    
    // Connection test successful (even if no updates available)
    LOG_INFO("Connected to OTA server successfully");
    return true;
}

void OTAClient::checkUpdatesLoop() {
    LOG_INFO("Starting update check loop (interval: " + std::to_string(config_.check_interval_seconds) + "s)");
    
    while (running_.load()) {
        try {
            if (!update_in_progress_.load()) {
                checkForUpdatesNow();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in update check loop: " + std::string(e.what()));
            
            if (shouldRetry()) {
                incrementRetryCount();
                waitForRetry();
            } else {
                LOG_ERROR("Max retry attempts reached, will retry on next interval");
                resetRetryCount();
            }
        }
        
        // Wait for next check interval
        std::this_thread::sleep_for(std::chrono::seconds(config_.check_interval_seconds));
    }
    
    LOG_INFO("Update check loop terminated");
}

void OTAClient::statusReportLoop() {
    LOG_INFO("Starting status report loop (interval: " + std::to_string(config_.status_report_interval_seconds) + "s)");
    
    while (running_.load()) {
        try {
            UpdateState state = getCurrentState();
            UpdateProgress progress = getUpdateProgress();
            
            if (!reportUpdateStatus(state, progress.current_operation, progress.progress_percentage)) {
                LOG_WARNING("Failed to report status to server");
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in status report loop: " + std::string(e.what()));
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(config_.status_report_interval_seconds));
    }
    
    LOG_INFO("Status report loop terminated");
}

bool OTAClient::processAvailableUpdate(const ota::FirmwareInfo& firmware_info) {
    LOG_INFO("Processing available update: " + firmware_info.version());
    
    if (update_in_progress_.load()) {
        LOG_WARNING("Update already in progress");
        return false;
    }
    
    // Check if this is actually a newer version
    if (firmware_info.version() == current_version_) {
        LOG_INFO("Firmware version is the same as current, skipping update");
        return true;
    }
    
    update_in_progress_.store(true);
    setState(UpdateState::DOWNLOADING, "Starting firmware download");
    
    // Start the update process
    if (!update_manager_->startUpdate(firmware_info)) {
        LOG_ERROR("Failed to start update process");
        update_in_progress_.store(false);
        setState(UpdateState::UPDATE_FAILED, "Failed to start update");
        return false;
    }
    
    // Download firmware
    if (!downloadFirmware(firmware_info)) {
        LOG_ERROR("Failed to download firmware");
        update_manager_->cancelUpdate();
        update_in_progress_.store(false);
        setState(UpdateState::UPDATE_FAILED, "Download failed");
        return false;
    }
    
    // Install firmware
    setState(UpdateState::INSTALLING, "Installing firmware");
    if (!update_manager_->installFirmware()) {
        LOG_ERROR("Failed to install firmware");
        update_in_progress_.store(false);
        setState(UpdateState::UPDATE_FAILED, "Installation failed");
        return false;
    }
    
    // Complete update (this will reboot the system)
    setState(UpdateState::REBOOT_REQUIRED, "Reboot required");
    if (!update_manager_->completeUpdate()) {
        LOG_ERROR("Failed to complete update");
        update_in_progress_.store(false);
        setState(UpdateState::UPDATE_FAILED, "Update completion failed");
        return false;
    }
    
    return true;
}

bool OTAClient::downloadFirmware(const ota::FirmwareInfo& firmware_info) {
    LOG_INFO("Downloading firmware: " + firmware_info.filename());
    
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::minutes(30));
    
    ota::DownloadRequest request;
    request.set_client_id(config_.client_id);
    request.set_firmware_version(firmware_info.version());
    request.set_offset(0); // Start from beginning
    
    auto stream = stub_->DownloadUpdate(&context, request);
    
    ota::DownloadResponse response;
    while (stream->Read(&response)) {
        // Convert bytes to vector
        std::vector<uint8_t> chunk_data(response.data().begin(), response.data().end());
        
        // Process chunk through update manager
        if (!update_manager_->processFirmwareChunk(
                chunk_data, 
                response.offset(), 
                response.total_size(), 
                response.is_last_chunk())) {
            LOG_ERROR("Failed to process firmware chunk");
            return false;
        }
        
        // Report progress
        int progress = (response.offset() + chunk_data.size()) * 100 / response.total_size();
        reportUpdateStatus(UpdateState::DOWNLOADING, "Downloading firmware", progress);
        
        if (response.is_last_chunk()) {
            LOG_INFO("Download completed successfully");
            break;
        }
    }
    
    grpc::Status status = stream->Finish();
    if (!status.ok()) {
        LOG_ERROR("Download failed: " + status.error_message());
        return false;
    }
    
    return true;
}

bool OTAClient::reportUpdateStatus(UpdateState state, const std::string& message, int progress) {
    try {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
        
        auto stream = stub_->ReportUpdateStatus(&context);
        
        ota::UpdateStatusRequest request;
        request.set_client_id(config_.client_id);
        request.set_firmware_version(current_version_);
        
        // Convert UpdateState to proto enum
        ota::UpdateStatus proto_status;
        switch (state) {
            case UpdateState::IDLE: proto_status = ota::UpdateStatus::IDLE; break;
            case UpdateState::DOWNLOADING: proto_status = ota::UpdateStatus::DOWNLOADING; break;
            case UpdateState::DOWNLOAD_COMPLETED: proto_status = ota::UpdateStatus::DOWNLOAD_COMPLETED; break;
            case UpdateState::VERIFYING: proto_status = ota::UpdateStatus::VERIFYING; break;
            case UpdateState::INSTALLING: proto_status = ota::UpdateStatus::INSTALLING; break;
            case UpdateState::REBOOT_REQUIRED: proto_status = ota::UpdateStatus::REBOOT_REQUIRED; break;
            case UpdateState::UPDATE_SUCCESS: proto_status = ota::UpdateStatus::UPDATE_SUCCESS; break;
            case UpdateState::UPDATE_FAILED: proto_status = ota::UpdateStatus::UPDATE_FAILED; break;
            case UpdateState::ROLLBACK_COMPLETED: proto_status = ota::UpdateStatus::ROLLBACK_COMPLETED; break;
            default: proto_status = ota::UpdateStatus::IDLE; break;
        }
        
        request.set_status(proto_status);
        request.set_error_message(last_error_message_);
        request.set_progress_percentage(progress);
        request.set_timestamp(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        if (!stream->Write(request)) {
            return false;
        }
        
        ota::UpdateStatusResponse response;
        if (stream->Read(&response)) {
            LOG_DEBUG("Status report acknowledged by server");
        }
        
        stream->WritesDone();
        grpc::Status status = stream->Finish();
        
        return status.ok();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in status report: " + std::string(e.what()));
        return false;
    }
}

void OTAClient::setState(UpdateState state, const std::string& message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    current_state_.store(state);
    
    if (state == UpdateState::UPDATE_FAILED || state == UpdateState::ROLLBACK_COMPLETED) {
        last_error_message_ = message;
        update_in_progress_.store(false);
    } else if (state == UpdateState::UPDATE_SUCCESS) {
        update_in_progress_.store(false);
        // Update current version after successful update
        current_version_ = update_manager_->getCurrentFirmwareVersion();
    }
    
    LOG_INFO("State changed to: " + std::to_string(static_cast<int>(state)) + " - " + message);
    
    notifyStatusChange(message, state);
}

void OTAClient::notifyStatusChange(const std::string& message, UpdateState state) {
    if (status_callback_) {
        status_callback_(message, state);
    }
}

std::string OTAClient::getHardwareModel() const {
    if (!config_.hardware_model.empty()) {
        return config_.hardware_model;
    }
    
    // Try to detect hardware model
    struct utsname sys_info;
    if (uname(&sys_info) == 0) {
        return std::string(sys_info.machine);
    }
    
    return "unknown";
}

void OTAClient::handleUpdateProgress(const UpdateProgress& progress) {
    // Update our state if it changed
    if (progress.state != current_state_.load()) {
        setState(progress.state, progress.current_operation);
    }
    
    // Report progress to server
    reportUpdateStatus(progress.state, progress.current_operation, progress.progress_percentage);
}

bool OTAClient::shouldRetry() const {
    return retry_count_ < config_.max_retry_attempts;
}

void OTAClient::incrementRetryCount() {
    retry_count_++;
    LOG_INFO("Retry attempt " + std::to_string(retry_count_) + "/" + std::to_string(config_.max_retry_attempts));
}

void OTAClient::resetRetryCount() {
    retry_count_ = 0;
}

void OTAClient::waitForRetry() {
    LOG_INFO("Waiting " + std::to_string(config_.retry_delay_seconds) + " seconds before retry");
    std::this_thread::sleep_for(std::chrono::seconds(config_.retry_delay_seconds));
}