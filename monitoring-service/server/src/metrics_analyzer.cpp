#include "metrics_analyzer.h"
#include "alert_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>


MetricsAnalyzer::MetricsAnalyzer(AlertManager* alert_manager, const std::string& thresholds_path)
    : alert_manager_(alert_manager) {
    // Initialize with default values
    thresholds_ = {
        {"cpu", {{"warning", 75.0}, {"critical", 90.0}}},
        {"memory", {{"warning", 80.0}, {"critical", 95.0}}},
        {"disk", {{"warning", 85.0}, {"critical", 95.0}}}
    };
}

void MetricsAnalyzer::processHardwareMetrics(const std::string& device_id, const nlohmann::json& metrics) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    // Check if device exists in our map, if not, initialize it
    if (device_states_.find(device_id) == device_states_.end()) {
        device_states_[device_id] = DeviceState();
    }
    
    // Get reference to the device state
    DeviceState& state = device_states_[device_id];
    
    // Store previous GPIO state for comparison
    int previous_gpio_state = -1;
    if (!state.gpio_state) {
        try {
            previous_gpio_state = state.gpio_state;
        } catch (...) {
            previous_gpio_state = -1;
        }
    }
    
    // Update device state with new hardware metrics
    try {
        if (metrics.contains("cpu_usage")) {
            state.cpu_usage = metrics["cpu_usage"].get<std::string>();
            analyzeCpuUsage(device_id, state.cpu_usage);
        }
        
        if (metrics.contains("memory_usage")) {
            state.memory_usage = metrics["memory_usage"].get<std::string>();
            analyzeMemoryUsage(device_id, state.memory_usage);
        }
        
        if (metrics.contains("disk_usage")) {
            state.disk_usage = metrics["disk_usage"].get<std::string>();
            analyzeDiskUsage(device_id, state.disk_usage);
        }
        
        if (metrics.contains("usb_state")) {
            state.usb_state = metrics["usb_state"].get<std::string>();
            analyzeUsbState(device_id, state.usb_state);
        }
        
        if (metrics.contains("gpio_state")) {
                state.gpio_state = metrics["gpio_state"].get<int>();  // Expecting an integer for GPIO count or status
                analyzeGpioState(device_id, state.gpio_state, previous_gpio_state); // Make sure previous_gpio_state is passed correctly
            }
        
        // Update timestamp
        if (metrics.contains("timestamp")) {
            state.last_hw_update = metrics["timestamp"].get<std::string>();
        } else {
            // Generate timestamp if not provided
            auto now = std::chrono::system_clock::now();
            auto now_time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
            state.last_hw_update = ss.str();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing hardware metrics for device " << device_id 
                  << ": " << e.what() << std::endl;
    }
}

void MetricsAnalyzer::processSoftwareMetrics(const std::string& device_id, const nlohmann::json& metrics) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    // Check if device exists in our map, if not, initialize it
    if (device_states_.find(device_id) == device_states_.end()) {
        device_states_[device_id] = DeviceState();
    }
    
    // Get reference to the device state
    DeviceState& state = device_states_[device_id];
    
    // Update device state with new software metrics
    try {
        if (metrics.contains("ip_address")) {
            state.ip_address = metrics["ip_address"].get<std::string>();
        }
        
        if (metrics.contains("network_status")) {
            state.network_status = metrics["network_status"].get<std::string>();
            analyzeNetworkStatus(device_id, state.network_status);
        }
        
        if (metrics.contains("services")) {
            // Clear previous services state
            state.services.clear();
            
            // Update with new services state
            auto services = metrics["services"];
            for (auto& [service, status] : services.items()) {
                state.services[service] = status.get<std::string>();
            }
            
            analyzeServices(device_id, state.services);
        }
        
        // Update timestamp
        if (metrics.contains("timestamp")) {
            state.last_sw_update = metrics["timestamp"].get<std::string>();
        } else {
            // Generate timestamp if not provided
            auto now = std::chrono::system_clock::now();
            auto now_time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
            state.last_sw_update = ss.str();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing software metrics for device " << device_id 
                  << ": " << e.what() << std::endl;
    }
}

MetricsAnalyzer::DeviceState MetricsAnalyzer::getDeviceState(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    if (device_states_.find(device_id) != device_states_.end()) {
        return device_states_[device_id];
    }
    
    // Return empty state if device not found
    return DeviceState();
}

std::vector<std::string> MetricsAnalyzer::getAllDeviceIds() {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<std::string> device_ids;
    device_ids.reserve(device_states_.size());
    
    for (const auto& [id, _] : device_states_) {
        device_ids.push_back(id);
    }
    
    return device_ids;
}

void MetricsAnalyzer::analyzeCpuUsage(const std::string& device_id, const std::string& cpu_usage) {
    float usage = extractPercentage(cpu_usage);
    
    if (std::isnan(usage)) {
        return;  // Invalid value, skip analysis
    }
    
    float warning_threshold = thresholds_["cpu"]["warning"].get<float>();
    float critical_threshold = thresholds_["cpu"]["critical"].get<float>();
    
    if (usage >= critical_threshold) {
        // Send critical alert
        alert_manager_->sendAlert(
            device_id,
            AlertManager::AlertSeverity::CRITICAL,
            "HIGH_CPU_USAGE",
            "CPU usage is critically high: " + cpu_usage,
            "Check for runaway processes or resource leaks"
        );
    } else if (usage >= warning_threshold) {
        // Send warning alert
        alert_manager_->sendAlert(
            device_id,
            AlertManager::AlertSeverity::WARNING,
            "ELEVATED_CPU_USAGE",
            "CPU usage is elevated: " + cpu_usage,
            "Monitor system performance and check active processes"
        );
    }
}

