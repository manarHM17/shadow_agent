#include "../include/ConfigManager.h"
#include <fstream>
#include <iostream>

std::string ConfigManager::config_path = "../config/config.txt";

bool ConfigManager::saveDeviceInfo(const std::string& hostname, const std::string& device_id) {
    ensureConfigDirExists();
    try {
        std::ofstream config_file(config_path);
        if (!config_file.is_open()) {
            std::cerr << "Unable to open config file for writing" << std::endl;
            return false;
        }
        
        config_file << hostname << std::endl;
        config_file << device_id << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving config: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::loadDeviceInfo(std::string& hostname, std::string& device_id) {
    try {
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            return false;
        }
        
        if (!std::getline(config_file, hostname)) {
            return false;
        }
        
        if (!std::getline(config_file, device_id)) {
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return false;
    }
}

void ConfigManager::ensureConfigDirExists() {
    std::filesystem::path dir = std::filesystem::path(config_path).parent_path();
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
        std::cout << "Created config directory at: " << dir << std::endl;
    }
}