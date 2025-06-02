#include "../include/file_manager.h"
#include "../../common/include/types.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <openssl/sha.h>
using namespace ota_common  ;
using namespace std ;
namespace ota {

FileManager::FileManager() {
    createDirectories();
}

FileManager::~FileManager() {}

bool FileManager::createDirectories() {
    // Créer les répertoires nécessaires
    mkdir("/tmp/ota_updates", 0755);
    mkdir(CONFIG_UPDATE_DIR.c_str(), 0755);
    mkdir(APP_UPDATE_DIR.c_str(), 0755);
    mkdir(SERVICE_UPDATE_DIR.c_str(), 0755);
    return true;
}

string FileManager::getUpdateDirectory(UpdateType update_type) {
    switch (update_type) {
        case UpdateType::CONFIG : return CONFIG_UPDATE_DIR;
        case UpdateType::APPLICATION : return APP_UPDATE_DIR;
        case UpdateType::SYSTEM_SERVICE : return SERVICE_UPDATE_DIR;
        default: return CONFIG_UPDATE_DIR;
    }
}

string FileManager::getUpdateFilePath(const string& update_id, UpdateType update_type) {
    return getUpdateDirectory(update_type) + update_id;
}

bool FileManager::storeUpdateFile(const string& update_id, UpdateType update_type,
                                 const vector<uint8_t>& data) {
    string file_path = getUpdateFilePath(update_id, update_type);
    
    ofstream file(file_path, ios::binary);
    if (!file.is_open()) {
        cerr << "Failed to create update file: " << file_path << endl;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
    
    return file.good();
}

bool FileManager::getUpdateFile(const string& update_id, UpdateType update_type,
                               vector<uint8_t>& data) {
    string file_path = getUpdateFilePath(update_id, update_type);
    
    ifstream file(file_path, ios::binary | ios::ate);
    if (!file.is_open()) {
        return false;
    }
    
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    
    data.resize(size);
    return file.read(reinterpret_cast<char*>(data.data()), size).good();
}

bool FileManager::readFileChunk(const string& file_path, int64_t offset,
                               int chunk_size, vector<uint8_t>& chunk) {
    ifstream file(file_path, ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.seekg(offset);
    if (file.fail()) {
        return false;
    }
    
    chunk.resize(chunk_size);
    file.read(reinterpret_cast<char*>(chunk.data()), chunk_size);
    
    streamsize bytes_read = file.gcount();
    chunk.resize(bytes_read);
    
    return bytes_read > 0;
}

bool FileManager::deleteUpdateFile(const string& update_id, UpdateType update_type) {
    string file_path = getUpdateFilePath(update_id, update_type);
    return remove(file_path.c_str()) == 0;
}

string FileManager::calculateChecksum(const string& file_path) {
    ifstream file(file_path, ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        SHA256_Update(&ctx, buffer, file.gcount());
    }
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);
    
    ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    
    return oss.str();
}

string FileManager::calculateChecksum(const vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    
    ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    
    return oss.str();
}

bool FileManager::fileExists(const string& file_path) {
    struct stat buffer;
    return stat(file_path.c_str(), &buffer) == 0;
}

int64_t FileManager::getFileSize(const string& file_path) {
    struct stat buffer;
    if (stat(file_path.c_str(), &buffer) == 0) {
        return buffer.st_size;
    }
    return -1;
}

} // namespace ota