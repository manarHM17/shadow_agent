// server/ota_server.h
#ifndef OTA_SERVER_H
#define OTA_SERVER_H

#include <grpcpp/grpcpp.h>
#include "ota_service.grpc.pb.h"
#include "database_manager.h"
#include "file_manager.h"
#include <memory>
using namespace std ;
using namespace grpc ;
using namespace ota ;

namespace ota {

class OTAServiceImpl final : public OTAService::Service {
public:
    OTAServiceImpl();
    ~OTAServiceImpl();
    
    bool initialize(const string& db_host, const string& db_user,
                   const string& db_password, const string& db_name);

    // Implémentation des services gRPC
    Status CheckUpdate(ServerContext* context,
                           const UpdateRequest* request,
                           UpdateResponse* response) override;
    
    Status DownloadUpdate(ServerContext* context,
                              const DownloadRequest* request,
                              ServerWriter<FileChunk>* writer) override;
    
    Status ConfirmInstallation(ServerContext* context,
                                   const InstallationRequest* request,
                                   InstallationResponse* response) override;
    
    Status GetDeviceStatus(ServerContext* context,
                               const DeviceStatusRequest* request,
                               DeviceStatusResponse* response) override;
    
    Status RegisterDevice(ServerContext* context,
                              const DeviceRegistration* request,
                              RegistrationResponse* response) override;

private:
    unique_ptr<DatabaseManager> db_manager_;
    unique_ptr<FileManager> file_manager_;
    bool initialized_;
    
    // Méthodes utilitaires
    bool needsUpdate(const string& current_version, const string& latest_version);
    string generateUpdateId();
};

} // namespace ota

#endif // OTA_SERVER_H