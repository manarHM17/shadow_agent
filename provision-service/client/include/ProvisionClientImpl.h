#pragma once

#include <grpcpp/grpcpp.h>
#include "provisioning.grpc.pb.h"
#include "ConfigManager.h"
#include <memory>
#include <string>

class ProvisioningClient {
public:
    ProvisioningClient(std::shared_ptr<grpc::Channel> channel);
    
    bool Authenticate(const std::string& hostname, const std::string& password, std::string& jwt_token);
    
    bool AddDevice(const std::string& hostname, const std::string& password,
                  const std::string& user, const std::string& location,
                  const std::string& hardware_type, const std::string& os_type,
                  int& device_id, std::string& jwt_token);
    
    bool UpdateDevice(int device_id, const std::string& user, 
                     const std::string& location,
                     const std::string& hardware_type, 
                     const std::string& os_type);
    
    bool DeleteDevice(int device_id);
    void GetAllDevices();
    bool GetDeviceById(int device_id);

private:
    std::unique_ptr<provisioning::ProvisioningService::Stub> stub_;
    std::string jwt_token_;
    std::unique_ptr<grpc::ClientContext> createContextWithAuth();
};