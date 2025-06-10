#include "metrics_collector.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <array>
#include <filesystem>
#include <cstdio>
#include <chrono>
#include <regex>

namespace fs = std::filesystem;

MetricsCollector::MetricsCollector(const std::string& log_dir) 
    : log_dir_(log_dir) {
    loadDeviceId();
}

std::pair<std::string, std::string> MetricsCollector::collectMetrics() {
    std::cout << "Searching for latest metrics files in: " << log_dir_ << std::endl;

    std::vector<fs::directory_entry> hw_files;
    std::vector<fs::directory_entry> sw_files;

    // Récupère tous les fichiers metrics
    for (const auto& entry : fs::directory_iterator(log_dir_)) {
        if (!fs::is_regular_file(entry)) continue;
        std::string filename = entry.path().filename().string();

        if (filename.find("hardware_metrics") != std::string::npos) {
            hw_files.push_back(entry);
        } else if (filename.find("software_metrics") != std::string::npos) {
            sw_files.push_back(entry);
        }
    }

    // Trie les fichiers par date (extrait depuis le nom)
    auto fileTimeComparator = [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return a.path().filename().string() > b.path().filename().string(); 
    };

    std::sort(hw_files.begin(), hw_files.end(), fileTimeComparator);
    std::sort(sw_files.begin(), sw_files.end(), fileTimeComparator);

    // Accept at least 1 file of each type
    if (hw_files.empty() || sw_files.empty()) {
        std::string error = "[Error] Not enough metric files. Found " + 
                            std::to_string(hw_files.size()) + " hardware, " +
                            std::to_string(sw_files.size()) + " software.";
        std::cerr << error << std::endl;
        throw std::runtime_error(error);
    }

    // Use the most recent file if only one exists, otherwise the second most recent
    std::string hw_file_path = hw_files.size() > 1 ? hw_files[1].path().string() : hw_files[0].path().string();
    std::string sw_file_path = sw_files.size() > 1 ? sw_files[1].path().string() : sw_files[0].path().string();

    std::cout << "Selected files: " << hw_file_path << " and " << sw_file_path << std::endl;
    return {hw_file_path, sw_file_path};
}


MetricsCollector::HardwareMetrics MetricsCollector::parseHardwareMetrics(const std::string& file_path) {
    nlohmann::json json = readJsonFile(file_path);

    HardwareMetrics metrics;
    metrics.device_id = device_id_;
    metrics.readable_date = json["readable_date"];
    metrics.cpu_usage = json["cpu_usage"];
    metrics.memory_usage = json["memory_usage"];
    metrics.disk_usage_root = json["disk_usage"];
    metrics.usb_data = json.contains("usb_state") ? json["usb_state"].get<std::string>() : "none";
    metrics.gpio_state = json["gpio_state"];
    metrics.kernel_version = json.value("kernel_version", "");
    metrics.hardware_model = json.value("hardware_model", "");
    metrics.firmware_version = json.value("firmware_version", "");
    return metrics;
}

MetricsCollector::SoftwareMetrics MetricsCollector::parseSoftwareMetrics(const std::string& file_path) {
    nlohmann::json json = readJsonFile(file_path);

    SoftwareMetrics metrics;
    metrics.device_id = device_id_;
    metrics.readable_date = json["readable_date"];
    metrics.ip_address = json["ip_address"];
    metrics.uptime = json["uptime"];
    metrics.network_status = json["network_status"];
    metrics.os_version = json.value("os_version", "");
    // Parse applications array
    if (json.contains("applications") && json["applications"].is_array()) {
        for (const auto& app : json["applications"]) {
            metrics.applications.push_back({
                app["name"].get<std::string>(),
                app["version"].get<std::string>()
            });
        }
    }
    // Parse services
    for (auto& [service, status] : json["services"].items()) {
        metrics.services[service] = status;
    }
    return metrics;
}

nlohmann::json MetricsCollector::readJsonFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " + file_path);
    }
    
    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse JSON file: " + std::string(e.what()));
    }
    
    return json;
}

std::string MetricsCollector::getDeviceId() const {
    return device_id_;
}

void MetricsCollector::loadDeviceId() {
    // Change path to config.txt
    std::string config_file = (fs::path(log_dir_).parent_path() / "config" / "config.txt").string();

    if (fs::exists(config_file)) {
        std::ifstream file(config_file);
        if (file.is_open()) {
            std::string line;
            if (std::getline(file, line)) {
                // Use the first line as device ID
                device_id_ = line;
                if (!device_id_.empty()) {
                    std::cout << "Loaded device ID from config: " << device_id_ << std::endl;
                    return;
                }
            }
        }
    }
    std::cerr << "Failed to load device ID from config file." << std::endl;
}

