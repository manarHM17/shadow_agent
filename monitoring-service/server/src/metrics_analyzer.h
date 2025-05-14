#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

// Forward declaration
class AlertManager;

class MetricsAnalyzer {
public:
    // Structure for storing device state
    struct DeviceState {
        // Hardware metrics
        std::string cpu_usage;
        std::string memory_usage;
        std::string disk_usage;
        std::string usb_state;
        int gpio_state;
        
        // Software metrics
        std::string ip_address;
        std::string network_status;
        std::map<std::string, std::string> services;
        
        // Timestamps
        std::string last_hw_update;
        std::string last_sw_update;
    };
    
    // Constructor
    MetricsAnalyzer(AlertManager* alert_manager, const std::string& thresholds_path);
    
    // Process hardware metrics from a device
    void processHardwareMetrics(const std::string& device_id, const nlohmann::json& metrics);
    
    // Process software metrics from a device
    void processSoftwareMetrics(const std::string& device_id, const nlohmann::json& metrics);
    
    // Get the current state of a device
    DeviceState getDeviceState(const std::string& device_id);
    
    // Get all known device IDs
    std::vector<std::string> getAllDeviceIds();

private:
    AlertManager* alert_manager_;
    nlohmann::json thresholds_;
    
    // Device states
    std::map<std::string, DeviceState> device_states_;
    std::mutex devices_mutex_;
    
    // Analyze CPU usage
    void analyzeCpuUsage(const std::string& device_id, const std::string& cpu_usage);
    
    // Analyze memory usage
    void analyzeMemoryUsage(const std::string& device_id, const std::string& memory_usage);
    
    // Analyze disk usage
    void analyzeDiskUsage(const std::string& device_id, const std::string& disk_usage);
    
    // Analyze USB state
    void analyzeUsbState(const std::string& device_id, const std::string& usb_state);
    
    // Analyze GPIO state
    void analyzeGpioState(const std::string& device_id, int current_gpio_state, int previous_gpio_state) ;
    
    // Analyze network status
    void analyzeNetworkStatus(const std::string& device_id, const std::string& status);  // <- this line
    
    // Analyze services
    void analyzeServices(const std::string& device_id, const std::map<std::string, std::string>& services);
    
    // Helper to extract percentage value from string
    float extractPercentage(const std::string& percentage_str);
};