void MetricsAnalyzer::analyzeMemoryUsage(const std::string& device_id, const std::string& memory_usage) {
    float usage = extractPercentage(memory_usage);
    
    if (std::isnan(usage)) {
        return;  // Invalid value, skip analysis
    }
    
    float warning_threshold = thresholds_["memory"]["warning"].get<float>();
    float critical_threshold = thresholds_["memory"]["critical"].get<float>();
    
    if (usage >= critical_threshold) {
        // Send critical alert
        alert_manager_->sendAlert(
            device_id,
            AlertManager::AlertSeverity::CRITICAL,
            "HIGH_MEMORY_USAGE",
            "Memory usage is critically high: " + memory_usage,
            "Check for memory leaks or increase available memory"
        );
    } else if (usage >= warning_threshold) {
        // Send warning alert
        alert_manager_->sendAlert(
            device_id,
            AlertManager::AlertSeverity::WARNING,
            "ELEVATED_MEMORY_USAGE",
            "Memory usage is elevated: " + memory_usage,
            "Monitor memory consumption and identify memory-intensive processes"
        );
    }
}

void MetricsAnalyzer::analyzeDiskUsage(const std::string& device_id, const std::string& disk_usage) {
    float usage = extractPercentage(disk_usage);
    
    if (std::isnan(usage)) {
        return;  // Invalid value, skip analysis
    }
    
    float warning_threshold = thresholds_["disk"]["warning"].get<float>();
    float critical_threshold = thresholds_["disk"]["critical"].get<float>();
    
    if (usage >= critical_threshold) {
        // Send critical alert
        alert_manager_->sendAlert(
            device_id,
            AlertManager::AlertSeverity::CRITICAL,
            "HIGH_DISK_USAGE",
            "Disk usage is critically high: " + disk_usage,
            "Free up disk space immediately or expand storage"
        );
    } else if (usage >= warning_threshold) {
        // Send warning alert
        alert_manager_->sendAlert(
            device_id,
            AlertManager::AlertSeverity::WARNING,
            "ELEVATED_DISK_USAGE",
            "Disk usage is elevated: " + disk_usage,
            "Cleanup unnecessary files or plan for storage expansion"
        );
    }
}

void MetricsAnalyzer::analyzeUsbState(const std::string& device_id, const std::string& usb_state) {
    if (usb_state != "none") {
        if (usb_state.find("1d6b") == std::string::npos) {
            std::cout << "[ALERT] USB peripheral detected on device " << device_id << std::endl;

            if (alert_manager_) {
            // Informational alert for USB connection
            alert_manager_->sendAlert(
                device_id,
                AlertManager::AlertSeverity::INFO,
                "USB_CONNECTED",
                "USB device connected",
                "You are not autorised to use extra USB peripheral "        );
                }
            }
        }
    }


void MetricsAnalyzer::analyzeGpioState(const std::string& device_id, int current_gpio_state, int previous_gpio_state) {
    if (current_gpio_state != previous_gpio_state) {
        std::cout << "[ALERT] New GPIO state detected on device " << device_id << std::endl;

        alert_manager_->sendAlert(
            device_id,
            AlertManager::AlertSeverity::INFO,
            "NEW_GPIO_DETECTED",
            "New GPIO pins detected",
            "Check the GPIO configuration"
        );
    }
}


void MetricsAnalyzer::analyzeServices(const std::string& device_id, const std::map<std::string, std::string>& services) {
    std::vector<std::string> essential_services = {"mqtt", "ssh"}; // Define essential services
    
    for (const auto& service_name : essential_services) {
        auto it = services.find(service_name);
        if (it != services.end()) {
            if (it->second == "inactive") {
                alert_manager_->sendAlert(
                    device_id,
                    AlertManager::AlertSeverity::CRITICAL,
                    "SERVICE_DOWN",
                    "Service " + service_name + " is inactive",
                    "Check service logs and attempt to restart the service"
                );
            }
        } else {
            // Service not found, consider it inactive
            alert_manager_->sendAlert(
                device_id,
                AlertManager::AlertSeverity::CRITICAL,
                "SERVICE_DOWN",
                "Service " + service_name + " is not found",
                "Check service configuration and ensure it is running"
            );
        }
    }
}
void MetricsAnalyzer::analyzeNetworkStatus(const std::string& device_id, const std::string& status) {
    if (status == "inreachable") {
        alert_manager_->sendAlert(
            device_id,
            AlertManager::AlertSeverity::CRITICAL,
            "NETWORK_UNREACHABLE",
            "Device network status reported as 'inreachable'",
            "Verify network interfaces and ensure connectivity to the device"
        );
    }
}

                                    


float MetricsAnalyzer::extractPercentage(const std::string& percentage_str) {
    try {
        // Remove '%' character if present
        std::string clean_str = percentage_str;
        size_t percent_pos = clean_str.find('%');
        if (percent_pos != std::string::npos) {
            clean_str = clean_str.substr(0, percent_pos);
        }
        
        // Convert to float
        return std::stof(clean_str);
    } catch (const std::exception& e) {
        std::cerr << "Error extracting percentage from string '" << percentage_str 
                  << "': " << e.what() << std::endl;
        return std::nanf("");
    }
}
