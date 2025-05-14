#pragma once

#include <string>
#include <cstdint>

// Function to extract device ID from token string format
int32_t DecodeDeviceIdFromToken(const std::string& token);

class JWTUtils {
public:
    // Generate a JWT token for a given device_id
    static std::string CreateToken(const std::string& device_id);

    // Validate a JWT token and extract device_id if valid
    static bool ValidateToken(const std::string& token, std::string& device_id);

private:
    // Secret key used to sign and verify tokens
    static const std::string SECRET_KEY;
};
