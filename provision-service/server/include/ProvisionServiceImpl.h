#pragma once

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>

#include "../../common/include/db_handler.h"
#include "../../common/include/jwt_handler.h"
#include "provisioning.grpc.pb.h"

using namespace std ;
class ProvisioningServiceImpl final : public provisioning::ProvisioningService::Service {
public:
    ProvisioningServiceImpl(shared_ptr<DBHandler> db_manager,
                           shared_ptr<JWTUtils> jwt_manager);
    
    grpc::Status Authenticate(grpc::ServerContext* context,
                            const provisioning::AuthRequest* request,
                            provisioning::AuthResponse* response) override;
    
    grpc::Status AddDevice(grpc::ServerContext* context,
                          const provisioning::AddDeviceRequest* request,
                          provisioning::AddDeviceResponse* response) override;
    
    grpc::Status DeleteDevice(grpc::ServerContext* context,
                             const provisioning::DeleteDeviceRequest* request,
                             provisioning::DeleteDeviceResponse* response) override;
    
    grpc::Status UpdateDevice(grpc::ServerContext* context,
                             const provisioning::UpdateDeviceRequest* request,
                             provisioning::UpdateDeviceResponse* response) override;
    
    grpc::Status GetAllDevices(grpc::ServerContext* context,
                              const provisioning::GetDevicesRequest* request,
                              provisioning::GetDevicesResponse* response) override;
    
    grpc::Status GetDeviceById(grpc::ServerContext* context,
                              const provisioning::GetDeviceByIdRequest* request,
                              provisioning::GetDeviceByIdResponse* response) override;

private:
    shared_ptr<DBHandler> db_manager_;
    shared_ptr<JWTUtils> jwt_manager_;
    
    bool validateJWTFromContext(grpc::ServerContext* context);
};