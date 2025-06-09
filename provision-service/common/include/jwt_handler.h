#pragma once

#include <string>
#include <cstdint>

// Function to extract device ID from token string format
int32_t DecodeDeviceIdFromToken(const std::string& token);

#pragma once

#include <string>
#include <cstdint>

class JWTUtils {
public:
    // Generate a JWT token for a given device_id and hostname
    static std::string CreateToken(const std::string& device_id, const std::string& hostname);

    // Validate a JWT token and extract device_id and hostname if valid
    static bool ValidateToken(const std::string& token, std::string& device_id, std::string& hostname);

private:
    static const std::string SECRET_KEY;
};