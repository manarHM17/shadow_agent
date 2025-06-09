#include "jwt_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

JWTManager::JWTManager(const std::string& secret_key) : secret_key_(secret_key) {}

std::string JWTManager::generateToken(const std::string& hostname, int device_id) {
    // Header
    nlohmann::json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    
    // Payload
    nlohmann::json payload;
    payload["hostname"] = hostname;
    if (device_id != -1) {
        payload["device_id"] = device_id;
    }
    
    auto now = std::chrono::system_clock::now();
    auto expire = now + std::chrono::hours(24); // Token valide 24h
    
    payload["iat"] = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    payload["exp"] = std::chrono::duration_cast<std::chrono::seconds>(expire.time_since_epoch()).count();
    
    // Encode header and payload
    std::string encoded_header = base64_encode(header.dump());
    std::string encoded_payload = base64_encode(payload.dump());
    
    // Create signature
    std::string data = encoded_header + "." + encoded_payload;
    std::string signature = base64_encode(hmac_sha256(data, secret_key_));
    
    return encoded_header + "." + encoded_payload + "." + signature;
}

bool JWTManager::validateToken(const std::string& token) {
    try {
        size_t first_dot = token.find('.');
        size_t second_dot = token.find('.', first_dot + 1);
        
        if (first_dot == std::string::npos || second_dot == std::string::npos) {
            return false;
        }
        
        std::string header = token.substr(0, first_dot);
        std::string payload = token.substr(first_dot + 1, second_dot - first_dot - 1);
        std::string signature = token.substr(second_dot + 1);
        
        // Verify signature
        std::string data = header + "." + payload;
        std::string expected_signature = base64_encode(hmac_sha256(data, secret_key_));
        
        if (signature != expected_signature) {
            return false;
        }
        
        // Check expiration
        nlohmann::json payload_json = nlohmann::json::parse(base64_decode(payload));
        auto now = std::chrono::system_clock::now();
        auto current_time = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        
        if (payload_json.contains("exp") && payload_json["exp"] < current_time) {
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

nlohmann::json JWTManager::decodeToken(const std::string& token) {
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);
    
    std::string payload = token.substr(first_dot + 1, second_dot - first_dot - 1);
    return nlohmann::json::parse(base64_decode(payload));
}

std::string JWTManager::base64_encode(const std::string& data) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data.c_str(), data.length());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    
    return result;
}

std::string JWTManager::base64_decode(const std::string& encoded) {
    BIO *bio, *b64;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(encoded.c_str(), -1);
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    
    char buffer[1024];
    int length = BIO_read(bio, buffer, 1024);
    BIO_free_all(bio);
    
    return std::string(buffer, length);
}

std::string JWTManager::hmac_sha256(const std::string& data, const std::string& key) {
    unsigned char result[SHA256_DIGEST_LENGTH];
    unsigned int len = SHA256_DIGEST_LENGTH;
    
    HMAC(EVP_sha256(), key.c_str(), key.length(), 
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), 
         result, &len);
    
    return std::string(reinterpret_cast<char*>(result), len);
}