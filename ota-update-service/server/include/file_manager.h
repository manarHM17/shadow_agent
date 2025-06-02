#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <fstream>
#include "../common/include/types.h"

using namespace std ;

namespace ota {

class FileManager {
public: 
    string CONFIG_UPDATE_DIR ;
    string APP_UPDATE_DIR ;
    string SERVICE_UPDATE_DIR ;
public:
    FileManager();
    ~FileManager();
    
    // Gestion des fichiers de mise à jour
    bool storeUpdateFile(const string& update_id, UpdateType update_type,
                        const vector<uint8_t>& data);
    bool getUpdateFile(const string& update_id, UpdateType update_type,
                      vector<uint8_t>& data);
    bool deleteUpdateFile(const string& update_id, UpdateType update_type);
    
    // Lecture par chunks pour le streaming
    bool readFileChunk(const string& file_path, int64_t offset,
                      int chunk_size, vector<uint8_t>& chunk);
    
    // Calcul de checksum
    string calculateChecksum(const string& file_path);
    string calculateChecksum(const vector<uint8_t>& data);
    
    // Utilitaires
    bool fileExists(const string& file_path);
    int64_t getFileSize(const string& file_path);
    bool createDirectories();
    
private:
    string getUpdateDirectory(UpdateType update_type);
    string getUpdateFilePath(const string& update_id, UpdateType update_type);
};

} // namespace ota
#endif