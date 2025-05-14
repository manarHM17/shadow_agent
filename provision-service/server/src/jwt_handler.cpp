#include "../include/jwt_handler.hpp"
#include <jwt-cpp/jwt.h>
#include <openssl/sha.h>
#include <string>
#include <stdexcept>
#include <fstream>


// Définir la clé secrète
const std::string JWTUtils::SECRET_KEY = std::getenv("JWT_SECRET_KEY");



//Implémentation de CreateToken

std::string JWTUtils::CreateToken(const std::string& device_id) {
    // Add expiration time (30 days from now)
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(24 * 256);
    
    auto token = jwt::create()
                     .set_issuer("iot-shadow")
                     .set_type("JWS")
                     .set_issued_at(now)
                     .set_expires_at(exp)
                     .set_payload_claim("device_id", jwt::claim(device_id))
                     .sign(jwt::algorithm::hs256{SECRET_KEY});

    return token;
}

// Implémentation de ValidateToken
bool JWTUtils::ValidateToken(const std::string& token, std::string& device_id) {
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

        // Extraire le device_id du token
        device_id = decoded.get_payload_claim("device_id").as_string();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}
