// client/ota_client.h
#ifndef OTA_CLIENT_H
#define OTA_CLIENT_H

#include <grpcpp/grpcpp.h>
#include "../common/include/ota_service.grpc.pb.h"
#include <string>
#include <memory>

namespace ota {

class OTAClient {
public:
    OTAClient(const std::string& server_address);
    ~OTAClient();
    
    // Gestion des dispositifs
    bool registerDevice(const std::string& device_id, const std::string& device_type,
                       const std::string& current_version, const std::string& platform);
    
    // Vérification et téléchargement des mises à jour
    bool checkForUpdates(const std::string& device_id, const std::string& current_version,
                        int update_type, ota::UpdateResponse& update_info);
    
    bool downloadUpdate(const std::string& device_id, const std::string& update_id,
                       const std::string& local_path);
    
    bool confirmInstallation(const std::string& device_id, const std::string& update_id,
                           bool success, const std::string& error_message = "");
    
    // Statut du dispositif
    bool getDeviceStatus(const std::string& device_id, ota::DeviceStatusResponse& status);
    
private:
    std::unique_ptr<ota::OTAService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
    
    bool isConnected();
};

} // namespace ota

#endif // OTA_CLIENT_H