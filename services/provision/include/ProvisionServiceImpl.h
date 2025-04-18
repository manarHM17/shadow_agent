#pragma once

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "../../db/db_handler.h"
#include "../../jwt-handler/jwt-handler.hpp"
#include "provision.grpc.pb.h"

using namespace std;
using namespace grpc;

class ProvisionServiceImpl final : public shadow_agent::ProvisionService::Service {
private:
    unique_ptr<DBHandler> db;
    string getCurrentTimestamp();

public:
    ProvisionServiceImpl();

    // Méthodes RPC
    Status RegisterDevice(ServerContext* context, 
                          const shadow_agent::DeviceInfo* request, 
                          shadow_agent::RegisterDeviceResponse* response) override;

    Status DeleteDevice(ServerContext* context, 
                        const shadow_agent::DeviceId* request, 
                        shadow_agent::Response* response) override;

    Status UpdateDevice(ServerContext* context, 
                        const shadow_agent::UpdateDeviceRequest* request, 
                        shadow_agent::Response* response) override;

    Status ListDevices(ServerContext* context, 
                       const shadow_agent::ListDeviceRequest* request, 
                       shadow_agent::DeviceList* response) override;

    Status GetDevice(ServerContext* context, 
                     const shadow_agent::DeviceId* request, 
                     shadow_agent::DeviceInfo* response) override;
};
