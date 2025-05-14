#pragma once

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>

#include "db_handler.hpp"
#include "jwt_handler.hpp"
#include "provision.grpc.pb.h"

class ProvisionServiceImpl final : public shadow_agent::ProvisionService::Service {
private:
    std::unique_ptr<DBHandler> db;

    std::string getCurrentTimestamp();

public:
    ProvisionServiceImpl();

    // RPC Methods
    grpc::Status RegisterDevice(grpc::ServerContext* context, 
                                 const shadow_agent::DeviceInfo* request, 
                                 shadow_agent::RegisterDeviceResponse* response) override;

    grpc::Status DeleteDevice(grpc::ServerContext* context, 
                               const shadow_agent::DeviceId* request, 
                               shadow_agent::Response* response) override;

    grpc::Status UpdateDevice(grpc::ServerContext* context, 
                               const shadow_agent::UpdateDeviceRequest* request, 
                               shadow_agent::Response* response) override;

    grpc::Status ListDevices(grpc::ServerContext* context, 
                              const shadow_agent::ListDeviceRequest* request, 
                              shadow_agent::DeviceList* response) override;

    grpc::Status GetDevice(grpc::ServerContext* context, 
                            const shadow_agent::DeviceId* request, 
                            shadow_agent::DeviceInfo* response) override;
};
