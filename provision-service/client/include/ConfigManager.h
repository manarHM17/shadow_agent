#pragma once
#include <string>
#include <filesystem>

class ConfigManager {
public:
    static bool saveDeviceInfo(const std::string& hostname, const std::string& device_id);
    static bool loadDeviceInfo(std::string& hostname, std::string& device_id);
    
private:
    static std::string config_path;
    static void ensureConfigDirExists();
};