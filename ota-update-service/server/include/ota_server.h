#ifndef OTA_SERVER_H
#define OTA_SERVER_H

#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "ota_service.grpc.pb.h"
#include "firmware_manager.h"
#include "db_connector.h"

namespace ota {

class OtaServiceImpl final : public OtaService::Service {
public:
    OtaServiceImpl(std::shared_ptr<FirmwareManager> firmware_manager, 
                  std::shared_ptr<DbConnector> db_connector);
    
    grpc::Status CheckForUpdate(grpc::ServerContext* context,
                               const CheckForUpdateRequest* request,
                               CheckForUpdateResponse* response) override;
    
    grpc::Status DownloadFirmware(grpc::ServerContext* context,
                                 const DownloadFirmwareRequest* request,
                                 grpc::ServerWriter<FirmwareChunk>* writer) override;
    
    grpc::Status ReportUpdateStatus(grpc::ServerContext* context,
                                   const UpdateStatusRequest* request,
                                   UpdateStatusResponse* response) override;
    
private:
    std::shared_ptr<FirmwareManager> firmware_manager_;
    std::shared_ptr<DbConnector> db_connector_;
};

class OtaServer {
public:
    OtaServer(const std::string& server_address, 
              std::shared_ptr<FirmwareManager> firmware_manager,
              std::shared_ptr<DbConnector> db_connector);
    
    void Start();
    void Stop();

private:
    std::unique_ptr<grpc::Server> server_;
    std::shared_ptr<FirmwareManager> firmware_manager_;
    std::shared_ptr<DbConnector> db_connector_;
};

} // namespace ota

#endif // OTA_SERVER_H