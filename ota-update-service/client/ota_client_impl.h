#pragma once
#include "ota_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

class SimpleOTAClient {
public:
    SimpleOTAClient(const std::string& server_address, int32_t user_device_id);
    void CheckAndApplyUpdates();

private:
    std::unique_ptr<ota::OTAUpdateService::Stub> stub;
    int32_t device_id;

    bool DownloadAndApplyUpdate(const ota::UpdateInfo& update);
    bool ApplyUpdate(const ota::UpdateInfo& update, const std::vector<char>& data);
    void ReportStatus(const std::string& app_name, const std::string& status_str, const std::string& error_msg);
    std::string CalculateChecksum(const std::vector<char>& data);
};