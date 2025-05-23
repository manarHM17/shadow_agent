#include "../include/checksum.h"
#include "../include/logging.h"
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

std::string ChecksumCalculator::calculateSHA256FromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for checksum calculation: " + filepath);
        return "";
    }
    
    SHA256_CTX sha256_ctx;
    SHA256_Init(&sha256_ctx);
    
    const size_t buffer_size = 8192;
    char buffer[buffer_size];
    
    while (file.read(buffer, buffer_size) || file.gcount() > 0) {
        SHA256_Update(&sha256_ctx, buffer, file.gcount());
    }
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256_ctx);
    
    std::vector<uint8_t> hash_vec(hash, hash + SHA256_DIGEST_LENGTH);
    return bytesToHex(hash_vec);
}

std::string ChecksumCalculator::calculateSHA256FromData(const std::vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    
    std::vector<uint8_t> hash_vec(hash, hash + SHA256_DIGEST_LENGTH);
    return bytesToHex(hash_vec);
}

bool ChecksumCalculator::verifyFile(const std::string& filepath, const std::string& expected_checksum) {
    std::string calculated_checksum = calculateSHA256FromFile(filepath);
    if (calculated_checksum.empty()) {
        return false;
    }
    
    // Convert to lowercase for comparison
    std::string expected_lower = expected_checksum;
    std::string calculated_lower = calculated_checksum;
    
    std::transform(expected_lower.begin(), expected_lower.end(), expected_lower.begin(), ::tolower);
    std::transform(calculated_lower.begin(), calculated_lower.end(), calculated_lower.begin(), ::tolower);
    
    bool match = (expected_lower == calculated_lower);
    
    if (match) {
        LOG_INFO("Checksum verification successful for file: " + filepath);
    } else {
        LOG_ERROR("Checksum verification failed for file: " + filepath);
        LOG_ERROR("Expected: " + expected_checksum);
        LOG_ERROR("Calculated: " + calculated_checksum);
    }
    
    return match;
}

bool ChecksumCalculator::verifyData(const std::vector<uint8_t>& data, const std::string& expected_checksum) {
    std::string calculated_checksum = calculateSHA256FromData(data);
    
    // Convert to lowercase for comparison
    std::string expected_lower = expected_checksum;
    std::string calculated_lower = calculated_checksum;
    
    std::transform(expected_lower.begin(), expected_lower.end(), expected_lower.begin(), ::tolower);
    std::transform(calculated_lower.begin(), calculated_lower.end(), calculated_lower.begin(), ::tolower);
    
    bool match = (expected_lower == calculated_lower);
    
    if (match) {
        LOG_INFO("Data checksum verification successful");
    } else {
        LOG_ERROR("Data checksum verification failed");
        LOG_ERROR("Expected: " + expected_checksum);
        LOG_ERROR("Calculated: " + calculated_checksum);
    }
    
    return match;
}

std::string ChecksumCalculator::bytesToHex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : bytes) {
        ss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return ss.str();
}