#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/client_context.h>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include "ota_service.grpc.pb.h"
#include "partition_manager.h"
#include "update_manager.h"

class OTAClient {
public:
    // Callback pour les événements OTA
    using StatusCallback = std::function<void(const std::string& message, UpdateState state)>;
    
    // Configuration du client
    struct ClientConfig {
        std::string server_address = "localhost:50051";
        int32_t device_id;
        std::string hardware_model;
        std::string temp_directory = "/tmp/ota";
        int check_interval_seconds = 300; // 5 minutes
        int status_report_interval_seconds = 60; // 1 minute
        int max_retry_attempts = 3;
        int retry_delay_seconds = 5;
    };

private:
    // Configuration
    ClientConfig config_;
    
    // gRPC
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<ota::OTAService::Stub> stub_;
    
    // Managers
    std::unique_ptr<UpdateManager> update_manager_;
    std::unique_ptr<PartitionManager> partition_manager_;
    
    // Threading
    std::thread check_updates_thread_;
    std::thread status_report_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> update_in_progress_;
    
    // State
    std::string current_version_;
    std::atomic<UpdateState> current_state_;
    std::string last_error_message_;
    std::mutex state_mutex_;
    
    // Callbacks
    StatusCallback status_callback_;
    
    // Retry mechanism
    int retry_count_;
    std::chrono::steady_clock::time_point last_check_time_;

public:
    explicit OTAClient(const ClientConfig& config);
    ~OTAClient();
    
    // Lifecycle
    bool initialize();
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Configuration
    void setStatusCallback(StatusCallback callback);
    
    // Status
    UpdateState getCurrentState() const { return current_state_.load(); }
    std::string getCurrentVersion() const { return current_version_; }
    UpdateProgress getUpdateProgress() const;
    
    // Manual operations
    bool checkForUpdatesNow();
    bool cancelCurrentUpdate();
    bool performRollback();

private:
    // gRPC operations
    bool connectToServer();
    void checkUpdatesLoop();
    void statusReportLoop();
    
    // Update process
    bool processAvailableUpdate(const ota::FirmwareInfo& firmware_info);
    bool downloadFirmware(const ota::FirmwareInfo& firmware_info);
    bool reportUpdateStatus(UpdateState state, const std::string& message = "", int progress = 0);
    
    // Utility
    void setState(UpdateState state, const std::string& message = "");
    void notifyStatusChange(const std::string& message, UpdateState state);
    std::string getHardwareModel() const;
    void handleUpdateProgress(const UpdateProgress& progress);
    
    // Retry logic
    bool shouldRetry() const;
    void incrementRetryCount();
    void resetRetryCount();
    void waitForRetry();
};
