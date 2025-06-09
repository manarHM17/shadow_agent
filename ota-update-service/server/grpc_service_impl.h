#pragma once
#include "ota_update_service.h"
#include "ota_service.grpc.pb.h"
#include <memory>

class OTAUpdateServiceImpl final : public ota::OTAUpdateService::Service {
public:
    explicit OTAUpdateServiceImpl(std::unique_ptr<OTAUpdateService> service);
    grpc::Status CheckForUpdates(grpc::ServerContext* context,
                                 const ota::CheckUpdatesRequest* request,
                                 ota::CheckUpdatesResponse* response) override;
    grpc::Status DownloadUpdate(grpc::ServerContext* context,
                                const ota::DownloadRequest* request,
                                grpc::ServerWriter<ota::DownloadResponse>* writer) override;
    grpc::Status ReportStatus(grpc::ServerContext* context,
                              const ota::StatusReport* request,
                              ota::StatusResponse* response) override;
private:
    std::unique_ptr<OTAUpdateService> ota_service;
};