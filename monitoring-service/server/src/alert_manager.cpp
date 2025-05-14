#include "alert_manager.h"
#include <iostream>
#include <chrono>
#include <sstream> // For std::to_string

AlertManager::AlertManager() {}

void AlertManager::sendAlert(const std::string& device_id,
                             AlertSeverity severity,
                             const std::string& alert_type,
                             const std::string& description,
                             const std::string& recommended_action) {
    monitoring::Alert alert;
    alert.set_device_id(device_id);
    alert.set_severity(convertSeverity(severity));
    alert.set_alert_type(alert_type);
    alert.set_description(description);
    alert.set_recommended_action(recommended_action);

    // Set timestamp as string
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    alert.set_timestamp(std::to_string(now_ms)); // Convert long int to string

    std::lock_guard<std::mutex> lock(devices_mutex_);

    auto it = devices_.find(device_id);
    if (it != devices_.end()) {
        try {
            if (it->second.stream) {
                if (!it->second.stream->Write(alert)) {
                    std::cerr << "Failed to send alert to device: " << device_id << std::endl;
                    devices_.erase(it);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception sending alert to device " << device_id
                      << ": " << e.what() << std::endl;
            devices_.erase(it);
        }
    } else {
        std::cout << "Alert generated for non-connected device " << device_id
                  << " - Type: " << alert_type
                  << " - Severity: " << static_cast<int>(severity) << std::endl;
        return;
    }

    std::cout << "Alert generated for device " << device_id
              << " - Type: " << alert_type
              << " - Severity: " << static_cast<int>(severity)
              << " - Description: " << description << std::endl;
}

void AlertManager::registerDevice(const std::string& device_id, 
                                 grpc::ServerWriter<monitoring::Alert>* stream) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    DeviceConnection connection{
        stream,
        std::chrono::system_clock::now()
    };
    
    devices_[device_id] = connection;
    
    std::cout << "Device registered: " << device_id << std::endl;
    
    monitoring::Alert welcome_alert;
    welcome_alert.set_device_id(device_id);
    welcome_alert.set_severity(monitoring::Alert::INFO);
    welcome_alert.set_alert_type("CONNECTION_ESTABLISHED");
    welcome_alert.set_description("Successfully connected to monitoring server");
    welcome_alert.set_recommended_action("No action needed");
    
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    welcome_alert.set_timestamp(std::to_string(now_ms)); // Convert long int to string
    
    try {
        if (stream) {
            stream->Write(welcome_alert);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception sending welcome alert to device " << device_id 
                  << ": " << e.what() << std::endl;
    }
}

void AlertManager::unregisterDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = devices_.find(device_id);
    if (it != devices_.end()) {
        devices_.erase(it);
        std::cout << "Device unregistered: " << device_id << std::endl;
    }
}

bool AlertManager::isDeviceConnected(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    return devices_.find(device_id) != devices_.end();
}

std::vector<std::string> AlertManager::getConnectedDevices() {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<std::string> connected_devices;
    connected_devices.reserve(devices_.size());
    
    for (const auto& [device_id, _] : devices_) {
        connected_devices.push_back(device_id);
    }
    
    return connected_devices;
}

monitoring::Alert::Severity AlertManager::convertSeverity(AlertSeverity severity) {
    switch (severity) {
        case AlertSeverity::INFO:
            return monitoring::Alert::INFO;
        case AlertSeverity::WARNING:
            return monitoring::Alert::WARNING;
        case AlertSeverity::CRITICAL:
            return monitoring::Alert::CRITICAL;
        default:
            return monitoring::Alert::INFO;
    }
}
