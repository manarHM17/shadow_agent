#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

class MetricsCollector {
public:
    MetricsCollector(const std::string& log_dir_);

    struct HardwareMetrics {
        std::string device_id;
        std::string readable_date;
        std::string cpu_usage;
        std::string memory_usage;
        std::string disk_usage_root;
        std::string usb_data;
        int gpio_state;
    };

    struct SoftwareMetrics {
        std::string device_id;
        std::string readable_date;
        std::string ip_address;
        std::string uptime;
        std::string network_status;
        std::map<std::string, std::string> services;
    };
    //collect metrics from the latest log files 
    std::pair<std::string, std::string> collectMetrics();

    // Parse hardware metrics from JSON file
    HardwareMetrics parseHardwareMetrics(const std::string& file_path);

    // Parse software metrics from JSON file
    SoftwareMetrics parseSoftwareMetrics(const std::string& file_path);

    // Gets the device ID (either from configuration or from system)
    std::string getDeviceId() const;

private:
    std::string log_dir_;
    std::string device_id_;
    
    // Load device ID from configuration file or environment
    void loadDeviceId();

    // Parse JSON file
    nlohmann::json readJsonFile(const std::string& file_path);
};