#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <string>
#include <vector>
#include <cstdint>

class ChecksumCalculator {
public:
    // Calculate SHA256 checksum from file
    static std::string calculateSHA256FromFile(const std::string& filepath);
    
    // Calculate SHA256 checksum from data buffer
    static std::string calculateSHA256FromData(const std::vector<uint8_t>& data);
    
    // Verify file against expected checksum
    static bool verifyFile(const std::string& filepath, const std::string& expected_checksum);
    
    // Verify data against expected checksum
    static bool verifyData(const std::vector<uint8_t>& data, const std::string& expected_checksum);

private:
    static std::string bytesToHex(const std::vector<uint8_t>& bytes);
};

#endif // CHECKSUM_H