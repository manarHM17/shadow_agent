#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <chrono>

class JWTManager {
public:
    JWTManager(const std::string& secret_key);
    
    std::string generateToken(const std::string& hostname, int device_id = -1);
    bool validateToken(const std::string& token);
    nlohmann::json decodeToken(const std::string& token);
    
private:
    std::string secret_key_;
    std::string base64_encode(const std::string& data);
    std::string base64_decode(const std::string& encoded);
    std::string hmac_sha256(const std::string& data, const std::string& key);
};