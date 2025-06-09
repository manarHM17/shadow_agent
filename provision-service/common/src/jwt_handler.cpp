#include "../include/jwt_handler.h"
#include <jwt-cpp/jwt.h>
#include <openssl/sha.h>
#include <string>
#include <stdexcept>
#include <fstream>

// Define secret key
const std::string JWTUtils::SECRET_KEY = "P@ssw0rd20iot25";

std::string JWTUtils::CreateToken(const std::string& hostname , const std::string& device_id) {
    // Add expiration time (30 days from now)
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(24 * 30);
    
    auto token = jwt::create()
                     .set_issuer("iot-shadow")
                     .set_type("JWS")
                     .set_issued_at(now)
                     .set_expires_at(exp)
                     .set_payload_claim("device_id", jwt::claim(device_id))
                     .set_payload_claim("hostname", jwt::claim(hostname))
                     .sign(jwt::algorithm::hs256{SECRET_KEY});

    return token;
}

bool JWTUtils::ValidateToken(const std::string& token,  std::string& hostname ,std::string& device_id) {
    try {
        auto decoded = jwt::decode(token);
        
        // Verify token signature and claims
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{SECRET_KEY})
            .with_issuer("iot-shadow")
            .verify(decoded);
        
        // Check if token is expired
        auto exp = decoded.get_expires_at();
        auto now = std::chrono::system_clock::now();
        if (exp < now) {
            return false; // Token has expired
        }

        // Extract both device_id and hostname from token
        device_id = decoded.get_payload_claim("device_id").as_string();
        hostname = decoded.get_payload_claim("hostname").as_string();
        
        // Verify both fields are present
        if (device_id.empty() || hostname.empty()) {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}