// server/database_manager.h
#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <memory>
#include "../common/include/types.h"
using namespace ota_common ;
using namespace std ;

namespace ota {

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();
    
    bool connect(const string& host, const string& user, 
                const string& password, const string& database);
    void disconnect();
    
    // Gestion des dispositifs
    bool registerDevice(const DeviceInfo& device);
    bool updateDeviceStatus(int32_t device_id, DeviceStatus status);
    DeviceInfo getDevice( int32_t device_id);
    
    // Gestion des mises à jour
    bool addUpdate(const UpdateInfo& update);
    UpdateInfo getLatestUpdate(const string& device_type, UpdateType update_type);
    vector<UpdateInfo> getAvailableUpdates(int32_t device_id);
    
    // Gestion des installations
    bool recordInstallation(const InstallationRecord& record);
    vector<InstallationRecord> getInstallationHistory(const string& device_id);
    
    bool initializeTables();

private:
    MYSQL* connection;
    bool connected;
    
    string escapeString(const string& str);
};

} // namespace ota

#endif // DATABASE_MANAGER_H
