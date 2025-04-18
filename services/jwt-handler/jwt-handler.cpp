#include "jwt-handler.hpp"
#include <jwt-cpp/jwt.h>
#include <openssl/sha.h>

// Définir la clé secrète
const std::string JWTUtils::SECRET_KEY = "P@ssw0rd20iot25";

// Implémentation de CreateToken
std::string JWTUtils::CreateToken(const std::string& device_id) {
    auto token = jwt::create()
                     .set_issuer("iot-shadow")
                     .set_type("JWS")
                     .set_payload_claim("device_id", jwt::claim(device_id))
                     .sign(jwt::algorithm::hs256{SECRET_KEY});

    return token;
}

// Implémentation de ValidateToken
bool JWTUtils::ValidateToken(const std::string& token, std::string& device_id) {
    try {
        auto decoded = jwt::decode(token);
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{SECRET_KEY})
            .with_issuer("iot-shadow")
            .verify(decoded);

        // Extraire le device_id du token
        device_id = decoded.get_payload_claim("device_id").as_string();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}