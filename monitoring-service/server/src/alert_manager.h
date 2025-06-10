#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include <monitoring.grpc.pb.h>

class AlertManager {
public:
    AlertManager();
    
    enum AlertSeverity {
        INFO,
        WARNING,
        CRITICAL
    };
    
    void sendAlert(const std::string& device_id, 
                  AlertSeverity severity,
                  const std::string& alert_type,
                  const std::string& description,
                  const std::string& recommended_action,
                  const std::string& corrective_command = "");
    
    // Change to raw pointer
    void registerDevice(const std::string& device_id, 
                       grpc::ServerWriter<monitoring::Alert>* stream);
    
    void unregisterDevice(const std::string& device_id);
    
    bool isDeviceConnected(const std::string& device_id);
    
    std::vector<std::string> getConnectedDevices();

private:
    struct DeviceConnection {
        grpc::ServerWriter<monitoring::Alert>* stream; // Raw pointer
        std::chrono::system_clock::time_point last_update;
    };
    
    std::map<std::string, DeviceConnection> devices_;
    std::mutex devices_mutex_;
    
    monitoring::Alert::Severity convertSeverity(AlertSeverity severity);
};