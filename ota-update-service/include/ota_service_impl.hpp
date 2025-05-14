#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include "ota_service.grpc.pb.h"
#include "mender_client.hpp"

class OTAServiceImpl final : public ota::OTAService::Service {
public:
    OTAServiceImpl(std::shared_ptr<MenderClient> mender_client);
    ~OTAServiceImpl() = default;

    grpc::Status CheckForUpdate(grpc::ServerContext* context,
                              const ota::CheckUpdateRequest* request,
                              ota::CheckUpdateResponse* response) override;

    grpc::Status DownloadUpdate(grpc::ServerContext* context,
                              const ota::DownloadRequest* request,
                              grpc::ServerWriter<ota::DownloadChunk>* writer) override;

    grpc::Status InstallUpdate(grpc::ServerContext* context,
                             const ota::InstallRequest* request,
                             ota::InstallResponse* response) override;

    grpc::Status GetUpdateStatus(grpc::ServerContext* context,
                               const ota::StatusRequest* request,
                               ota::StatusResponse* response) override;

private:
    std::shared_ptr<MenderClient> m_mender_client;
    
    // Structure pour stocker l'état des mises à jour
    struct UpdateStatus {
        ota::StatusResponse::Status status;
        int progress;
        std::string message;
        std::string version;
    };
    
    std::unordered_map<std::string, UpdateStatus> m_update_statuses;
    std::mutex m_status_mutex;
    
    bool validateToken(const std::string& jwt_token);
    bool verifyDeviceId(int32_t device_id);
    bool installFirmware(const std::string& update_id, int32_t device_id);
    void updateStatus(const std::string& installation_id, 
                     ota::StatusResponse::Status status,
                     int progress, 
                     const std::string& message);
};