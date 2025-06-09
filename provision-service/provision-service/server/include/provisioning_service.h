#pragma once

#include <grpcpp/grpcpp.h>
#include "provisioning.grpc.pb.h"
#include "database_manager.h"
#include "jwt_manager.h"
#include <memory>

class ProvisioningServiceImpl final : public provisioning::ProvisioningService::Service {
public:
    ProvisioningServiceImpl(std::shared_ptr<DatabaseManager> db_manager,
                           std::shared_ptr<JWTManager> jwt_manager);
    
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
    std::shared_ptr<DatabaseManager> db_manager_;
    std::shared_ptr<JWTManager> jwt_manager_;
    
    bool validateJWTFromContext(grpc::ServerContext* context);
    std::string getClientIP(grpc::ServerContext* context);
};